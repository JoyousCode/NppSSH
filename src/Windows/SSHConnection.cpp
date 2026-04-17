//SSHConnection.cpp（SSH 连接具体实现）

#include "SSHConnection.h"
#include <tchar.h>
#include <mutex>
#include <thread>
#include <stdexcept>
#include <string>
#include <future>


// 异步连接防重入锁
//static std::mutex s_connectMutex;
static std::timed_mutex s_connectMutex; // 支持超时锁操作
std::atomic<bool> s_connecting(false);
// 新增：普通bool镜像变量（用于返回引用）
static bool s_connected_mirror = false;
// 新增：保护镜像变量的锁
static std::mutex s_connected_mirror_mutex;

std::atomic<bool> s_connected(false);   // 新增：原子化连接状态
std::atomic<bool> s_cancelConnect(false); // 保留原子变量
std::mutex s_errorMsgMutex;
std::string s_lastConnectError; // 存储最后一次连接错误信息
std::mutex s_threadResourceMutex; // 线程资源专用锁
// 新增全局变量专用锁（在文件顶部定义）
std::mutex s_globalVarMutex;
std::mutex s_threadMutex;
std::thread* s_pConnectThread = nullptr;   // 保存线程指针，用于取消

// 连接超时配置（毫秒）
#define CONNECT_SOCKET_TIMEOUT 50    // Socket连接超时
#define SSH_HANDSHAKE_TIMEOUT  1000    // SSH握手超时
#define SSH_AUTH_TIMEOUT      1000    // 认证超时
const int MAIN_THREAD_WAIT_INTERVAL = 50; // 主线程轮询间隔（保持50ms）1000除以MAIN_THREAD_WAIT_INTERVAL=轮询次数
const int MAX_MAIN_THREAD_WAIT = 10000;   // 主线程最大等待3秒（原30秒）
const UINT WM_SSH_CONNECT_CANCEL = WM_USER + 101; // 取消连接消息
// 全局变量新增（需线程安全）
//std::atomic<bool> s_cancelConnect(false); // 取消连接标记



// SSH连接全局状态实际定义
static LIBSSH2_SESSION* s_sshSession = nullptr;
static SOCKET s_sock = INVALID_SOCKET;
//static bool s_connected = false;

static char* ssh_host = _strdup("192.168.137.201"); // 改为char* 避免const导致free崩溃
static int s_port = 22;
static char* s_user = _strdup("root");             // 改为char*
static char* s_pass = _strdup("123456");           // 改为char*

static NppData s_nppData;
static HINSTANCE s_hInst;
// ========== IP/端口合法性校验工具函数（主线程前置校验） ==========
static bool ValidateIPAddress(const char* ip) {
    if (!ip) return false;
    sockaddr_in sa;
    return inet_pton(AF_INET, ip, &sa.sin_addr) == 1;
}

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
int& SSHConnection_GetPort() { return s_port; }
const char*& SSHConnection_GetUser() { return (const char*&)s_user; }
const char*& SSHConnection_GetPass() { return (const char*&)s_pass; }

