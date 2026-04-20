//SSHConnection.cpp（SSH 连接具体实现）

#include "SSHConnection.h"

// 连接信息结构体（每个面板一套）
struct SSHConnInfo {
    LIBSSH2_SESSION* session = nullptr;
    SOCKET sock = INVALID_SOCKET;
    bool connected = false;
    char* host = nullptr;
    char* user = nullptr;
    char* pass = nullptr;
    int port = 22;
};
// 全局存储：每个面板独立的连接资源（索引对应面板）
static std::unordered_map<int, SSHConnInfo> s_panelConnections;
static int s_lastPanelIndex = -1;


// 异步连接防重入锁
static std::timed_mutex s_connectMutex; // 支持超时锁操作
std::mutex s_errorMsgMutex;
std::string s_lastConnectError; // 存储最后一次连接错误信息
//std::mutex s_threadResourceMutex; // 线程资源专用锁

// 全局变量专用锁
std::mutex s_globalVarMutex;
std::mutex s_threadMutex;
std::thread* s_pConnectThread = nullptr;   // 保存线程指针，用于取消

std::atomic<bool> s_connected(false);   // 原子化连接状态
std::atomic<bool> s_cancelConnect(false); // 保留原子变量
std::atomic<bool> s_connecting(false);
static bool s_connected_mirror = false;//普通bool镜像变量（用于返回引用）
static std::mutex s_connected_mirror_mutex;// 保护镜像变量的锁

// 连接超时配置（毫秒）
#define CONNECT_SOCKET_TIMEOUT 50    // Socket连接超时
#define SSH_HANDSHAKE_TIMEOUT  1000    // SSH握手超时
#define SSH_AUTH_TIMEOUT      1000    // 认证超时
const int MAIN_THREAD_WAIT_INTERVAL = 50; // 主线程轮询间隔（保持50ms）1000除以MAIN_THREAD_WAIT_INTERVAL=轮询次数
const int MAX_MAIN_THREAD_WAIT = 10000;   // 主线程最大等待3秒（原30秒）
//const UINT WM_SSH_CONNECT_CANCEL = WM_USER + 101; // 取消连接消息


// SSH连接全局状态实际定义
static LIBSSH2_SESSION* s_sshSession = nullptr;
static SOCKET s_sock = INVALID_SOCKET;

static char* ssh_host = _strdup("192.168.137.201"); // 改为char* 避免const导致free崩溃
static int ssh_port = 22;
static char* ssh_user = _strdup("root");             // 改为char*
static char* ssh_pass = _strdup("123456");           // 改为char*

static NppData s_nppData;
static HINSTANCE s_hInst;

static bool ValidatePort(int port) {
    return port > 0 && port <= 65535;
}
// 编码转换工具（自动识别 UTF8 / GBK，彻底解决Windows弹框乱码）
inline std::wstring GBKToWstring(const std::string& str) {
    if (str.empty())
        return L"";

    wchar_t buf[1024] = { 0 };

    // 1. 优先按 UTF-8 转换（libssh2 错误信息都是 UTF-8）
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len > 0 && len < 1024) {
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf, len);
        return buf;
    }

    // 2. 失败则使用 GBK（系统本地编码）
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buf, _countof(buf));
    return buf;
}

inline std::string GetLibssh2ErrorMsg(LIBSSH2_SESSION* session) {
    if (!session) return "无效的session";
    char* errmsg = nullptr;
    int code = libssh2_session_last_error(session, &errmsg, nullptr, 0);
    if (!errmsg) return "未知错误（错误码：" + std::to_string(code) + "）";
    return std::string(errmsg) + "（错误码：" + std::to_string(code) + "）";
}

// 全局状态获取接口
LIBSSH2_SESSION*& SSHConnection_GetSession() { return s_sshSession; }
SOCKET& SSHConnection_GetSocket() { return s_sock; }
bool& SSHConnection_GetConnectedState() {
    std::lock_guard<std::mutex> lock(s_connected_mirror_mutex);
    // 改用memory_order_seq_cst，确保全局可见性
    s_connected_mirror = s_connected.load(std::memory_order_seq_cst);
    return s_connected_mirror;
}
const char*& SSHConnection_GetHost() { return (const char*&)ssh_host; }
int& SSHConnection_GetPort() { return ssh_port; }
const char*& SSHConnection_GetUser() { return (const char*&)ssh_user; }
const char*& SSHConnection_GetPass() { return (const char*&)ssh_pass; }

