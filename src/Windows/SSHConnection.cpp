//SSHConnection.cpp（SSH 连接具体实现）

#include "SSHConnection.h"
#include <tchar.h>

// SSH连接全局状态实际定义
static LIBSSH2_SESSION* s_sshSession = nullptr;
static SOCKET s_sock = INVALID_SOCKET;
static bool s_connected = false;
static const char* ssh_host = "36.33.27.234";
//const char* ssh_host = "36.33.27.234";
static int s_port = 22;
static const char* s_user = "";
static const char* s_pass = "";

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

// SSH连接具体实现
bool SSHConnection_Connect(const char* host, int port, const char* user, const char* pass) {
    s_connected = false;
    s_sshSession = nullptr;
    s_sock = INVALID_SOCKET;

    TCHAR szPort[16];
    _stprintf_s(szPort, _T("%d"), port);
    ::MessageBox(NULL, szPort, TEXT("NppSSHRel定义"), MB_OK);

    //开始连接操作
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return false; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    TCHAR buf[64];
    wsprintf(buf, TEXT("端口(网络序): %d\n端口(真实端口): %d"),
        addr.sin_port,            // 网络字节序（数字很大）
        ntohs(addr.sin_port));    // 真实端口（22）
    ::MessageBox(NULL, buf, TEXT("NppSSH端口"), MB_OK);
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError(); // 获取具体错误码
        TCHAR errBuf[128];
        wsprintf(errBuf, TEXT("SOCKET_ERROR, 错误码: %d"), err);
        ::MessageBox(NULL, errBuf, TEXT("NppSSH提示"), MB_OK);
        ::MessageBox(NULL, TEXT("10060：连接超时::服务器无响应-防火墙拦截\n10061：连接被拒绝::服务器端口未开放\n10065：无路由到主机::网络不可达"), TEXT("NppSSH提示"), MB_OK);
        closesocket(sock); WSACleanup(); return false;
    }
    if (libssh2_init(0) != 0) {
        ::MessageBox(NULL, TEXT("libssh2_init_ERROR"), TEXT("NppSSH提示"), MB_OK);
        closesocket(sock);
        WSACleanup();
        return false;
    }
    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        ::MessageBox(NULL, TEXT("libssh2_session_init_ERROR"), TEXT("NppSSH提示"), MB_OK);
        libssh2_exit();
        closesocket(sock);
        WSACleanup();
        return false;
    }
    libssh2_session_set_blocking(session, 1);
    if (libssh2_session_handshake(session, sock) != 0)
    {
        ::MessageBox(NULL, TEXT("libssh2_session_handshake_ERROR"), TEXT("NppSSH提示"), MB_OK);
        libssh2_session_free(session); libssh2_exit(); closesocket(sock); WSACleanup(); return false;
    }
    if (libssh2_userauth_password(session, user, pass) != 0)
    {
        libssh2_session_disconnect(session, "Auth Fail");
        ::MessageBox(NULL, TEXT("Auth Fail_ERROR!\n用户名或者密码错误！"), TEXT("NppSSH提示"), MB_OK);

        libssh2_session_free(session);
        libssh2_exit();
        closesocket(sock);
        WSACleanup();
        return false;
    }
    s_sock = sock;
    s_sshSession = session;
    s_connected = true;
    ssh_host = host;
    s_port = port;
    s_user = user;
    s_pass = pass;
    return true;
}

// 断开SSH连接
void SSHConnection_Disconnect() {
    if (s_connected) {
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
    }
}

// 判断是否连接
bool SSHConnection_IsConnected() {
    return s_connected;
}

// 重置连接状态
void SSHConnection_ResetState() {
    s_connected = false;
    s_sshSession = nullptr;
    s_sock = INVALID_SOCKET;
    ssh_host = "36.33.27.234";
    s_port = 22;
    s_user = "";
    s_pass = "";
}