// 工具函数：释放连接资源（抽离公共逻辑）统一释放 Socket、SSH 会话、libssh2、WSA 资源；
static void ReleaseConnectionResources(SOCKET sock, LIBSSH2_SESSION* session) {
    //NppSSH_LogInfoAuto("开始释放SSH连接资源：session="+std::to_string(sock));
    NppSSH_LogInfoAuto("开始释放SSH连接资源：sock=" + std::to_string((uintptr_t)sock) 
        + ", session=" + std::to_string((uintptr_t)session));

    // 先断开会话再释放
    if (session) {
        libssh2_session_disconnect(session, "Connection closed");
        libssh2_session_free(session);
        //session = nullptr;
    }
    // Socket关闭顺序
    if (sock != INVALID_SOCKET) {
        //shutdown(sock, SD_BOTH); // 先关闭读写
        closesocket(sock);
        //sock = INVALID_SOCKET;
    }
    // 3. 仅在连接失败时清理WSA/libssh2（连接成功时由Disconnect函数清理）
    if (!s_connected.load(std::memory_order_seq_cst)) {
        WSACleanup();
        // libssh2_exit(); // 全局退出仅在插件卸载时执行
    }
    // libssh2和WSA清理顺序（避免崩溃）
    //libssh2_exit();
    //WSACleanup();

    NppSSH_LogInfoAuto("SSH连接资源释放完成");
}
// 新增：安全释放libssh2和WSA资源（避免重复释放）
static void SafeReleaseGlobalLibResources() {
    static std::once_flag s_libOnceFlag;
    std::call_once(s_libOnceFlag, []() {
        // 只在插件卸载时调用，连接过程不清理
        //libssh2_exit();
        //WSACleanup();
        NppSSH_LogInfoAuto("全局Lib资源延迟到插件卸载时释放");
        });
}
// 取消连接的处理函数（供UI调用）
void SSHConnection_CancelConnect() {

    std::lock_guard<std::timed_mutex> lock(s_connectMutex); // 改为timed_mutex
    if (s_connecting.load()) {
        s_cancelConnect.store(true);
        NppSSH_LogInfoAuto("用户取消SSH连接");
        if (s_pConnectThread && s_pConnectThread->joinable()) {
            // 线程内检测s_cancelConnect自终止
        }
    }
}
// 核心连接函数（完全重构时序逻辑）
bool SSHConnection_Connect(const char* host, int port, const char* user, const char* pass) {
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
            // 设置Socket为非阻塞模式（核心优化）
            // 2. 非阻塞connect，实现严格50ms快速超时（Windows最稳方案）
            u_long nonblock = 1;
            ioctlsocket(sock, FIONBIO, &nonblock);

            // 开始连接
            connect(sock, result->ai_addr, (int)result->ai_addrlen);
            freeaddrinfo(result); // 解析完立刻释放

            // 3. select 严格超时 50ms，不等系统默认20秒
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

            // 4. 获取真实连接结果
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

            // 5. 恢复阻塞模式（给libssh2使用）
            nonblock = 0;
            ioctlsocket(sock, FIONBIO, &nonblock);
            NppSSH_LogInfoAuto("Socket连接成功：" + strHost + ":" + std::to_string(nPort));
            //DWORD timeout = CONNECT_SOCKET_TIMEOUT;
            //setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
            //setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            //// 额外设置连接超时（Windows特有，确保connect超时生效）
            //DWORD connectTimeout = CONNECT_SOCKET_TIMEOUT;
            //setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&connectTimeout, sizeof(connectTimeout));

            // 9.4 连接服务器
            //sockaddr_in addr{};
            //addr.sin_family = AF_INET;
            //addr.sin_port = htons(nPort);
            //if (inet_pton(AF_INET, strHost.c_str(), &addr.sin_addr) != 1) {
            //    int err = WSAGetLastError();
            //    errorMsg = "IP地址转换失败，主机：" + strHost + "，错误码：" + std::to_string(err);
            //    NppSSH_LogErrorAuto(errorMsg);
            //    throw std::runtime_error(errorMsg);
            //}
            //NppSSH_LogInfoAuto("开始连接Socket，主机：" + strHost + "，端口：" + std::to_string(nPort));
            //int connectRet = connect(sock, (sockaddr*)&addr, sizeof(addr));
            //if (connectRet == SOCKET_ERROR) {
            //    int err = WSAGetLastError();
            //    errorMsg = "SOCKET连接失败！错误码：" + std::to_string(err);
            //    // 错误码语义化提示（快速定位问题）
            //    if (err == 10060) errorMsg += "（连接超时/服务器无响应/防火墙拦截）";
            //    if (err == 10061) errorMsg += "（连接被拒绝/服务器端口未开放）";
            //    if (err == 10065) errorMsg += "（无路由到主机/网络不可达）";
            //    NppSSH_LogErrorAuto(errorMsg);
            //    throw std::runtime_error(errorMsg);
            //}
            
            
            
            
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
                if (s_user) { free(s_user); s_user = nullptr; }
                if (s_pass) { free(s_pass); s_pass = nullptr; }
                // 保存新参数
                ssh_host = _strdup(strHost.c_str());
                s_port = nPort;
                s_user = _strdup(strUser.c_str());
                s_pass = _strdup(strPass.c_str());
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

        // 10. 最终状态重置（无论成功/失败）
        s_connecting.store(false, std::memory_order_seq_cst);
        s_cancelConnect.store(false, std::memory_order_seq_cst);
        NppSSH_LogInfoAuto("错误消息"+ errorMsg);
        threadErrorMsg = errorMsg;
        connectPromise.set_value(connectResult); // 传递结果到主线程

        // 11. 投递连接结果消息到UI
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

        // 12. 主线程消息循环等待（修复：超时必须等线程退出，否则NPP卡死）
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
            if (!threadErrorMsg.empty()) {
                NppSSH_LogInfoAuto("调试4==================" + strUser + "====" + std::to_string(finalResult));
            }
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

    // 13. 清理线程资源
    {
        std::lock_guard<std::mutex> threadLock(s_threadMutex);
        if (s_pConnectThread && s_pConnectThread->joinable()) {
            s_pConnectThread->join();
        }
        delete s_pConnectThread;
        s_pConnectThread = nullptr;
    }

    // 14. 失败提示
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
// SSH连接具体实现    异步 + 超时 + 防重入
//bool SSHConnection_Connect(const char* host, int port, const char* user, const char* pass) {
//    NppSSH_LogInfoAuto("s_connecting的状态1========================" + std::to_string(s_connecting.load()));
//    // 1. 入参合法性校验
//    if (!host || !user || !pass || port <= 0 || port > 65535) {
//        ::MessageBoxW(s_nppData._nppHandle, L"无效的连接参数！主机/用户/密码不能为空，端口需在1-65535之间", L"NppSSH 错误提示", MB_OK | MB_ICONERROR);
//        NppSSH_LogInfoAuto("连接失败：无效参数 host=" + std::string(host ? host : "NULL") + ", port=" + std::to_string(port) + ", user=" + std::string(user ? user : "NULL"));
//        return false;
//    }
//    // 防重复点击（关键：防止多次点击卡顿）
//    //std::lock_guard<std::timed_mutex> lock(s_connectMutex);
//    // 【修复】防重入锁：只判断，不长期持有！启动线程后立即释放
//    //std::unique_lock<std::timed_mutex> lock(s_connectMutex, std::defer_lock);
//    //if (!lock.try_lock_for(std::chrono::milliseconds(100))) {
//    //    ::MessageBoxW(s_nppData._nppHandle, L"正在连接中，请等待...", L"NppSSH 提示", MB_OK | MB_ICONINFORMATION);
//    //    NppSSH_LogInfoAuto("连接失败：已有连接任务在执行");
//    //    return false;
//    //}
//
//    //// 【关键】确认未连接后，马上解锁 → 避免死锁
//    //lock.unlock();
//    bool isConnecting = false;
//    {
//        std::lock_guard<std::timed_mutex> lock(s_connectMutex);
//        isConnecting = s_connecting.load();
//    }
//    if (isConnecting) {
//        ::MessageBoxW(s_nppData._nppHandle, L"正在连接中，解锁请等待...", L"NppSSH 提示", MB_OK | MB_ICONINFORMATION);
//        return false;
//    }
//    // 标记为连接中（原子变量，无需锁）
//    s_connecting.store(true);
//
//    //if (s_connecting) {
//    //    ::MessageBoxW(s_nppData._nppHandle, L"正在连接中，请等待...", L"NppSSH 提示", MB_OK | MB_ICONINFORMATION);
//    //    NppSSH_LogInfoAuto("连接失败：已有连接任务在执行");
//    //    return false;
//    //}
//
//    // 连接前打印Info日志（手动指定事件名）
//    std::string connectInfo = "开始尝试连接SSH服务器，主机：" + std::string(host) + "，端口：" + std::to_string(port) + "，用户：" + std::string(user);
//    NppSSH_LogInfoAuto(connectInfo);
//
//    wchar_t paramMsg[1024] = { 0 }; // 加大缓冲区并初始化
//    swprintf_s(paramMsg, _countof(paramMsg), L"是否确认连接？\n\nSSH连接参数：\n主机 = %hs\n端口 = %d\n用户 = %hs\n密码 = %hs",
//        host, port, user, pass);
//    // 强制激活NPP主窗口，确保系统认定NPP为活跃窗口（关键！）
//    ::SetForegroundWindow(s_nppData._nppHandle);
//    ::SetActiveWindow(s_nppData._nppHandle);
//    ::SetWindowPos(s_nppData._nppHandle, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
//
//    // 禁用NPP主窗口，防止操作（系统标准模态第一步）
//    //::EnableWindow(s_nppData._nppHandle, FALSE);
//    int ret = ::MessageBoxW(                        //弹框确认取消提示框
//        s_nppData._nppHandle, // 登录对话框自身作为父窗口，而非NPP主窗口
//        paramMsg,
//        L"NppSSH 连接提示",
//        MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL // 置顶+模态// 系统模态，强制锁定
//    );
//
//    if (ret == IDNO) {
//        return false;
//    }
//
//
//    s_connecting.store(true);
//    s_connected.store(false);
//    s_sshSession = nullptr;
//    s_sock = INVALID_SOCKET;
//    s_cancelConnect.store(false);
//
//    // 新增：保存当前面板窗口句柄（用于消息投递）
//    //HWND hPanelWnd = nullptr;
//    //if (!s_sshPanels.empty()) {
//    //    hPanelWnd = s_sshPanels.back()->getHSelf(); // 获取当前操作的面板句柄
//    //}
//    // 保存NPP主窗口句柄的副本（避免线程中s_nppData被销毁）
//    HWND hNppMainWnd = s_nppData._nppHandle;
//    // 拷贝连接参数（避免原指针失效导致线程中参数丢失）
//    std::string strHost = host;
//    std::string strUser = user;
//    std::string strPass = pass;
//    int nPort = port;
//
//    // 通过promise/future获取异步线程的执行结果
//    std::promise<bool> connectPromise;
//    std::future<bool> connectFuture = connectPromise.get_future();
//    std::string threadErrorMsg; // 线程错误信息
//    // 启动异步连接线程
//    {
//        std::lock_guard<std::mutex> threadLock(s_threadMutex);
//        s_pConnectThread = new std::thread([=, &connectPromise, &threadErrorMsg]() {
//            SOCKET sock = INVALID_SOCKET;
//            LIBSSH2_SESSION* session = nullptr;
//            bool connectResult = false;
//            std::string errorMsg; // 保存错误信息
//            bool wsaInited = false; // 标记WSA是否初始化成功
//
//            try {
//                NppSSH_LogInfoAuto("连接线程启动，开始初始化WSA");
//                //开始连接操作   初始化WSA  创建Socket
//                WSADATA wsaData;
//                int wsaRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
//                if (wsaRet != 0) {
//                    errorMsg = "WSA初始化失败，错误码：" + std::to_string(wsaRet);
//                    NppSSH_LogErrorAuto(errorMsg);
//                    throw std::runtime_error(errorMsg);
//                }
//                wsaInited = true; // 标记WSA初始化成功
//                // 检测取消标记
//                if (s_cancelConnect.load()) {
//                    errorMsg = "用户取消连接（WSA初始化后）";
//                    throw std::runtime_error(errorMsg);
//                }
//
//                sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//                if (sock == INVALID_SOCKET) {
//                    int err = WSAGetLastError();
//                    errorMsg = "Socket创建失败，错误码：" + std::to_string(err);
//                    NppSSH_LogErrorAuto(errorMsg);
//                    //WSACleanup();
//                    throw std::runtime_error(errorMsg);
//                }
//
//                // ===================== Socket设置超时（主动快速失败） =====================
//                DWORD timeout = CONNECT_SOCKET_TIMEOUT;
//                setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
//                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
//                // 检测取消标记
//                if (s_cancelConnect.load()) {
//                    errorMsg = "用户取消连接（Socket创建后）";
//                    throw std::runtime_error(errorMsg);
//                }
//
//                // 配置服务器地址
//                sockaddr_in addr{};
//                addr.sin_family = AF_INET;
//                addr.sin_port = htons(nPort);
//                NppSSH_LogInfoAuto("端口(网络序)：" + std::to_string(addr.sin_port) + "，端口(真实端口)：" + std::to_string(ntohs(addr.sin_port)));
//
//                // IP地址转换校验
//                if (inet_pton(AF_INET, strHost.c_str(), &addr.sin_addr) != 1) {
//                    int err = WSAGetLastError();
//                    errorMsg = "IP地址转换失败，主机：" + strHost + "，错误码：" + std::to_string(err);
//                    NppSSH_LogErrorAuto(errorMsg);
//                    throw std::runtime_error(errorMsg);
//                }
//                // 检测取消标记
//                if (s_cancelConnect.load()) {
//                    errorMsg = "用户取消连接（IP转换后）";
//                    throw std::runtime_error(errorMsg);
//                }
//
//                // 连接服务器（超时由setsockopt控制）
//                NppSSH_LogInfoAuto("开始连接Socket，主机：" + std::string(strHost) + "，端口：" + std::to_string(nPort));
//                int connectRet = connect(sock, (sockaddr*)&addr, sizeof(addr));
//                if (connectRet == SOCKET_ERROR) {
//                    int err = WSAGetLastError(); // 获取具体错误码
//                    errorMsg = "SOCKET连接失败！用户：" + strUser + "，错误码：" + std::to_string(err);
//                    NppSSH_LogErrorAuto(errorMsg);
//                    NppSSH_LogWarnAuto("10060：连接超时::服务器无响应-防火墙拦截 10061：连接被拒绝::服务器端口未开放 10065：无路由到主机::网络不可达");
//                    //ReleaseConnectionResources(sock, nullptr);
//                    //return false;
//                    throw std::runtime_error(errorMsg);
//                }
//                // 检测取消标记
//                if (s_cancelConnect.load()) {
//                    errorMsg = "用户取消连接（Socket连接后）";
//                    throw std::runtime_error(errorMsg);
//                }
//                NppSSH_LogInfoAuto("Socket连接成功，开始初始化libssh2");
//
//                // 初始化libssh2
//                int libssh2Ret = libssh2_init(0);
//                if (libssh2Ret != 0) {
//                    errorMsg = "libssh2初始化失败，返回码：" + std::to_string(libssh2Ret);
//                    NppSSH_LogErrorAuto(errorMsg);
//                    throw std::runtime_error(errorMsg);
//                }
//
//                // 创建SSH会话
//                //LIBSSH2_SESSION* session = libssh2_session_init();
//                session = libssh2_session_init();
//                if (!session) {
//                    errorMsg = "libssh2_session_init失败！用户：" + strUser;
//                    NppSSH_LogErrorAuto(errorMsg);
//                    throw std::runtime_error(errorMsg);
//                }
//
//                // SSH握手
//                libssh2_session_set_blocking(session, 1);
//                libssh2_session_set_timeout(session, SSH_HANDSHAKE_TIMEOUT); // 握手
//                // 检测取消标记
//                if (s_cancelConnect.load()) {
//                    errorMsg = "用户取消连接（会话创建后）";
//                    throw std::runtime_error(errorMsg);
//                }
//                if (libssh2_session_handshake(session, sock) != 0)
//                {
//                    errorMsg = "SSH握手失败！用户：" + strUser;
//                    NppSSH_LogErrorAuto(errorMsg);
//                    throw std::runtime_error(errorMsg);
//                }
//                NppSSH_LogInfoAuto("SSH握手成功，开始密码认证");
//                // 检测取消标记
//                if (s_cancelConnect.load()) {
//                    errorMsg = "用户取消连接（SSH握手后）";
//                    throw std::runtime_error(errorMsg);
//                }
//
//                // 密码认证（超时SSH_AUTH_TIMEOUT秒）
//                libssh2_session_set_timeout(session, SSH_AUTH_TIMEOUT); 
//                if (libssh2_userauth_password(session, _strdup(strUser.c_str()), _strdup(strPass.c_str())))
//                {
//                    libssh2_session_disconnect(session, "Auth Fail");
//                    // 错误日志  异常场景示例（比如认证失败）
//                    errorMsg = "SSH密码认证失败，用户：" + strUser;
//                    NppSSH_LogErrorAuto(errorMsg);
//                    throw std::runtime_error(errorMsg);
//                }
//                
//                // 检测取消标记
//                if (s_cancelConnect.load()) {
//                    errorMsg = "用户取消连接（认证成功前）";
//                    throw std::runtime_error(errorMsg);
//                }
//                NppSSH_LogInfoAuto("SSH密码认证后s_connecting的状态1========================" + std::to_string(s_connecting.load()));
//
//                // 连接成功，更新全局状态（需加锁，避免线程竞争）
//                {
//                    std::lock_guard<std::mutex> globalLock(s_globalVarMutex);
//                    NppSSH_LogInfoAuto("线程竞争前s_connecting的状态========================" + std::to_string(s_connecting.load()));
//                    //std::lock_guard<std::mutex> lock(s_connectMutex);
//                    // 内存释放顺序（先释放旧内存，再分配新内存）
//                    if (ssh_host) { free(ssh_host); ssh_host = nullptr; }
//                    if (s_user) { free(s_user); s_user = nullptr; }
//                    if (s_pass) { free(s_pass); s_pass = nullptr; }
//
//                    ssh_host = _strdup(strHost.c_str());
//                    s_port = nPort;
//                    s_user = _strdup(strUser.c_str());
//                    s_pass = _strdup(strPass.c_str());
//                    s_sock = sock;
//                    s_sshSession = session;
//                    s_connected.store(true);
//                    std::lock_guard<std::mutex> mirrorLock(s_connected_mirror_mutex);
//                    s_connected_mirror = true;
//                    NppSSH_LogInfoAuto("线程竞争后s_connecting的状态========================" + std::to_string(s_connecting.load()));
//                }
//                NppSSH_LogInfoAuto("SSH密码认证后s_connecting的状态2========================" + std::to_string(s_connecting.load()));
//                connectResult = true;
//                NppSSH_LogInfoAuto("SSH连接成功！用户：" + strUser);
//
//            }
//            catch (const std::exception& e) {
//                // 捕获异常，打印日志
//                errorMsg = "SSH连接异常：" + std::string(e.what());
//                NppSSH_LogErrorAuto(errorMsg);
//
//                ReleaseConnectionResources(sock, session);
//                // 异常时清理WSA
//                if (wsaInited) {
//                    WSACleanup();
//                    wsaInited = false;
//                }
//
//                // 异常时强制重置全局状态为未连接
//                //std::lock_guard<std::mutex> lock(s_connectMutex);
//                std::lock_guard<std::timed_mutex> lock(s_connectMutex);
//                s_connected.store(false);
//                s_sshSession = nullptr;
//                s_sock = INVALID_SOCKET;
//                // 同步镜像
//                std::lock_guard<std::mutex> mirrorLock(s_connected_mirror_mutex);
//                s_connected_mirror = false;
//                s_sshSession = nullptr;
//                s_sock = INVALID_SOCKET;
//            }
//            catch (...) {
//                // 捕获所有未处理异常，防止线程崩溃
//                errorMsg = "SSH连接发生未知异常，用户：" + strUser;
//                NppSSH_LogErrorAuto(errorMsg);
//                ReleaseConnectionResources(sock, session);
//                // 异常时清理WSA
//                if (wsaInited) {
//                    WSACleanup();
//                    wsaInited = false;
//                }
//
//                // 异常时强制重置全局状态为未连接
//                std::lock_guard<std::timed_mutex> lock(s_connectMutex);
//                s_connected.store(false);
//                s_sshSession = nullptr;
//                s_sock = INVALID_SOCKET;
//                // 同步镜像
//                std::lock_guard<std::mutex> mirrorLock(s_connected_mirror_mutex);
//                s_connected_mirror = false;
//                s_sshSession = nullptr;
//                s_sock = INVALID_SOCKET;
//            }
//
//            // 【新增】兜底WSA清理（防止漏执行）
//            // 3. 仅在连接失败时清理WSA
//            if (wsaInited && !connectResult) {
//                NppSSH_LogInfoAuto("执行WSA清理（仅失败时）");
//                WSACleanup();
//                wsaInited = false;
//            }
//            NppSSH_LogInfoAuto("准备解锁连接状态");
//            // 解锁连接状态（加锁保证线程安全）
//            {
//                //s_connecting.store(false, std::memory_order_relaxed);
//                //s_cancelConnect.store(false, std::memory_order_relaxed);
//                // 异步线程中更新s_connecting时，强制内存同步
//                s_connecting.store(false, std::memory_order_seq_cst);
//                s_cancelConnect.store(false, std::memory_order_seq_cst);
//            }
//            // 保存错误信息并设置异步结果
//            threadErrorMsg = errorMsg;
//            connectPromise.set_value(connectResult);
//
//
//            // 消息投递安全校验
//            NppSSH_LogInfoAuto("连接线程结束，结果：" + std::to_string(connectResult) + "，准备投递消息到NPP主窗口");
//            // 优先使用保存的窗口句柄
//            if (::IsWindow(hNppMainWnd)) {
//                // 扩展消息参数：传递错误信息（通过自定义结构体/全局变量）
//                //std::string* pErrMsg = new std::string(errorMsg);
//                //::PostMessage(hNppMainWnd, WM_SSH_CONNECT_RESULT, (WPARAM)connectResult, (LPARAM)pErrMsg);
//                // 使用全局变量存储错误信息（线程安全）
//                std::lock_guard<std::mutex> errLock(s_errorMsgMutex);
//                s_lastConnectError = errorMsg;
//                // 只传递结果，不传递堆内存
//                ::PostMessage(hNppMainWnd, WM_SSH_CONNECT_RESULT, (WPARAM)connectResult, 0);
//
//            }
//            // 兜底 - 直接在主线程弹出错误提示（如果窗口句柄失效）
//            else if (!connectResult && !errorMsg.empty()) {
//                std::wstring wErrorMsg(errorMsg.begin(), errorMsg.end());
//                ::MessageBoxW(NULL, wErrorMsg.c_str(), L"NppSSH 连接失败", MB_OK | MB_ICONERROR);
//            }
//            });
//    }
//
//    //NppSSH_LogInfoAuto("s_connecting的状态2========================" + std::to_string(s_connecting.load()));
//    //NppSSH_LogInfoAuto("s_connecting的状态2========================" + std::to_string(s_connecting));
//    // 主线程等待（非阻塞优化：改用轮询+消息泵，避免UI卡死）
//    bool bTimeout = false;
//    std::future_status status;
//    //NppSSH_LogInfoAuto("进入主线程等待循环，当前s_connecting=" + std::to_string(s_connecting.load()));
//    do {
//        NppSSH_LogInfoAuto("s_connecting的状态0001========================" + std::to_string(s_connecting.load()));
//        // 每次等待100ms，期间处理UI消息
//        status = connectFuture.wait_for(std::chrono::milliseconds(50));
//        // 处理UI消息，防止界面卡死
//        MSG msg;
//        while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {// NULL = 处理所有窗口
//            //NppSSH_LogInfoAuto("s_connecting的状态0003========================" + std::to_string(s_connecting.load()));
//
//            ::TranslateMessage(&msg);
//            ::DispatchMessage(&msg);
//            // 检测取消消息
//            if (msg.message == WM_SSH_CONNECT_CANCEL) {
//                s_cancelConnect.store(true);
//                NppSSH_LogInfoAuto("主线程收到取消连接消息");
//            }
//            //NppSSH_LogInfoAuto("s_connecting的状态0004========================" + std::to_string(s_connecting.load()));
//
//            //if (msg.hwnd == s_nppData._nppHandle) {
//            //    ::SendMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam);
//            //}
//        }
//        NppSSH_LogInfoAuto("s_connecting的状态0002========================" + std::to_string(s_connecting.load()));
//    } while (status != std::future_status::ready && !s_cancelConnect.load());// 【修复】退出条件：明确超时/取消/就绪
//    NppSSH_LogInfoAuto("s_connecting的状态3========================" + std::to_string(s_connecting));
//    NppSSH_LogInfoAuto("s_connecting的状态3========================" + std::to_string(s_connecting.load()));
//    // 超时判断
//    if (s_cancelConnect.load() || connectFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout) {
//        bTimeout = true;
//        std::string errMsg = s_cancelConnect.load() ? "用户取消SSH连接" : "SSH连接超时（30秒），强制终止";
//        NppSSH_LogErrorAuto(errMsg);
//        // 加锁前日志
//        NppSSH_LogInfoAuto("准备加锁更新状态，当前s_connecting=" + std::to_string(s_connecting.load()));
//        std::lock_guard<std::timed_mutex> lock(s_connectMutex);
//        s_connecting.store(false);
//        s_connected.store(false);
//        // 同步镜像
//        std::lock_guard<std::mutex> mirrorLock(s_connected_mirror_mutex);
//        s_connected_mirror = false;
//        s_cancelConnect.store(false);
//        NppSSH_LogInfoAuto("加锁更新状态完成，s_connecting=" + std::to_string(s_connecting.load()));
//        // 提示用户
//        std::wstring wErrMsg = s_cancelConnect.load() ? L"已取消SSH连接" : L"SSH连接超时（30秒），请检查网络或服务器状态";
//        ::MessageBoxW(s_nppData._nppHandle, wErrMsg.c_str(), L"NppSSH 连接失败", MB_OK | MB_ICONERROR);
//
//        // 【新增】强制终止线程（防止线程挂起）
//        if (s_pConnectThread && s_pConnectThread->joinable()) {
//            // 注意：线程内需检测s_cancelConnect主动退出，此处仅兜底
//            s_cancelConnect.store(true);
//            s_pConnectThread->join();
//            delete s_pConnectThread;
//            s_pConnectThread = nullptr;
//        }
//    }
//    NppSSH_LogInfoAuto("s_connecting的状态4========================" + std::to_string(s_connecting.load()));
//    // 获取线程结果
//    bool finalResult = false;
//    if (!bTimeout) {
//        finalResult = connectFuture.get();
//        if (!finalResult && !threadErrorMsg.empty()) {
//            std::wstring wErrorMsg(threadErrorMsg.begin(), threadErrorMsg.end());
//            ::MessageBoxW(s_nppData._nppHandle, wErrorMsg.c_str(), L"NppSSH 连接失败", MB_OK | MB_ICONERROR);
//        }
//    }
//    NppSSH_LogInfoAuto("s_connecting的状态5========================" + std::to_string(s_connecting.load()));
//    // 主线程释放线程资源的逻辑修改：
//    {
//        std::lock_guard<std::mutex> threadLock(s_threadMutex);
//        if (s_pConnectThread) { // 先判断指针非空，再检查joinable
//            if (s_pConnectThread->joinable()) {
//                try {
//                    // 增加超时保护，避免永久阻塞
//                    std::chrono::seconds timeout(5);
//                    if (std::this_thread::sleep_for(std::chrono::milliseconds(100)), s_pConnectThread->joinable()) {
//                        s_pConnectThread->join();
//                    }
//                }
//                catch (const std::exception& e) {
//                    NppSSH_LogErrorAuto("线程join异常：" + std::string(e.what()));
//                }
//            }
//            delete s_pConnectThread;
//            s_pConnectThread = nullptr;
//        }
//        NppSSH_LogInfoAuto("s_connecting的状态7========================" + std::to_string(s_connecting.load()));
//    }
//
//    NppSSH_LogInfoAuto("异步连接完成，最终结果：" + std::to_string(finalResult));
//    return finalResult;
//}
 
// 断开SSH连接
void SSHConnection_Disconnect() {
    std::lock_guard<std::timed_mutex> lock(s_connectMutex); // 加锁保证线程安全
    if (s_connected.load()) {
        // 格式化指针/句柄为字符串
        char sessionBuf[64] = { 0 };
        char sockBuf[64] = { 0 };
        sprintf(sessionBuf, "0x%p", s_sshSession);
        sprintf(sockBuf, "%u", static_cast<unsigned int>(s_sock));
        NppSSH_LogInfoAuto("断开SSH连接，释放资源，服务器主机：" + std::string(ssh_host) + 
            " 端口：" + std::to_string(s_port) + 
            " 用户：" + std::string(s_user)+
            "sshSession："+std::string(sessionBuf) +
            "socket:"+std::string(sockBuf));
        // 释放动态分配的内存
        if (ssh_host) {
            free(ssh_host);
            ssh_host = nullptr;
        }
        if (s_user) {
            free(s_user);
            s_user = nullptr;
        }
        if (s_pass) {
            free(s_pass);
            s_pass = nullptr;
        }

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

        // 修复：安全释放全局Lib资源（避免重复释放）
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

// 重置连接状态
void SSHConnection_ResetState() {
    std::lock_guard<std::timed_mutex> lock(s_connectMutex);

    if (ssh_host) { free(ssh_host); ssh_host = _strdup("36.33.27.234"); }
    if (s_user) { free(s_user);  s_user = _strdup(""); }
    if (s_pass) { free(s_pass);  s_pass = _strdup(""); }

    s_connected.store(false);
    // 同步镜像
    std::lock_guard<std::mutex> mirrorLock(s_connected_mirror_mutex);
    s_connected_mirror = false;
    s_sshSession = nullptr;
    s_sock = INVALID_SOCKET;
    s_port = 22;

    NppSSH_LogInfoAuto("SSH连接状态已重置");
}