// 工具函数：释放连接资源（抽离公共逻辑）统一释放 Socket、SSH 会话、libssh2、WSA 资源；
static void ReleaseConnectionResources(SOCKET sock, LIBSSH2_SESSION* session) {
    NppSSH_LogInfoAuto("开始释放SSH连接资源：sock=" + std::to_string((uintptr_t)sock) 
        + ", session=" + std::to_string((uintptr_t)session));

    // 先断开会话再释放
    if (session) {
        libssh2_session_disconnect(session, "Connection closed");
        libssh2_session_free(session);
    }
    // Socket关闭
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    if (!s_connected.load(std::memory_order_seq_cst)) {
        WSACleanup();
    }
    NppSSH_LogInfoAuto("SSH连接资源释放完成");
}
// 安全释放libssh2和WSA资源（避免重复释放）
static void SafeReleaseGlobalLibResources() {
    static std::once_flag s_libOnceFlag;
    std::call_once(s_libOnceFlag, []() {
        NppSSH_LogInfoAuto("全局Lib资源延迟到插件卸载时释放");
        });
}

// 核心连接函数
bool SSHConnection_Connect(const char* host, int port, const char* user, const char* pass) {
    s_nppData = g_nppData;

    // 1. 入参合法性校验
    if (!host || strlen(host) == 0 || !user || strlen(user) == 0 || !pass || strlen(pass) == 0) {
        std::wstring errMsg = L"主机/用户名/密码不能为空！";
        ::MessageBoxW(s_nppData._nppHandle, errMsg.c_str(), L"NppSSH 错误提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("连接失败：空参数，host=" + std::string(host ? host : "NULL"));
        return false;
    }
    if (!ValidatePort(port)) {
        std::wstring errMsg = L"端口无效！请输入1-65535之间的端口";
        ::MessageBoxW(s_nppData._nppHandle, errMsg.c_str(), L"NppSSH 错误提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("连接失败：无效端口，port=" + std::to_string(port));
        return false;
    }

    // 2. 防重复连接（原子判断+锁，仅做一次性检查）
    bool expected = false;
    if (!s_connecting.compare_exchange_strong(expected, true, std::memory_order_seq_cst)) {
        ::MessageBoxW(s_nppData._nppHandle, L"正在连接中，请等待...", L"NppSSH 提示", MB_OK | MB_ICONINFORMATION);
        NppSSH_LogInfoAuto("连接失败：已有连接任务在执行");
        return false;
    }

    // 3. 标记为连接中（原子变量，全局可见）
    s_connecting.store(true, std::memory_order_seq_cst);

    // 4. 连接前日志
    std::string connectInfo = "开始尝试连接SSH服务器，主机：" + std::string(host) + "，端口：" + std::to_string(port) + "，用户：" + std::string(user);
    NppSSH_LogInfoAuto(connectInfo);

    // 5. 初始化连接状态
    s_connected.store(false, std::memory_order_seq_cst);
    s_sshSession = nullptr;
    s_sock = INVALID_SOCKET;
    s_cancelConnect.store(false, std::memory_order_seq_cst);

    // 6. 拷贝参数（避免子线程指针失效）
    std::string strHost = host;
    std::string strUser = user;
    std::string strPass = pass;
    int nPort = port;
    HWND hNppMainWnd = s_nppData._nppHandle;

    // 7. 异步结果传递（promise/future）
    std::promise<bool> connectPromise;
    std::future<bool> connectFuture = connectPromise.get_future();
    std::string threadErrorMsg;

    // 8. 清理旧线程（避免残留）
    {
        std::lock_guard<std::mutex> threadLock(s_threadMutex);
        if (s_pConnectThread) {
            if (s_pConnectThread->joinable()) s_pConnectThread->join();
            delete s_pConnectThread;
            s_pConnectThread = nullptr;
        }
    }

    // 9. 启动子线程执行连接逻辑（修复lambda语法）
    s_pConnectThread = new std::thread([=, &connectPromise, &threadErrorMsg]() {
        NppSSH_LogInfoAuto("SSH启动子线程执行连接逻辑1==========用户：" + strUser);
        SOCKET sock = INVALID_SOCKET;
        LIBSSH2_SESSION * session = nullptr;
        bool connectResult = false;
        std::string errorMsg;
        bool wsaInited = false;

        try {
            NppSSH_LogInfoAuto("连接线程启动，开始初始化WSA");
            // 9.1 初始化WSA
            WSADATA wsaData;
            int wsaRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (wsaRet != 0) {
                errorMsg = "WSA初始化失败，错误码：" + std::to_string(wsaRet);
                NppSSH_LogErrorAuto(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            wsaInited = true;

            // 9.2 检测取消标记（提前退出）
            if (s_cancelConnect.load(std::memory_order_seq_cst)) {
                errorMsg = "用户取消连接（WSA初始化后）";
                throw std::runtime_error(errorMsg);
            }

            // 9.3 创建Socket并设置超时
            sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sock == INVALID_SOCKET) {
                int err = WSAGetLastError();
                errorMsg = "Socket创建失败，错误码：" + std::to_string(err);
                NppSSH_LogErrorAuto(errorMsg);
                throw std::runtime_error(errorMsg);
            }

            // 域名/IP 通用解析（支持 mky.3ds.com）
            addrinfo hints = { 0 };
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            addrinfo* result = nullptr;
            int ret = getaddrinfo(strHost.c_str(), std::to_string(nPort).c_str(), &hints, &result);
            if (ret != 0 || !result) {
                errorMsg = "域名/IP解析失败：" + strHost + " 错误码：" + std::to_string(ret);
                NppSSH_LogErrorAuto(errorMsg);
                closesocket(sock);
                throw std::runtime_error(errorMsg);
            }
            // 9.4 设置Socket为非阻塞模式（核心优化），实现严格50ms快速超时（Windows最稳方案）
            u_long nonblock = 1;
            ioctlsocket(sock, FIONBIO, &nonblock);

            // 开始连接
            //int connectRet = connect(sock, (sockaddr*)&addr, sizeof(addr));//阻塞模式（弃用，连接失败导程序崩溃）
            connect(sock, result->ai_addr, (int)result->ai_addrlen);//非阻塞模式
            freeaddrinfo(result); // 解析完立刻释放

            // select 严格超时 50ms，不等系统默认阻塞20秒，解决连接失败等待的20多秒
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(sock, &wfds);

            timeval tv = { 0 };
            tv.tv_sec = 0;
            tv.tv_usec = 50 * 1000; // 50ms 超时

            int select_ret = select(0, nullptr, &wfds, nullptr, &tv);
            if (select_ret <= 0) {
                errorMsg = "Socket连接超时（50ms）：" + strHost + ":" + std::to_string(nPort) + "（服务器无响应/防火墙拦截）";
                NppSSH_LogErrorAuto(errorMsg);
                closesocket(sock);
                throw std::runtime_error(errorMsg);
            }

            // 获取真实连接结果
            int err_code = 0;
            int len = sizeof(err_code);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err_code, &len);
            if (err_code != 0) {
                errorMsg = "Socket连接失败！错误码：" + std::to_string(err_code);
                if (err_code == 10061) errorMsg += "（连接被拒绝，端口未开放）";
                if (err_code == 10065) errorMsg += "（网络不可达）";
                NppSSH_LogErrorAuto(errorMsg);
                closesocket(sock);
                throw std::runtime_error(errorMsg);
            }

            // 恢复阻塞模式（给libssh2使用）
            nonblock = 0;
            ioctlsocket(sock, FIONBIO, &nonblock);
            NppSSH_LogInfoAuto("Socket连接成功：" + strHost + ":" + std::to_string(nPort));
  
            NppSSH_LogInfoAuto("结束连接Socket，主机：" + strHost + "，端口：" + std::to_string(nPort));
            // 9.5 初始化libssh2并创建会话
            if (libssh2_init(0) != 0) {
                errorMsg = "libssh2初始化失败";
                NppSSH_LogErrorAuto(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            NppSSH_LogInfoAuto("开始初始化session，主机：" + strHost + "，端口：" + std::to_string(nPort));
            session = libssh2_session_init();
            if (!session) {
                errorMsg = "libssh2_session_init失败";
                NppSSH_LogErrorAuto(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            NppSSH_LogInfoAuto("结束初始化session，sock=" + std::to_string((uintptr_t)sock)
                + ", session=" + std::to_string((uintptr_t)session));

            libssh2_session_set_blocking(session, 1);
            libssh2_session_set_timeout(session, SSH_HANDSHAKE_TIMEOUT);

            // 9.6 SSH握手
            NppSSH_LogInfoAuto("开始SSH握手：sock=" + std::to_string((uintptr_t)sock)
                + ", session=" + std::to_string((uintptr_t)session));
            if (libssh2_session_handshake(session, sock) != 0) {
                errorMsg = "SSH握手失败：" + GetLibssh2ErrorMsg(session);
                NppSSH_LogErrorAuto(errorMsg);
                throw std::runtime_error(errorMsg);
            }

            // 9.7 SSH密码认证
            NppSSH_LogInfoAuto("开始SSH密码认证：sock=" + std::to_string((uintptr_t)sock)
                + ", session=" + std::to_string((uintptr_t)session));
            libssh2_session_set_timeout(session, SSH_AUTH_TIMEOUT);
            int authRet = libssh2_userauth_password(session, strUser.c_str(), strPass.c_str());
            if (authRet != 0) {
                errorMsg = "SSH密码认证失败，用户：" + strUser + "，错误：" + GetLibssh2ErrorMsg(session);
                NppSSH_LogErrorAuto(errorMsg);
                throw std::runtime_error(errorMsg);
            }

            // 9.8 连接成功：更新全局状态（加锁保证线程安全）
            {
                NppSSH_LogInfoAuto("连接成功，更新全局状态：sock=" + std::to_string((uintptr_t)sock)
                    + ", session=" + std::to_string((uintptr_t)session));
                std::lock_guard<std::mutex> globalLock(s_globalVarMutex);
                // 释放旧参数内存
                if (ssh_host) { free(ssh_host); ssh_host = nullptr; }
                if (ssh_user) { free(ssh_user); ssh_user = nullptr; }
                if (ssh_pass) { free(ssh_pass); ssh_pass = nullptr; }
                // 保存新参数
                ssh_host = _strdup(strHost.c_str());
                ssh_port = nPort;
                ssh_user = _strdup(strUser.c_str());
                ssh_pass = _strdup(strPass.c_str());
                s_sock = sock;
                s_sshSession = session;
                // 更新原子状态
                s_connected.store(true, std::memory_order_seq_cst);
                std::lock_guard<std::mutex> mirrorLock(s_connected_mirror_mutex);
                s_connected_mirror = true;
            }
            connectResult = true;
            NppSSH_LogInfoAuto("SSH连接成功！用户：" + strUser);

        }
        catch (const std::exception& e) {
            // 异常处理：释放资源 + 重置状态
            errorMsg = "SSH连接异常：" + std::string(e.what());
            NppSSH_LogErrorAuto(errorMsg);
            ReleaseConnectionResources(sock, session);
            if (wsaInited) WSACleanup();
            // 重置全局状态
            std::lock_guard<std::timed_mutex> lock(s_connectMutex);
            s_connected.store(false, std::memory_order_seq_cst);
            s_sshSession = nullptr;
            s_sock = INVALID_SOCKET;
            std::lock_guard<std::mutex> mirrorLock(s_connected_mirror_mutex);
            s_connected_mirror = false;

        }
        catch (...) {
            // 兜底异常处理
            errorMsg = "SSH连接发生未知异常，用户：" + strUser;
            NppSSH_LogErrorAuto(errorMsg);
            ReleaseConnectionResources(sock, session);
            if (wsaInited) WSACleanup();
            std::lock_guard<std::timed_mutex> lock(s_connectMutex);
            s_connected.store(false, std::memory_order_seq_cst);
            s_sshSession = nullptr;
            s_sock = INVALID_SOCKET;
            std::lock_guard<std::mutex> mirrorLock(s_connected_mirror_mutex);
            s_connected_mirror = false;
        }

    // 最终状态重置（无论成功/失败）
    s_connecting.store(false, std::memory_order_seq_cst);
    s_cancelConnect.store(false, std::memory_order_seq_cst);
    NppSSH_LogInfoAuto("错误消息"+ errorMsg);
    threadErrorMsg = errorMsg;
    connectPromise.set_value(connectResult); // 传递结果到主线程

    // 投递连接结果消息到UI
    if (::IsWindow(hNppMainWnd)) {
        std::lock_guard<std::mutex> errLock(s_errorMsgMutex);
        s_lastConnectError = errorMsg;
        ::PostMessage(hNppMainWnd, WM_SSH_CONNECT_RESULT, (WPARAM)connectResult, 0);
    }
    //else if (!connectResult && !errorMsg.empty())
    //{
    //    std::wstring wMsg = GBKToWstring(errorMsg);
    //
    //    ::MessageBoxW(NULL, wMsg.c_str(), L"NppSSH 连接失败", MB_OK | MB_ICONERROR);
    //}
    });

    // 主线程消息循环等待（修复：超时必须等线程退出，否则NPP卡死）
    bool finalResult = false;
    auto start = std::chrono::steady_clock::now();
    bool isTimeout = false;

    while (true) {
            
        // 必须持续处理Windows消息，否则必卡死
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        // 线程正常结束
        auto status = connectFuture.wait_for(std::chrono::milliseconds(MAIN_THREAD_WAIT_INTERVAL));
        if (status == std::future_status::ready) {
            finalResult = connectFuture.get();
            NppSSH_LogInfoAuto("调试1==================" + strUser+"===="+std::to_string(finalResult));
            break;
        }
        NppSSH_LogInfoAuto("调试2==================" + strUser + "====" + std::to_string(finalResult));

        // 3秒超时：只标记取消，不退出循环
        auto now = std::chrono::steady_clock::now();
        auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (cost >=  MAX_MAIN_THREAD_WAIT && !isTimeout) {
            NppSSH_LogErrorAuto("SSH连接超时（3秒），直接返回失败，子线程后台安全退出");
            s_cancelConnect.store(true, std::memory_order_seq_cst);
            isTimeout = true;
            finalResult = false;
            //break;
        }
    }

    // 清理线程资源
    {
        std::lock_guard<std::mutex> threadLock(s_threadMutex);
        if (s_pConnectThread && s_pConnectThread->joinable()) {
            s_pConnectThread->join();
        }
        delete s_pConnectThread;
        s_pConnectThread = nullptr;
    }

    // 失败提示
    if (!finalResult) {
        std::string showMsg;

        if (isTimeout) {
            showMsg = "SSH连接超时（3秒），请检查IP/端口是否正确";
        }
        else if (!threadErrorMsg.empty()) {
            showMsg = threadErrorMsg;
        }
        else {
            showMsg = "SSH连接失败";
        }

        std::wstring wErr = GBKToWstring(showMsg);
        ::MessageBoxW(s_nppData._nppHandle, wErr.c_str(), L"NppSSH 连接失败", MB_OK | MB_ICONERROR);
    }

    NppSSH_LogInfoAuto("异步连接完成，最终结果：" + std::to_string(finalResult));
    return finalResult;
}
 
// 断开SSH连接
void SSHConnection_Disconnect() {
    std::lock_guard<std::timed_mutex> lock(s_connectMutex); // 加锁保证线程安全
    if (s_connected.load()) {
        // 格式化指针/句柄为字符串
        char sessionBuf[64] = { 0 };
        char sockBuf[64] = { 0 };
        sprintf_s(sessionBuf, _countof(sessionBuf), "0x%p", s_sshSession);
        sprintf_s(sockBuf, _countof(sockBuf), "%u", static_cast<unsigned int>(s_sock));
        //sprintf(sessionBuf, "0x%p", s_sshSession);
        //sprintf(sockBuf, "%u", static_cast<unsigned int>(s_sock));
        NppSSH_LogInfoAuto("断开SSH连接，释放资源，服务器主机：" + std::string(ssh_host) + 
            " 端口：" + std::to_string(ssh_port) +
            " 用户：" + std::string(ssh_user)+
            "sshSession："+std::string(sessionBuf) +
            "socket:"+std::string(sockBuf));
        // 释放动态分配的内存
        if (ssh_host) {free(ssh_host);ssh_host = nullptr;}
        if (ssh_user) {free(ssh_user);ssh_user = nullptr;}
        if (ssh_pass) {free(ssh_pass);ssh_pass = nullptr;}

        // 断开SSH会话
        if (s_sshSession != nullptr) {
            libssh2_session_disconnect(s_sshSession, "Panel closed manually");
            libssh2_session_free(s_sshSession);
            s_sshSession = nullptr;
        }
        // 关闭Socket
        if (s_sock != INVALID_SOCKET) {
            shutdown(s_sock, SD_BOTH);
            closesocket(s_sock);
            s_sock = INVALID_SOCKET;
        }
        // 更新状态
        s_connected.store(false);
        // 同步镜像
        std::lock_guard<std::mutex> mirrorLock(s_connected_mirror_mutex);
        s_connected_mirror = false;

        // 安全释放全局Lib资源（避免重复释放）
        SafeReleaseGlobalLibResources();
        NppSSH_LogInfoAuto("SSH连接已断开，资源释放完成");
    }
    else {
        NppSSH_LogInfoAuto("当前无活跃SSH连接，无需断开");
    }
}

// 判断是否连接
bool SSHConnection_IsConnected() {
    return s_connected.load();
}

// 重置连接状态（暂未使用）
void SSHConnection_ResetState() {
    std::lock_guard<std::timed_mutex> lock(s_connectMutex);

    if (ssh_host) { free(ssh_host); ssh_host = _strdup("36.33.27.234"); }
    if (ssh_user) { free(ssh_user);  ssh_user = _strdup(""); }
    if (ssh_pass) { free(ssh_pass);  ssh_pass = _strdup(""); }

    s_connected.store(false);
    // 同步镜像
    std::lock_guard<std::mutex> mirrorLock(s_connected_mirror_mutex);
    s_connected_mirror = false;
    s_sshSession = nullptr;
    s_sock = INVALID_SOCKET;
    ssh_port = 22;

    NppSSH_LogInfoAuto("SSH连接状态已重置");
}

// 连接成功后，把当前全局连接绑定到面板索引
void SSHConnection_BindPanelIndex(int panelIndex) {
    SSHConnInfo info;
    info.session = s_sshSession;
    info.sock = s_sock;
    info.connected = s_connected;
    info.host = _strdup(ssh_host);
    info.user = _strdup(ssh_user);
    info.pass = _strdup(ssh_pass);
    info.port = ssh_port;

    s_panelConnections[panelIndex] = info;
    s_lastPanelIndex = panelIndex;
}
// ---------------- 核心：按面板索引断开 ----------------
void SSHConnection_DisconnectByPanelIndex(int panelIndex) {
    auto it = s_panelConnections.find(panelIndex);
    if (it == s_panelConnections.end()) return;

    SSHConnInfo& info = it->second;
    if (!info.connected || !info.session || info.sock == INVALID_SOCKET)
        return;

    // 关闭当前面板的连接（服务器侧释放）
    libssh2_session_disconnect(info.session, "Panel Disconnect");
    libssh2_session_free(info.session);
    shutdown(info.sock, SD_BOTH);
    closesocket(info.sock);

    // 释放内存
    if (info.host) free(info.host);
    if (info.user) free(info.user);
    if (info.pass) free(info.pass);

    // 清空状态
    info.session = nullptr;
    info.sock = INVALID_SOCKET;
    info.connected = false;

    // 如果断开的是最后一个面板，同步全局状态
    if (panelIndex == s_lastPanelIndex) {
        s_connected = false;
        s_sshSession = nullptr;
        s_sock = INVALID_SOCKET;
    }
}