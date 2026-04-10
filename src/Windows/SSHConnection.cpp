//SSHConnection.cpp（SSH 连接具体实现）

#include "SSHConnection.h"
#include <tchar.h>
// SSH连接全局状态实际定义
static LIBSSH2_SESSION* s_sshSession = nullptr;
static SOCKET s_sock = INVALID_SOCKET;
static bool s_connected = false;
static const char* ssh_host = "192.168.137.201";
//const char* ssh_host = "36.33.27.234";
static int s_port = 22;
static const char* s_user = "root";
static const char* s_pass = "123456";

static NppData s_nppData;
static HINSTANCE s_hInst;

// 全局状态获取接口
LIBSSH2_SESSION*& SSHConnection_GetSession() {
    return s_sshSession;
}

SOCKET& SSHConnection_GetSocket() {
    return s_sock;
}

bool& SSHConnection_GetConnectedState() {
    return s_connected;
}

const char*& SSHConnection_GetHost() {
    return ssh_host;
}

int& SSHConnection_GetPort() {
    return s_port;
}

const char*& SSHConnection_GetUser() {
    return s_user;
}

const char*& SSHConnection_GetPass() {
    return s_pass;
}
// 工具函数：释放连接资源（抽离公共逻辑）统一释放 Socket、SSH 会话、libssh2、WSA 资源；
static void ReleaseConnectionResources(SOCKET sock, LIBSSH2_SESSION* session) {
    if (session) {
        libssh2_session_free(session);
    }
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    libssh2_exit();
    WSACleanup();
}
// SSH连接具体实现
bool SSHConnection_Connect(const char* host, int port, const char* user, const char* pass) {
    
    // 1. 入参合法性校验
    if (!host || !user || !pass || port <= 0 || port > 65535) {
        ::MessageBoxW(s_nppData._nppHandle, L"无效的连接参数！主机/用户/密码不能为空，端口需在1-65535之间", L"NppSSH 错误提示", MB_OK | MB_ICONERROR);
        return false;
    }
    // 连接前打印Info日志（手动指定事件名）
    std::string connectInfo = "开始尝试连接SSH服务器，主机：" + std::string(host) + "，端口：" + std::to_string(port) + "，用户：" + std::string(user);
    NppSSH_LogInfoAuto(connectInfo);


    s_connected = false;
    s_sshSession = nullptr;
    s_sock = INVALID_SOCKET;
    wchar_t paramMsg[1024] = { 0 }; // 加大缓冲区并初始化
    swprintf_s(paramMsg, _countof(paramMsg), L"是否确认连接？\n\nSSH连接参数：\n主机 = %hs\n端口 = %d\n用户 = %hs\n密码 = %hs",
        host, port, user, pass);
    // 强制激活NPP主窗口，确保系统认定NPP为活跃窗口（关键！）
    ::SetForegroundWindow(s_nppData._nppHandle);
    ::SetActiveWindow(s_nppData._nppHandle);
    ::SetWindowPos(s_nppData._nppHandle, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 禁用NPP主窗口，防止操作（系统标准模态第一步）
    //::EnableWindow(s_nppData._nppHandle, FALSE);
    int ret = ::MessageBoxW(                        //弹框确认取消提示框
        s_nppData._nppHandle,
        paramMsg,
        L"NppSSH 连接提示",
        MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL // 置顶+模态// 系统模态，强制锁定
    );
    
    if (ret == IDNO) {
        return false;
    }
    //开始连接操作   初始化WSA  创建Socket
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return false; }

    // 配置服务器地址
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    NppSSH_LogInfoAuto("端口(网络序)：" + std::to_string(addr.sin_port) + "，端口(真实端口)：" + std::to_string(ntohs(addr.sin_port)));

    inet_pton(AF_INET, host, &addr.sin_addr);

    // 连接服务器
    int connectRet = connect(sock, (sockaddr*)&addr, sizeof(addr));
    if (connectRet == SOCKET_ERROR)
    {      
        int err = WSAGetLastError(); // 获取具体错误码
        NppSSH_LogErrorAuto("SOCKET_ERROR失败！用户：" + std::string(user) + "，错误码：" + std::to_string(err));
        NppSSH_LogWarnAuto("10060：连接超时::服务器无响应-防火墙拦截 10061：连接被拒绝::服务器端口未开放 10065：无路由到主机::网络不可达");
        ReleaseConnectionResources(sock, nullptr);
        return false;
    }

    // 初始化libssh2
    int libssh2Ret = libssh2_init(0);
    if (libssh2Ret != 0) {
        NppSSH_LogErrorAuto("libssh2初始化失败！用户：" + std::string(user));
        ReleaseConnectionResources(sock, nullptr);
        return false;
    }

    // 创建SSH会话
    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        NppSSH_LogErrorAuto("libssh2_session_init_ERROR！用户：" + std::string(user));
        ReleaseConnectionResources(sock, nullptr);
        return false;
    }

    // SSH握手
    libssh2_session_set_blocking(session, 1);
    if (libssh2_session_handshake(session, sock) != 0)
    {
        NppSSH_LogErrorAuto("SSH握手失败！用户：" + std::string(user));
        ReleaseConnectionResources(sock, session);
        return false;
    }

    // 密码认证
    if (libssh2_userauth_password(session, user, pass) != 0)
    {
        libssh2_session_disconnect(session, "Auth Fail");
        // 错误日志  异常场景示例（比如认证失败）
        NppSSH_LogErrorAuto("SSH密码认证失败，用户：" + std::string(user));
        ReleaseConnectionResources(sock, session);
        return false;
    }

    // 连接成功，更新全局状态
    s_sock = sock;
    s_sshSession = session;
    s_connected = true;
    ssh_host = _strdup(host);  // 避免原指针失效，动态分配内存（需在Disconnect时释放）
    s_port = port;
    s_user = _strdup(user);    // 动态分配内存
    s_pass = _strdup(pass);    // 动态分配内存

    ///// ===================== 日志测试（连接成功输出，全部走SSHWindow中转）=====================
    //// 1. 自动获取当前函数名作为 event（最常用）
    //NppSSH_LogInfoAuto("==============测试日志使用开始==========");
    //NppSSH_LogInfoAuto("SSH连接成功，Socket与会话已创建");

    //// 2. 手动指定 event 名称
    //NppSSH_LogInfo("SSH_Handshake", "SSH协议握手完成，服务器响应正常");

    //// 3. event 传空字符串（触发兜底 unknown）
    //NppSSH_LogInfo("", "用户密码认证通过，登录成功");

    //// 4. 错误级别日志（测试）
    //NppSSH_LogError("SSH_Connect_Test", "测试错误日志：连接流程正常结束");

    //// 5. 调试级别日志
    //NppSSH_LogDebug("SSH_Session", "libssh2会话已初始化，阻塞模式开启");

    //// 6. 警告级别日志
    //NppSSH_LogWarn("SSH_KeepAlive", "测试警告：连接成功，心跳未启动");//支持“\n”换行，例如：心跳\n未启动

    //// 7. 输出服务器远程信息（你要的握手/返回内容）
    //std::string serverInfo = "服务器主机：" + std::string(host) + " 端口：" + std::to_string(port) + " 用户：" + std::string(user);
    //NppSSH_LogInfo("SSH_ServerInfo", serverInfo);

    //// 8. event 传空字符串 + 错误级别（兜底测试）
    //NppSSH_LogError("", "连接状态已标记为已连接");
    //NppSSH_LogInfoAuto("==============测试日志使用结束==========");
    return true;
}

// 断开SSH连接
void SSHConnection_Disconnect() {
    if (s_connected) {
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

        // 释放动态分配的字符串内存
        if (ssh_host) {
            free((void*)ssh_host);
            ssh_host = nullptr;
        }
        if (s_user) {
            free((void*)s_user);
            s_user = nullptr;
        }
        if (s_pass) {
            free((void*)s_pass);
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
            closesocket(s_sock);
            s_sock = INVALID_SOCKET;
        }
        // 更新状态
        s_connected = false;
        WSACleanup();
        libssh2_exit();
        NppSSH_LogInfoAuto("SSH连接已断开，资源释放完成");
    }
}

// 判断是否连接
bool SSHConnection_IsConnected() {
    return s_connected;
}

// 重置连接状态
void SSHConnection_ResetState() {
    // 释放动态分配的内存
    if (ssh_host) {
        free((void*)ssh_host);
        ssh_host = "36.33.27.234";
    }
    if (s_user) {
        free((void*)s_user);
        s_user = "";
    }
    if (s_pass) {
        free((void*)s_pass);
        s_pass = "";
    }
    s_connected = false;
    s_sshSession = nullptr;
    s_sock = INVALID_SOCKET;
    ssh_host = "36.33.27.234";
    s_port = 22;
}