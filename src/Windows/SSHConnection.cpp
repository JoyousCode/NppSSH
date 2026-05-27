//SSHConnection.cpp（SSH 连接具体实现）
#include "SSHConnection.h"
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <stdarg.h>

// 全局变量（改用智能指针管理）
std::unordered_map<int, std::unique_ptr<SSHConnection>> g_panelConnections;
std::mutex g_panelConnMutex;


static NppData s_nppData;
static HINSTANCE s_hInst;

// ====================== 工具函数 ======================
// 端口验证
static bool ValidatePort(int port) {
    return port >= SSHConst::MIN_PORT && port <= SSHConst::MAX_PORT;
}

// 编码转换（自动识别 UTF8 / GBK）
inline std::wstring GBKToWstring(const std::string& str) {
    if (str.empty()) return L"";

    // 1. 优先按 UTF-8 转换（libssh2 错误信息都是 UTF-8）
    int utf8Len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (utf8Len > 0) {
        std::wstring wstr(utf8Len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], utf8Len);
        wstr.pop_back(); // 移除末尾的\0
        return wstr;
    }

    // 2. 失败则使用 GBK（系统本地编码）
    int gbkLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(gbkLen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], gbkLen);
    wstr.pop_back();
    return wstr;
}

// 获取libssh2错误信息
inline std::string GetLibssh2ErrorMsg(LIBSSH2_SESSION* session) {
    if (!session) return "无效的session";

    char* errmsg = nullptr;
    int code = libssh2_session_last_error(session, &errmsg, nullptr, 0);

    std::string result;

    // 如果有libssh2提供的错误消息
    if (errmsg && strlen(errmsg) > 0) {
        result = std::string(errmsg);
    }
    else {
        // 如果没有详细错误消息，使用错误码映射
        result = "未知错误";
    }

    // 添加错误码
    result += "（错误码：" + std::to_string(code) + "）";

    // 根据错误码添加详细解释
    std::string explanation = GetLibssh2ErrorExplanation(code);
    if (!explanation.empty()) {
        result += " [" + explanation + "]";
    }

    return result;
}

// 获取libssh2错误码的详细解释
inline std::string GetLibssh2ErrorExplanation(int error_code) {
    switch (error_code) {
        // 通用错误
    case LIBSSH2_ERROR_NONE: return "无错误";
    case LIBSSH2_ERROR_SOCKET_NONE: return "Socket无效或未初始化";
    case LIBSSH2_ERROR_BANNER_SEND: return "发送SSH banner失败";
    case LIBSSH2_ERROR_BANNER_RECV: return "接收SSH banner失败，服务器未响应或响应无效";
    case LIBSSH2_ERROR_INVALID_MAC: return "MAC验证失败，可能被篡改";
    case LIBSSH2_ERROR_KEX_FAILURE: return "密钥交换失败，算法不兼容";
    case LIBSSH2_ERROR_ALLOC: return "内存分配失败";
    case LIBSSH2_ERROR_SOCKET_SEND: return "Socket发送失败，网络问题";
    case LIBSSH2_ERROR_SOCKET_RECV: return "Socket接收失败，网络问题";
    case LIBSSH2_ERROR_SOCKET_DISCONNECT: return "Socket连接已断开";
    case LIBSSH2_ERROR_PROTO: return "SSH协议错误";
    case LIBSSH2_ERROR_PASSWORD_EXPIRED: return "密码已过期";
    case LIBSSH2_ERROR_FILE: return "文件操作失败";
    case LIBSSH2_ERROR_METHOD_NONE: return "未设置认证方法";
    case LIBSSH2_ERROR_AUTHENTICATION_FAILED: return "认证失败，用户名或密码错误";
    case LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED: return "公钥未验证";
    case LIBSSH2_ERROR_CHANNEL_OUTOFORDER: return "通道顺序错误";
    case LIBSSH2_ERROR_CHANNEL_FAILURE: return "通道操作失败";
    case LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED: return "通道请求被服务器拒绝";
    case LIBSSH2_ERROR_CHANNEL_UNKNOWN: return "未知通道";
    case LIBSSH2_ERROR_CHANNEL_WINDOW_EXCEEDED: return "通道窗口大小超出";
    case LIBSSH2_ERROR_CHANNEL_PACKET_EXCEEDED: return "通道数据包大小超出";
    case LIBSSH2_ERROR_CHANNEL_CLOSED: return "通道已关闭";
    case LIBSSH2_ERROR_CHANNEL_EOF_SENT: return "已发送EOF";
    case LIBSSH2_ERROR_SCP_PROTOCOL: return "SCP协议错误";
    case LIBSSH2_ERROR_ZLIB: return "ZLIB压缩错误";
    case LIBSSH2_ERROR_SOCKET_TIMEOUT: return "Socket操作超时";
    case LIBSSH2_ERROR_SFTP_PROTOCOL: return "SFTP协议错误";
    case LIBSSH2_ERROR_REQUEST_DENIED: return "请求被服务器拒绝";
    case LIBSSH2_ERROR_METHOD_NOT_SUPPORTED: return "方法不被支持";
    case LIBSSH2_ERROR_INVAL: return "无效参数";
    case LIBSSH2_ERROR_INVALID_POLL_TYPE: return "无效的轮询类型";
    case LIBSSH2_ERROR_PUBLICKEY_PROTOCOL: return "公钥协议错误";
    case LIBSSH2_ERROR_EAGAIN: return "操作会阻塞，请在非阻塞模式下重试";
    case LIBSSH2_ERROR_BUFFER_TOO_SMALL: return "缓冲区太小";
    case LIBSSH2_ERROR_BAD_USE: return "API使用错误";
    case LIBSSH2_ERROR_COMPRESS: return "压缩错误";
    case LIBSSH2_ERROR_OUT_OF_BOUNDARY: return "超出边界";
    case LIBSSH2_ERROR_AGENT_PROTOCOL: return "SSH代理协议错误";
    case LIBSSH2_ERROR_ENCRYPT: return "加密错误";
    case LIBSSH2_ERROR_BAD_SOCKET: return "无效的socket";
    case LIBSSH2_ERROR_KNOWN_HOSTS: return "已知主机验证失败";
    case LIBSSH2_ERROR_HOSTKEY_INIT: return "主机密钥初始化失败";
    case LIBSSH2_ERROR_HOSTKEY_SIGN: return "主机密钥签名失败";
    case LIBSSH2_ERROR_DECRYPT: return "解密失败";
    case LIBSSH2_ERROR_KEY_EXCHANGE_FAILURE: return "密钥交换失败";
    case LIBSSH2_ERROR_TIMEOUT: return "操作超时";

        // 系统错误码范围（通常为负值）
    default:
        if (error_code < 0) {
            if (error_code >= -100) {
                return "libssh2内部错误";
            }
            else {
                return "系统错误或网络错误";
            }
        }
        return "";
    }
}
// 辅助函数：判断字符是否为命令分隔符
inline bool isCmdSeparator(char c) {
    return c == ';' || c == '|' || c == '&';
}

// 辅助函数：跳过连续的空白字符（空格/制表符）
inline size_t skipWhitespace(const std::string& str, size_t pos) {
    while (pos < str.size() && std::isspace(static_cast<unsigned char>(str[pos]))) {
        pos++;
    }
    return pos;
}

// 辅助函数：查找命令参数的结束位置（分隔符/空白符）
inline size_t findArgEnd(const std::string& str, size_t pos) {
    while (pos < str.size()) {
        if (std::isspace(static_cast<unsigned char>(str[pos])) || isCmdSeparator(str[pos])) {
            break;
        }
        pos++;
    }
    return pos;
}
// 辅助函数：清理字符串末尾所有 \r\n，直到最后一个字符不是换行 / 回车。
inline std::string TrimTrailingNewlines(std::string str) {
    // 循环删除末尾的 \r 和 \n，直到不是这两个字符
    while (!str.empty()) {
        char c = str.back();
        if (c == '\r' || c == '\n') {
            str.pop_back();
        }
        else {
            break;
        }
    }
    return str;
}
// 工具函数：去除字符串末尾所有空白（空格、\t、\n、\r）
inline std::string TrimTrailingWhitespace(std::string str) {
    size_t pos = str.find_last_not_of(" \t\n\r");
    if (pos != std::string::npos) {
        str.erase(pos + 1);
    }
    else {
        str.clear();
    }
    return str;
}

// 工具函数：判断命令【去除末尾空白后】是否以 ; 结尾
inline bool EndsWithSemicolonAfterTrim(const std::string& cmd) {
    std::string trimmed = TrimTrailingWhitespace(cmd);
    return !trimmed.empty() && trimmed.back() == ';';
}
// 工具函数：根据 panelId 获取连接实例（线程安全）
SSHConnection* GetSSHConnectionByPanelId(int panelId) {
    // 先判断是否存在（复用你已有的工具函数）
    if (!IsPanelIdExists(panelId)) {
        return nullptr;
    }

    // 加锁安全获取实例指针
    SSHConnection* conn = nullptr;
    {
        std::lock_guard<std::mutex> mapLock(g_panelConnMutex);
        auto it = g_panelConnections.find(panelId);
        if (it != g_panelConnections.end()) {
            conn = it->second.get();
        }
    }

    return conn;
}
//工具函数，通过 this或者实例对象 指针查找对应的 面板ID（key）
int SSHConnection_GetPanelId(SSHConnection* self) {
    std::lock_guard<std::mutex> lock(g_panelConnMutex);
    for (auto& pair : g_panelConnections) {
        if (pair.second.get() == self) {
            return pair.first; // 返回正确面板ID
        }
    }
    return -1;
}
// 工具函数：提取字符串最后一行
std::string SSHConnection::extractLastLine(const std::string& str) {
    if (str.empty()) {
        return "";
    }

    // 找到最后一个换行符
    size_t lastPos = str.find_last_of("\r\n");
    if (lastPos == std::string::npos) {
        return str; // 没有换行，直接返回整个字符串
    }

    // 直接返回最后一行，**不做任何清理、不做任何处理**
    return str.substr(lastPos + 1);
}
//工具函数，InitSSHSession函数初始化SSH会话并握手，检查socket是否有效
bool SSHConnection::IsSocketAlive(SOCKET sock) {
    if (sock == INVALID_SOCKET) {
        return false;
    }

    // 1. 检查socket错误状态
    int error = 0;
    socklen_t len = sizeof(error);
    int ret = getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len);

    if (ret != 0) {
        NppSSH_LogDebugAuto("getsockopt失败: " + std::to_string(WSAGetLastError()));
        return false;
    }

    if (error != 0) {
        NppSSH_LogDebugAuto("Socket错误码: " + std::to_string(error));
        return false;
    }

    // 2. 使用select检查socket是否可读（Windows兼容版本）
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    timeval timeout = { 0, 100000 }; // 100ms超时 (0秒, 100000微秒)

    int select_ret = select(0, &readfds, nullptr, nullptr, &timeout);

    if (select_ret < 0) {
        int err = WSAGetLastError();
        NppSSH_LogDebugAuto("select失败: " + std::to_string(err));
        return false;
    }

    if (select_ret > 0 && FD_ISSET(sock, &readfds)) {
        // socket可读，说明连接正常
        return true;
    }

    // 3. 补充检查：尝试发送0字节数据
    char dummy = 0;
    ret = send(sock, &dummy, 0, 0); // 使用0标志，而不是MSG_NOSIGNAL或MSG_DONTWAIT

    if (ret < 0) {
        int err = WSAGetLastError();
        // 在Windows上，WSAEWOULDBLOCK表示socket是可写的但暂时阻塞
        // WSAEISCONN表示已连接
        if (err != WSAEWOULDBLOCK && err != WSAEISCONN) {
            NppSSH_LogDebugAuto("send测试失败: " + std::to_string(err));
            return false;
        }
    }

    return true;
}
// 工具函数：SSH握手成功后，再次检查socket是否有效，
// 连接成功后，CreatePtyChannel函数申请PTY伪终端再次检测socket是否有效。
bool SSHConnection::IsSocketValid(SOCKET sock) {
    if (sock == INVALID_SOCKET) {
        return false;
    }

    // 通过select检查socket是否可读（但不会阻塞）
    fd_set fd;
    struct timeval tv = { 0, 0 };  // 零超时

    FD_ZERO(&fd);
    FD_SET(sock, &fd);

    int rc = select(sock + 1, &fd, nullptr, nullptr, &tv);

    if (rc < 0) {
        // select错误，socket可能无效
        int err = WSAGetLastError();
        if (err == WSAENOTSOCK) {
            return false;
        }
    }

    return true;
}
// 工具函数：根据阻塞方向等待socket
// 优化的等待函数：带指数退避
bool SSHConnection::WaitSocketWithBackoff(SOCKET sock, LIBSSH2_SESSION* session,
    int base_wait_ms, int max_attempts) {
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        // 计算当前等待时间（带退避）
        int wait_ms = base_wait_ms * attempt;

        // 检查阻塞方向
        long directions = libssh2_session_block_directions(session);

        if (directions == 0) {
            NppSSH_LogInfoAuto("【WaitSocket】无阻塞方向，无需等待");
            return true;
        }

        NppSSH_LogInfoAuto("【WaitSocket】等待方向: " + std::to_string(directions) +
            ", 时间: " + std::to_string(wait_ms) + "ms");

        fd_set fd_read, fd_write;
        struct timeval tv;

        FD_ZERO(&fd_read);
        FD_ZERO(&fd_write);

        if (directions & LIBSSH2_SESSION_BLOCK_INBOUND) {
            FD_SET(sock, &fd_read);
        }
        if (directions & LIBSSH2_SESSION_BLOCK_OUTBOUND) {
            FD_SET(sock, &fd_write);
        }

        // 设置超时
        tv.tv_sec = wait_ms / 1000;
        tv.tv_usec = (wait_ms % 1000) * 1000;

        int rc = select(sock + 1,
            (directions & LIBSSH2_SESSION_BLOCK_INBOUND) ? &fd_read : nullptr,
            (directions & LIBSSH2_SESSION_BLOCK_OUTBOUND) ? &fd_write : nullptr,
            nullptr, &tv);

        if (rc > 0) {
            NppSSH_LogInfoAuto("【WaitSocket】socket可读写");

            // 再次检查阻塞方向是否清除
            directions = libssh2_session_block_directions(session);
            if (directions == 0) {
                NppSSH_LogInfoAuto("【WaitSocket】阻塞方向已清除");
            }
            return true;
        }
        else if (rc == 0) {
            NppSSH_LogInfoAuto("【WaitSocket】等待超时 (" +
                std::to_string(attempt) + "/" +
                std::to_string(max_attempts) + ")");

            // 检查socket是否仍然有效
            if (!IsSocketValid(sock)) {
                NppSSH_LogErrorAuto("【WaitSocket】socket已失效");
                return false;
            }

            // 重新检查阻塞方向
            long new_directions = libssh2_session_block_directions(session);
            if (new_directions != directions) {
                NppSSH_LogInfoAuto("【WaitSocket】阻塞方向已改变: " +
                    std::to_string(new_directions));
                // 阻塞方向改变，重新等待
                directions = new_directions;
            }
        }
        else {
            int err = WSAGetLastError();
            NppSSH_LogErrorAuto("【WaitSocket】select错误: " + std::to_string(err));
            return false;
        }
    }

    NppSSH_LogErrorAuto("【WaitSocket】达到最大等待次数");
    return false;
}
// 工具函数：判断伪终端是否就绪
bool SSHConnection::IsShellReady()
{
    // 1. 先判断通道指针是否存在
    LIBSSH2_CHANNEL* ch = m_shellChannel.load(std::memory_order_acquire);
    if (!ch)
        return false;

    // 2. 判断通道是否已退出（!=0 表示已关闭）
    int exitStatus = libssh2_channel_get_exit_status(ch);
    if (exitStatus != 0)
        return false;

    // 3. 判断是否收到 EOF（流结束）
    if (libssh2_channel_eof(ch))
        return false;

    // 4. 能走到这里 = 通道已创建、未关闭、未EOF、就绪可用
    return true;
}
// 工具函数：判断是否以指定字符串开头
bool SSHConnection::startsWith(const std::string& str, const std::string& prefix) {
    if (str.size() < prefix.size()) return false;
    return str.compare(0, prefix.size(), prefix) == 0;
}
// ====================== SSHConnection类实现 ======================
// 构造函数
SSHConnection::SSHConnection() = default;

// 移动构造
SSHConnection::SSHConnection(SSHConnection&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_mutex);

    // 移动资源
    m_session = other.m_session;
    m_sock = other.m_sock;
    m_connected.store(other.m_connected.load(std::memory_order_acquire), std::memory_order_release);
    m_connecting.store(other.m_connecting.load(std::memory_order_acquire), std::memory_order_release);
    m_cancelConnect.store(other.m_cancelConnect.load(std::memory_order_acquire), std::memory_order_release);
    m_host = std::move(other.m_host);
    m_user = std::move(other.m_user);
    m_pass = std::move(other.m_pass);
    m_port = other.m_port;
    m_prompt = std::move(other.m_prompt);
    m_shellChannel.store(other.m_shellChannel.load(std::memory_order_acquire), std::memory_order_release);
    m_stopHeartbeat.store(other.m_stopHeartbeat.load(std::memory_order_acquire), std::memory_order_release);
    m_heartbeatThread = std::move(other.m_heartbeatThread);
    m_pConnectThread = other.m_pConnectThread;

    // 源对象置空
    other.m_session = nullptr;
    other.m_sock = INVALID_SOCKET;
    other.m_connected.store(false, std::memory_order_release);
    other.m_connecting.store(false, std::memory_order_release);
    other.m_cancelConnect.store(false, std::memory_order_release);
    other.m_port = 22;
    other.m_shellChannel.store(nullptr, std::memory_order_release);
    other.m_stopHeartbeat.store(true, std::memory_order_release);

    other.m_pConnectThread = nullptr;
}

// 移动赋值
SSHConnection& SSHConnection::operator=(SSHConnection&& other) noexcept {
    if (this != &other) {
        // 释放当前资源
        ReleaseResources();

        // 移动对方资源
        std::lock_guard<std::mutex> lock(other.m_mutex);
        m_session = other.m_session;
        m_sock = other.m_sock;
        m_connected.store(other.m_connected.load(std::memory_order_acquire), std::memory_order_release);
        m_connecting.store(other.m_connecting.load(std::memory_order_acquire), std::memory_order_release);
        m_cancelConnect.store(other.m_cancelConnect.load(std::memory_order_acquire), std::memory_order_release);
        m_host = std::move(other.m_host);
        m_user = std::move(other.m_user);
        m_pass = std::move(other.m_pass);
        m_port = other.m_port;
        m_prompt = std::move(other.m_prompt);
        m_shellChannel.store(other.m_shellChannel.load(std::memory_order_acquire), std::memory_order_release);
        m_stopHeartbeat.store(other.m_stopHeartbeat.load(std::memory_order_acquire), std::memory_order_release);

        m_heartbeatThread = std::move(other.m_heartbeatThread);
        m_pConnectThread = other.m_pConnectThread;

        // 源对象置空
        other.m_session = nullptr;
        other.m_sock = INVALID_SOCKET;
        other.m_connected.store(false, std::memory_order_release);
        other.m_connecting.store(false, std::memory_order_release);
        other.m_cancelConnect.store(false, std::memory_order_release);
        other.m_port = 22;
        other.m_shellChannel.store(nullptr, std::memory_order_release);
        other.m_stopHeartbeat.store(true, std::memory_order_release);

        other.m_pConnectThread = nullptr;
    }
    return *this;
}

// 析构函数
SSHConnection::~SSHConnection() {
    // 1. 标记实例死亡
    m_isAlive.store(false, std::memory_order_release);
    m_stopHeartbeat.store(true, std::memory_order_release);

    // 2. 停止心跳（等待线程退出）
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }

    // 3. 清理连接线程
    if (m_pConnectThread) {
        if (m_pConnectThread->joinable()) {
            m_pConnectThread->detach(); // 避免线程未结束导致崩溃
        }
        delete m_pConnectThread;
        m_pConnectThread = nullptr;
    }

    // 4. 释放资源（加锁）
    ReleaseResources();
    StopShellReader();
}

// 释放资源
void SSHConnection::ReleaseResources() {
    //std::lock_guard<std::mutex> lock(m_mutex);//打开会发生重复加锁
    NppSSH_LogInfoAuto("释放资源.............");
    SOCKET oldSock = INVALID_SOCKET;
    LIBSSH2_SESSION* oldSession = nullptr;
    LIBSSH2_CHANNEL* oldChannel = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_readerMutex);
        m_stopReader.store(true, std::memory_order_release);
        m_waitingForPrompt.store(false, std::memory_order_release);

        // 保存旧指针（不提前置空！）
        oldSock = m_sock;
        oldSession = m_session;
        oldChannel = m_shellChannel.load(std::memory_order_acquire);
    }
    // 等待线程彻底退出，防止线程还在操作通道
    if (m_shellReaderThread.joinable()) {
        NppSSH_LogInfoAuto("释放资源.............等待ShellReader线程完全退出");
        m_shellReaderThread.join();
        m_shellReaderThread = std::thread(); // 重置线程对象
    }


    // 先停止心跳（避免心跳线程访问已释放资源）
    // 第一步：先标记停止心跳（内存序用release，保证对其他线程可见）
    m_stopHeartbeat.store(true, std::memory_order_release);
    // 唤醒心跳线程（如果在wait_for中阻塞，立即唤醒）
    m_heartbeatCv.notify_one();
    if (m_heartbeatThread.joinable()) {
        NppSSH_LogInfoAuto("释放资源.............直接分离心跳线程（不等待）");
        m_heartbeatThread.detach();//直接不等待，让线程脱离主线程，自生自灭，根据废掉所有资源会自动销毁
        //m_heartbeatThread.join();//等线程执行完才会执行
    }
    else {
        NppSSH_LogInfoAuto("释放资源.............心跳线程不存在");
    }

    
    // 释放shell通道
    if (oldChannel) {
        try {
            NppSSH_LogInfoAuto("释放资源.............释放shell通道");
            libssh2_channel_close(oldChannel);
            libssh2_channel_wait_closed(oldChannel);
            libssh2_channel_free(oldChannel);
        }
        catch (...) {}
    }

    // 释放SSH会话
    if (oldSession) {
        try {
            NppSSH_LogInfoAuto("释放资源.............释放SSH会话");
            //发一个 SSH 关闭包,注释掉与Putty和Mobaxterm一致，不发 SSH 断开，直接关 TCP
            // 发送 SSH 协议断开报文，属于协议规范。暂时注释关闭与其他软件一致
            libssh2_session_disconnect(oldSession, "bye");//"bye"正确退出，"Connection closed"强制退出
            libssh2_session_free(oldSession);
        }
        catch (...) {}
    }

    // 关闭Socket
    if (oldSock != INVALID_SOCKET) {
        try {
            NppSSH_LogInfoAuto("释放资源.............关闭Socket");
            //shutdown(oldSock, SD_BOTH);// 会强制切断 TCP，导致服务器报 Connection closed。导致强制断开
            closesocket(oldSock);
        }
        catch (...) {}
    }
    NppSSH_LogInfoAuto("释放资源.............重置状态");
    // 重置状态
    {
        std::lock_guard<std::mutex> lock(m_readerMutex);
        m_sock = INVALID_SOCKET;
        m_session = nullptr;
        m_shellChannel.store(nullptr, std::memory_order_release);
        m_connected.store(false, std::memory_order_release);
        m_connecting.store(false, std::memory_order_release);
        m_cancelConnect.store(false, std::memory_order_release);
        m_prompt.clear();
    }
    
}

// 设置主机
void SSHConnection::SetHost(const char* host) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_host = host ? host : "";
}

// 设置用户名
void SSHConnection::SetUser(const char* user) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_user = user ? user : "";
}

// 设置密码
void SSHConnection::SetPass(const char* pass) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pass = pass ? pass : "";
}

// 判断是否连接
bool SSHConnection::IsConnected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected.load(std::memory_order_acquire)
        && m_session != nullptr
        && m_sock != INVALID_SOCKET;
}

// 心跳线程函数
void SSHConnection::HeartbeatThreadFunc() {
    NppSSH_LogInfoAuto("心跳线程已启动");
    int secondCount = 0;

    // 循环运行，直到收到停止信号
    while (true) {
        // 第一步：先检测停止信号（打印日志前检测，避免无效日志）
        if (m_stopHeartbeat.load(std::memory_order_acquire)) { // 改用acquire内存序
            NppSSH_LogInfoAuto("收到停止信号，立即退出");
            goto THREAD_EXIT;
        }

        // 第二步：带超时的等待（替代sleep_for，支持即时唤醒）
        {
            std::unique_lock<std::mutex> lock(m_heartbeatMtx);
            // 等待1秒，或被唤醒（停止信号触发时唤醒）
            if (m_heartbeatCv.wait_for(lock, std::chrono::seconds(SSHConst::MAX_HEART_BEAT_WAIT_MS),
                [this]() { return m_stopHeartbeat.load(std::memory_order_acquire); })) {
                // 被唤醒且检测到停止信号，直接退出
                NppSSH_LogInfoAuto("等待期间收到停止信号，退出心跳线程");
                goto THREAD_EXIT;
            }
        }

        // 第三步：计数+打印日志（此时已确认未收到停止信号）
        secondCount++;

        //std::ostringstream oss;
        //oss << m_heartbeatThread.get_id();
        //NppSSH_LogInfoAuto("心跳线程[" + oss.str() + "]已启动循环，第" + std::to_string(secondCount) + "次" +
        //    std::to_string(m_stopHeartbeat.load(std::memory_order_acquire)));

        // 第四步：心跳逻辑（增加空指针检测，避免访问已释放资源）
        if (secondCount >= SSHConst::HEARTBEAT_INTERVAL_MS) {
            secondCount = 0;
            // 双重检测：停止信号+资源有效性
            if (m_stopHeartbeat.load(std::memory_order_acquire)) {
                goto THREAD_EXIT;
            }
            if (m_session != nullptr && m_connected.load(std::memory_order_acquire)) {
                int next_interval = 0;
                int ret = libssh2_keepalive_send(m_session, &next_interval);
                if (ret == 0) {
                    NppSSH_LogInfoAuto("心跳包发送成功（SSH_MSG_IGNORE），下次间隔：" + std::to_string(next_interval) + "s");
                }
                else {
                    NppSSH_LogInfoAuto("心跳包发送失败，连接可能已断开");
                    Disconnect();
                    goto THREAD_EXIT;
                }
            }
        }
    }

THREAD_EXIT:
    //模拟断开按钮操作，解决连接状态下服务器关机，重置面板状态，
                    //主要是通过心跳失败，进行模拟点击，目前1秒睡眠+间隔30秒心跳包，服务器关机后，最多60秒即可重置面板
    if (m_panelHwnd != nullptr && ::IsWindow(m_panelHwnd)) {
        // 发送WM_COMMAND消息，模拟点击断开按钮
        // WPARAM: LOWORD=控件ID, HIWORD=BN_CLICKED（按钮点击通知码）
        // LPARAM: 控件句柄（如果不需要精准定位控件，传NULL也可）
        WPARAM wParam = MAKEWPARAM(IDC_BTN_DISCONNECT_SSH, BN_CLICKED);
        LPARAM lParam = (LPARAM)::GetDlgItem(m_panelHwnd, IDC_BTN_DISCONNECT_SSH); // 获取断开按钮句柄

        NppSSH_LogInfoAuto("模拟点击面板的断开按钮");
        // 发送消息（SendMessage同步，确保UI处理完成；PostMessage异步，根据需求选）
        ::SendMessage(m_panelHwnd, WM_COMMAND, wParam, lParam);
    }
    NppSSH_LogInfoAuto("心跳线程正常退出");
}

// 启动心跳
void SSHConnection::StartHeartbeat() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_heartbeatThread.joinable())
        return;
    m_stopHeartbeat.store(false, std::memory_order_release);
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }
    NppSSH_LogInfoAuto("启动心跳线程");
    
    // 参数说明：want_reply = 1：要求服务器回复心跳包（服务器会确认收到，避免超时断开）
    // interval = 10：每10秒发送一次心跳包（低于服务器默认的3分钟超时）
    libssh2_keepalive_config(m_session, SSHConst::MAX_HEART_BEAT_WAIT_MS, SSHConst::HEARTBEAT_INTERVAL_MS);

    m_heartbeatThread = std::thread(&SSHConnection::HeartbeatThreadFunc, this);
    if (m_heartbeatThread.joinable()) {
        NppSSH_LogInfoAuto("✅ 心跳线程启动成功");
    }
    else {
        NppSSH_LogInfoAuto("❌ 心跳线程启动失败");
    }
}


// 断开连接
void SSHConnection::Disconnect() {
    StopShellReader();
    ReleaseResources();
    m_connected.store(false, std::memory_order_release);
}

// 重置状态
void SSHConnection::ResetState() {
    // 标记取消连接
    m_cancelConnect.store(true, std::memory_order_release);

    std::lock_guard<std::mutex> lock(m_mutex);

    // 停止心跳
    m_stopHeartbeat.store(true, std::memory_order_release);
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.detach(); // 改用detach，避免join阻塞导致死锁
    }

    // 清理连接线程
    if (m_pConnectThread) {
        delete m_pConnectThread;
        m_pConnectThread = nullptr;
    }

    // 重置状态
    m_host.clear();
    m_user.clear();
    m_pass.clear();
    m_port = 22;
    m_connected.store(false, std::memory_order_release);
    m_connecting.store(false, std::memory_order_release);
    m_session = nullptr;
    m_sock = INVALID_SOCKET;
}
// 启动后台持续读（官方poll）
// ========== 改造StartShellReader：仅执行命令时启动 ==========
void SSHConnection::StartShellReader() {
    std::lock_guard<std::mutex> lock(m_readerMutex);
    // 前置检查：通道无效/已有运行线程 → 直接返回
    if ((m_shellChannel.load(std::memory_order_acquire) == nullptr) || m_shellReaderThread.joinable()) {
        NppSSH_LogWarnAuto("【WARN】StartShellReader 跳过：通道无效或线程已运行");
        return;
    }

    // 重置线程控制状态
    m_stopReader.store(false, std::memory_order_release);
    m_waitingForPrompt.store(true, std::memory_order_release);// 标记"等待提示符"

    m_isReadingOutput.store(true, std::memory_order_release);
    m_prompt.clear(); // 清空旧提示符

    // 启动线程
    m_shellReaderThread = std::thread(&SSHConnection::ShellReaderLoop, this);
    NppSSH_LogInfoAuto("【OK】ShellReader 线程启动（仅本次命令）");
}

// 停止后台读
void SSHConnection::StopShellReader() {
    std::lock_guard<std::mutex> lock(m_readerMutex);
    m_stopReader.store(true, std::memory_order_release);
    m_waitingForPrompt.store(false, std::memory_order_release);// 取消等待提示符
    if (m_shellChannel.load(std::memory_order_acquire) == nullptr) {//通道为空直接放弃，不操作
        m_shellReaderThread = std::thread();
        return;
    }
    if (m_shellReaderThread.joinable()) {
        try {
            // 正确的线程等待方式：直接join（无超时）
            m_shellReaderThread.join();
        }
        catch (...) {
            // 如果join失败，强制分离
            m_shellReaderThread.detach();
            NppSSH_LogErrorAuto("【ERROR】ShellReader 线程join失败，强制分离");
        }
    }

    // 重置线程对象
    m_shellReaderThread = std::thread();
    NppSSH_LogInfoAuto("【OK】ShellReader 线程已停止，最终提示符：[" + (m_prompt.empty() ? "空" : m_prompt) + "]");
}


// 伪终端线程，后台无限流
void SSHConnection::ShellReaderLoop() {
    NppSSH_LogInfoAuto("==============================================");
    NppSSH_LogInfoAuto("=        ShellReaderLoop 线程运行中          =");
    NppSSH_LogInfoAuto("==============================================");
    NppSSH_LogInfoAuto("【ShellReaderLoop 正式运行】m_prompt = " + std::string(m_prompt));

    char buf[SSHConst::BUF_SIZE_LARGE];
    const int IDLE_MS = 10;
    // 延长超时（兼容长命令/脚本执行）
    const int MAX_IDLE_COUNT = 1200; // 12秒超时兜底
    int idleCount = 0;
    // 输出到面板
    int panelId = SSHConnection_GetPanelId(this);
    // 仅在"等待提示符"且连接有效时运行
    while (m_waitingForPrompt.load(std::memory_order_acquire) && m_connected.load(std::memory_order_acquire) && (m_shellChannel.load(std::memory_order_acquire) != nullptr) && !m_stopReader.load(std::memory_order_acquire)) {
        if (!m_connected.load(std::memory_order_acquire) || m_stopReader.load(std::memory_order_acquire)) break;

        int n = libssh2_channel_read(
            m_shellChannel.load(std::memory_order_acquire),
            buf,
            sizeof(buf) - 1,
            LIBSSH2_CHANNEL_EXTENDED_DATA_NORMAL
        );

        if (n > 0) {
            buf[n] = 0;
            std::string chunk(buf);

            if (!m_currentCommand.empty()) {
                // 只匹配【命令本身】，不匹配后面的换行
                std::string cmd = m_currentCommand;
                size_t firstNewline = chunk.find_first_of("\r\n");
                std::string firstLine = (firstNewline != std::string::npos) ? chunk.substr(0, firstNewline) : chunk;
                // 仅当首行完全匹配命令时，才过滤首行（保留后续内容）
                if (firstLine == cmd) {
                    chunk = (firstNewline != std::string::npos) ? chunk.substr(firstNewline + 1) : "";
                    NppSSH_LogInfoAuto("【过滤】仅移除首行命令回显，保留结果内容");
                }

                // 只过滤一次
                m_currentCommand.clear();
            }


            
            if (panelId >= 0) {
                SSH_AppendOutputText(panelId, chunk);
                SSH_SetIsCommandRunning(panelId, true);
            }

            // 重置空闲计数
            idleCount = 0;

            // 提取最后一行作为提示符（空行也直接赋值，不处理）
            std::string lastLine = extractLastLine(chunk);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_prompt = lastLine; // 空行直接赋值，不做任何过滤
            }
            // 如果输出了大量内容（脚本运行中），且连续空行，允许正常退出
            static int scriptEmptyLineCount = 0;
            if (m_prompt.empty() && chunk.empty()) { // 仅当chunk为空且提示符为空时计数
                scriptEmptyLineCount++;
                // 延长空行阈值（从5→20，对应2秒），且仅当无任何输出时才触发
                if (scriptEmptyLineCount >= 20) {
                    NppSSH_LogInfoAuto("【脚本执行完成】连续空行且无输出，自动结束读取");
                    m_stopReader.store(true, std::memory_order_release);
                    m_waitingForPrompt.store(false, std::memory_order_release);

                    scriptEmptyLineCount = 0;
                    break;
                }
            }
            else {
                scriptEmptyLineCount = 0; // 有任何输出就重置
            }

            NppSSH_LogInfoAuto(chunk+"【更新提示符】=> [" + (m_prompt.empty() ? "空" : m_prompt) + "]");
            // 核心逻辑：检测到非空提示符 → 停止线程
            if (!m_prompt.empty()) {
                // 优化：等待500ms确认输出稳定，避免提前终止
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                int n_check = libssh2_channel_read(m_shellChannel.load(std::memory_order_acquire), buf, sizeof(buf) - 1, LIBSSH2_CHANNEL_EXTENDED_DATA_NORMAL);
                if (n_check <= 0) {
                    NppSSH_LogInfoAuto("【检测到稳定提示符】停止ShellReader线程");
                    m_stopReader.store(true, std::memory_order_release);
                    m_waitingForPrompt.store(false, std::memory_order_release);


                    break;
                }
                else {
                    // 读取剩余输出
                    buf[n_check] = 0;
                    std::string remaining(buf);
                    SSH_AppendOutputText(panelId, remaining);
                    m_prompt = extractLastLine(remaining); // 更新提示符
                    scriptEmptyLineCount = 0;
                }
            }

           

        }
        else if (n == 0) {
            // 无数据，短暂休眠
            Sleep(IDLE_MS);
            idleCount++;

            // 超时兜底：即使无提示符，也停止线程（避免永久阻塞）
            if (idleCount >= MAX_IDLE_COUNT) {
                NppSSH_LogInfoAuto("【ShellReader 超时】12秒无输出，强制停止线程");
                m_stopReader.store(true, std::memory_order_release);
                m_waitingForPrompt.store(false, std::memory_order_release);
                break;
            }
        }
        else {
            // 处理libssh2错误码
            int err = libssh2_session_last_errno(m_session);
            std::string errDesc;
            switch (err) {
            case LIBSSH2_ERROR_TIMEOUT:
                errDesc = "LIBSSH2_ERROR_TIMEOUT(-9)：套接字发送失败";
                break;
            case LIBSSH2_ERROR_SOCKET_RECV:
                errDesc = "LIBSSH2_ERROR_SOCKET_RECV(-8)：套接字接收失败";
                break;
            case LIBSSH2_ERROR_EAGAIN:
                errDesc = "LIBSSH2_ERROR_EAGAIN(-37)：无数据，需重试";
                break;
            case LIBSSH2_ERROR_BAD_USE:
                errDesc = "LIBSSH2_ERROR_BAD_USE(-78)：API 使用错误";
                break;
            default:
                errDesc = "未知错误码：" + std::to_string(err);
            }
            NppSSH_LogInfoAuto("【错误】" + errDesc);

            // 仅非阻塞错误继续等待，其他错误停止线程
            if (err == LIBSSH2_ERROR_EAGAIN || err == LIBSSH2_ERROR_BAD_USE) {
                Sleep(IDLE_MS);
                continue;
            }
            else {
                NppSSH_LogInfoAuto("【致命错误】停止ShellReader线程");
                m_stopReader.store(true, std::memory_order_release);
                m_waitingForPrompt.store(false, std::memory_order_release);
                break;
            }
        }
    }
    if (panelId >= 0) {
        NppSSH_LogInfoAuto("【ShellReaderLoop 线程退出】调用SSHTerminal_PanelPrompt函数赋值私有成员变量 _prompt = "+ m_prompt);
        SSH_PanelPrompt(panelId, m_prompt);
        SSH_SetIsCommandRunning(panelId, false);
    }

    NppSSH_LogInfoAuto("【ShellReaderLoop 线程退出】最终提示符：[" + (m_prompt.empty() ? "空" : m_prompt) + "]");
}
// 伪终端执行命令（终极纯净版）
bool SSHConnection::ExecuteCommand(const std::string& cmd) {
    NppSSH_LogInfoAuto("【ExecuteCommand 执行】命令 = " + cmd);

    // 前置检查：未连接/通道无效 → 返回失败
    if (!m_connected.load(std::memory_order_acquire) || (m_shellChannel.load(std::memory_order_acquire) == nullptr)) {
        NppSSH_LogErrorAuto("【ERROR】ExecuteCommand 失败：未连接或通道无效");
        return false;
    }
    // 保存当前命令，用于过滤回显
    m_currentCommand = cmd;
    // 1. 停止旧线程（防止残留）
    StopShellReader();

    // 2. 启动本次命令的ShellReader线程
    StartShellReader();

    // 3. 发送命令
    std::string command = cmd + "\n";
    int writeRet = libssh2_channel_write(m_shellChannel.load(std::memory_order_acquire), command.c_str(), command.length());
    if (writeRet < 0) {
        NppSSH_LogErrorAuto("【ERROR】命令发送失败：" + std::to_string(writeRet));
        StopShellReader(); // 发送失败立即停止线程
        return false;
    }
    NppSSH_LogInfoAuto("【OK】命令发送完成，等待提示符...");

    // 4. （可选）等待线程自动停止（或业务层按需处理）
    // 如需同步等待：while(m_waitingForPrompt.load(std::memory_order_acquire)) { Sleep(10); }

    return true;
}

// 获取提示符
std::string SSHConnection::GetPrompt() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_prompt.empty() ? "" : m_prompt;
}

// 初始化WSA
bool SSHConnection::InitWSA(WSADATA& wsaData) {
    int wsaRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaRet != 0) {
        NppSSH_LogErrorAuto("WSA初始化失败，错误码：" + std::to_string(wsaRet));
        return false;
    }
    return true;
}

// 创建并连接Socket
SOCKET SSHConnection::CreateAndConnectSocket(const std::string& host, int port, std::string& errorMsg) {
    const int MAX_RETRIES = 3;               // 最大重试次数
    const int BASE_WAIT_MS = 1000;          // 基础等待时间1秒

    SOCKET final_socket = INVALID_SOCKET;
    std::string last_error = "";
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        // 清空前一次的错误信息
        errorMsg.clear();

        // 打印重试日志
        if (attempt > 1) {
            int wait_time = BASE_WAIT_MS * (1 << (attempt - 2)); // 指数退避：1秒, 2秒, 4秒
            NppSSH_LogInfoAuto("Socket连接重试 " + std::to_string(attempt) + "/" +
                std::to_string(MAX_RETRIES) + "：等待 " +
                std::to_string(wait_time) + "ms 后重试...");
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        }

        NppSSH_LogInfoAuto("Socket连接尝试 " + std::to_string(attempt) + "/" +
            std::to_string(MAX_RETRIES) + "：正在连接 " +
            host + ":" + std::to_string(port));

        // 1. 创建Socket
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            int err = WSAGetLastError();
            errorMsg = "Socket创建失败（错误码：" + std::to_string(err) + "）";
            NppSSH_LogErrorAuto(errorMsg);
            last_error = errorMsg;
            continue;  // 继续下一次重试
        }

        // 2. 域名/IP解析
        addrinfo hints = { 0 };
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        int ret = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
        if (ret != 0 || !result) {
            errorMsg = "IP解析失败（错误码：" + std::to_string(ret) + "）：" + host + ":" + std::to_string(port);
            NppSSH_LogErrorAuto(errorMsg);
            closesocket(sock);
            last_error = errorMsg;
            continue;  // 继续下一次重试
        }

        // 3. 设置非阻塞模式
        u_long nonblock = 1;
        ioctlsocket(sock, FIONBIO, &nonblock);

        // 4. 非阻塞连接
        int connectRet = connect(sock, result->ai_addr, (int)result->ai_addrlen);
        freeaddrinfo(result);

        // 5. 处理非阻塞连接的立即失败
        if (connectRet == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                errorMsg = "Socket连接立即失败（错误码：" + std::to_string(err) + "）";
                NppSSH_LogErrorAuto(errorMsg);
                closesocket(sock);
                last_error = errorMsg;
                continue;  // 继续下一次重试
            }
        }

        // 6. Select超时检测（原有的select逻辑完全不变）
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sock, &wfds);

        timeval tv = { 0 };
        tv.tv_sec = SSHConst::CONNECT_SOCKET_TIMEOUT_MS / 1000;
        tv.tv_usec = (SSHConst::CONNECT_SOCKET_TIMEOUT_MS % 1000) * 1000;

        int select_ret = select(0, nullptr, &wfds, nullptr, &tv);
        if (select_ret <= 0) {
            errorMsg = "Socket连接超时（" + std::to_string(SSHConst::CONNECT_SOCKET_TIMEOUT_MS) + "ms）：" + host + ":" + std::to_string(port);
            NppSSH_LogErrorAuto(errorMsg);
            closesocket(sock);
            last_error = errorMsg;
            continue;  // 继续下一次重试
        }

        // 7. 检查连接结果
        int err_code = 0;
        int len = sizeof(err_code);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err_code, &len);
        if (err_code != 0) {
            errorMsg = "Socket连接失败（错误码：" + std::to_string(err_code) + "）";
            if (err_code == 10061) errorMsg += "（端口未开放/拒绝连接）";
            if (err_code == 10065) errorMsg += "（网络不可达）";
            NppSSH_LogErrorAuto(errorMsg);
            closesocket(sock);
            last_error = errorMsg;
            continue;  // 继续下一次重试
        }

        // 8. 恢复阻塞模式
        nonblock = 0;
        ioctlsocket(sock, FIONBIO, &nonblock);

        // 连接成功
        NppSSH_LogInfoAuto("✓ Socket连接成功！总尝试次数：" + std::to_string(attempt));
        return sock;
    }

    // 所有重试都失败了
    errorMsg = "Socket连接在 " + std::to_string(MAX_RETRIES) + " 次重试后仍然失败";
    if (!last_error.empty()) {
        errorMsg += "，最后错误：" + last_error;
    }
    NppSSH_LogErrorAuto(errorMsg);
    return INVALID_SOCKET;
}

// 初始化SSH会话并握手（带加密算法优化和指数退避重试）
// 初始化SSH会话并握手（优化版本：指数退避重试 + 多种算法配置）
LIBSSH2_SESSION* SSHConnection::InitSSHSession(SOCKET sock, const std::string& host, int port, std::string& errorMsg) {
    const int MAX_RETRIES = 3;               // 每个算法配置的最大重试次数
    const int BASE_WAIT_MS = 1000;          // 基础等待时间1秒

    // 初始化libssh2
    if (libssh2_init(0) != 0) {
        errorMsg = "libssh2初始化失败";
        NppSSH_LogErrorAuto(errorMsg);
        return nullptr;
    }

    // 检查socket状态
    if (!IsSocketAlive(sock)) {
        errorMsg = "Socket在SSH握手前已断开";
        NppSSH_LogErrorAuto(errorMsg);
        return nullptr;
    }

    // 记录开始时间
    auto start_time = std::chrono::steady_clock::now();
    LIBSSH2_SESSION* session = nullptr;

    // 定义多种算法配置，从简单到复杂
    struct AlgorithmConfig {
        const char* name;
        const char* kex_algorithms;
        const char* ciphers;
        int timeout_ms;  // 该配置的超时时间
    };

    std::vector<AlgorithmConfig> algorithm_configs = {
        // 配置1：最广兼容（老设备 + 老OpenSSH，group1+group14-sha1）
        {
            "广兼容(KEX老)",
            "diffie-hellman-group1-sha1,diffie-hellman-group14-sha1,"
            "diffie-hellman-group14-sha256,diffie-hellman-group-exchange-sha256,"
            "curve25519-sha256,curve25519-sha256@libssh.org,"
            "ecdh-sha2-nistp256,ecdh-sha2-nistp384,ecdh-sha2-nistp521",
            "aes128-cbc,3des-cbc,aes256-cbc,aes128-ctr,aes256-ctr",
            15000
        },
        // 配置2：标准（大多数服务器，优先现代算法）
        {
            "标准(KEX混合)",
            "curve25519-sha256,curve25519-sha256@libssh.org,"
            "ecdh-sha2-nistp256,ecdh-sha2-nistp384,ecdh-sha2-nistp521,"
            "diffie-hellman-group-exchange-sha256,"
            "diffie-hellman-group16-sha512,diffie-hellman-group14-sha256,"
            "diffie-hellman-group14-sha1",
            "aes256-ctr,aes192-ctr,aes128-ctr,aes256-cbc,aes128-cbc,3des-cbc",
            10000
        },
        // 配置3：现代（新服务器，不含弱算法）
        {
            "现代(KEX强)",
            "curve25519-sha256,curve25519-sha256@libssh.org,"
            "ecdh-sha2-nistp256,ecdh-sha2-nistp384,ecdh-sha2-nistp521,"
            "diffie-hellman-group-exchange-sha256,"
            "diffie-hellman-group16-sha512,diffie-hellman-group18-sha512,"
            "diffie-hellman-group14-sha256",
            "chacha20-poly1305@openssh.com,aes256-ctr,aes128-ctr,aes256-cbc,aes128-cbc",
            10000
        }
    };

    int total_attempts = 0;
    bool handshake_success = false;

    // 尝试不同的算法配置
    for (size_t config_idx = 0; config_idx < algorithm_configs.size() && !handshake_success; config_idx++) {
        const auto& config = algorithm_configs[config_idx];
        NppSSH_LogInfoAuto("尝试算法配置: " + std::string(config.name));

        // 检查总耗时
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        if (elapsed_ms > 30000) {  // 30秒总超时
            errorMsg = "SSH握手总耗时超过30秒，放弃尝试";
            NppSSH_LogErrorAuto(errorMsg);
            break;
        }

        // 对当前配置进行指数退避重试
        for (int retry = 1; retry <= MAX_RETRIES && !handshake_success; retry++) {
            total_attempts++;

            // 指数退避等待
            if (retry > 1) {
                int wait_time = BASE_WAIT_MS * (1 << (retry - 2)); // 1秒, 2秒, 4秒
                NppSSH_LogInfoAuto("SSH握手尝试 " + std::to_string(total_attempts) +
                    " (配置: " + config.name +
                    ", 重试: " + std::to_string(retry) + "/" + std::to_string(MAX_RETRIES) +
                    ")：等待 " + std::to_string(wait_time) + "ms 后重试...");

                // 等待期间检查socket状态
                if (!IsSocketAlive(sock)) {
                    errorMsg = "Socket在等待期间失效";
                    NppSSH_LogErrorAuto(errorMsg);
                    if (session) {
                        libssh2_session_free(session);
                        session = nullptr;
                    }
                    return nullptr;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
            }
            else {
                NppSSH_LogInfoAuto("SSH握手尝试 " + std::to_string(total_attempts) +
                    " (配置: " + config.name +
                    ", 首次尝试)");
            }

            // 清理之前的会话
            if (session) {
                libssh2_session_free(session);
                session = nullptr;
            }

            // 创建新会话
            session = libssh2_session_init();
            if (!session) {
                errorMsg = "libssh2_session_init失败";
                NppSSH_LogWarnAuto(errorMsg);
                continue;  // 继续下一次重试
            }

            // 设置会话参数
            // 1. 设置banner（有些服务器对banner有要求）
            libssh2_session_banner_set(session, "SSH-2.0-NppSSH_Client");

            // 2. 设置超时（不同配置使用不同超时）
            libssh2_session_set_timeout(session, config.timeout_ms);

            // 3. 设置为阻塞模式
            libssh2_session_set_blocking(session, 1);

            // 4. 设置算法偏好
            NppSSH_LogInfoAuto("设置密钥交换算法: " + std::string(config.kex_algorithms));
            libssh2_session_method_pref(session, LIBSSH2_METHOD_KEX, config.kex_algorithms);

            NppSSH_LogInfoAuto("设置加密算法: " + std::string(config.ciphers));
            libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_CS, config.ciphers);
            libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_SC, config.ciphers);

            // 5. 设置MAC算法和压缩算法为最简单
            libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_CS, "hmac-sha1");
            libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_SC, "hmac-sha1");
            libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_CS, "none");
            libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_SC, "none");

            // 6. 在握手前再次检查socket
            if (!IsSocketAlive(sock)) {
                errorMsg = "Socket在握手前失效";
                NppSSH_LogErrorAuto(errorMsg);
                libssh2_session_free(session);
                session = nullptr;
                continue;  // 继续重试
            }

            // 尝试SSH握手
            NppSSH_LogInfoAuto("开始SSH握手...");
            auto handshake_start = std::chrono::steady_clock::now();

            int handshake_ret = libssh2_session_handshake(session, sock);

            auto handshake_end = std::chrono::steady_clock::now();
            auto handshake_ms = std::chrono::duration_cast<std::chrono::milliseconds>(handshake_end - handshake_start).count();
            NppSSH_LogInfoAuto("握手耗时: " + std::to_string(handshake_ms) + "ms");

            if (handshake_ret == 0) {
                // 握手成功
                std::string negotiated_kex = libssh2_session_methods(session, LIBSSH2_METHOD_KEX);
                std::string negotiated_cipher = libssh2_session_methods(session, LIBSSH2_METHOD_CRYPT_CS);

                NppSSH_LogInfoAuto("✓ SSH握手成功：" + host + ":" + std::to_string(port));
                NppSSH_LogInfoAuto("  使用算法配置: " + std::string(config.name));
                NppSSH_LogInfoAuto("  协商的KEX算法: " + negotiated_kex);
                NppSSH_LogInfoAuto("  协商的加密算法: " + negotiated_cipher);
                NppSSH_LogInfoAuto("  总尝试次数: " + std::to_string(total_attempts));
                NppSSH_LogInfoAuto("  总耗时: " + std::to_string(elapsed_ms) + "ms");

                handshake_success = true;
                return session;  // 成功，直接返回
            }
            else {
                // 握手失败
                errorMsg = GetLibssh2ErrorMsg(session) +
                    "（错误码：" + std::to_string(handshake_ret) + "）";

                // 获取详细错误
                char* err_msg = nullptr;
                libssh2_session_last_error(session, &err_msg, nullptr, 0);
                if (err_msg && strlen(err_msg) > 0) {
                    NppSSH_LogErrorAuto("libssh2详细错误: " + std::string(err_msg));

                    // 针对"Failed getting banner"错误的特殊处理
                    if (strstr(err_msg, "Failed getting banner") != nullptr) {
                        NppSSH_LogWarnAuto("⚠️ 检测到banner获取失败，可能原因：");
                        NppSSH_LogWarnAuto("  1. 服务器未正确响应");
                        NppSSH_LogWarnAuto("  2. 网络连接不稳定");
                        NppSSH_LogWarnAuto("  3. 防火墙/代理问题");
                    }
                }

                // 检查错误类型
                if (handshake_ret == LIBSSH2_ERROR_SOCKET_DISCONNECT ||
                    handshake_ret == LIBSSH2_ERROR_SOCKET_TIMEOUT) {
                    NppSSH_LogWarnAuto("⚠️ 网络连接问题，将尝试下一个配置");
                    // 网络问题，跳出当前配置的循环，尝试下一个配置
                    break;
                }

                if (handshake_ret == LIBSSH2_ERROR_PROTO) {
                    NppSSH_LogWarnAuto("⚠️ 协议错误，将尝试下一个配置");
                    // 协议错误，跳出当前配置的循环
                    break;
                }

                if (retry < MAX_RETRIES) {
                    NppSSH_LogInfoAuto("握手失败，将指数退避后重试...");
                }
                else {
                    NppSSH_LogInfoAuto("当前算法配置达到最大重试次数，将尝试下一个配置...");
                }

                // 清理当前会话
                libssh2_session_free(session);
                session = nullptr;
            }
        }

        // 清理当前会话（如果还存在）
        if (session) {
            libssh2_session_free(session);
            session = nullptr;
        }
    }

    // 如果执行到这里，说明所有算法配置都失败了
    if (session) {
        libssh2_session_free(session);
        session = nullptr;
    }

    // 详细错误诊断
    NppSSH_LogErrorAuto("❌ SSH握手最终失败");
    NppSSH_LogErrorAuto("诊断信息：");
    NppSSH_LogErrorAuto("  目标: " + host + ":" + std::to_string(port));
    NppSSH_LogErrorAuto("  总尝试次数: " + std::to_string(total_attempts));
    NppSSH_LogErrorAuto("  最后错误: " + errorMsg);
    NppSSH_LogErrorAuto("  总耗时: " +
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count()) + "ms");

    // 提供可能的解决方案
    NppSSH_LogErrorAuto("建议检查：");
    NppSSH_LogErrorAuto("  1. 服务器SSH服务是否正常运行（netstat -an | grep :22）");
    NppSSH_LogErrorAuto("  2. 防火墙是否允许SSH连接");
    NppSSH_LogErrorAuto("  3. 服务器是否配置了AllowUsers/AllowGroups限制");
    NppSSH_LogErrorAuto("  4. 网络连接是否稳定");

    return nullptr;
}

// SSH密码认证
bool SSHConnection::AuthenticateSSH(LIBSSH2_SESSION* session, const std::string& user, const std::string& pass, std::string& errorMsg) {
    if (!session) {
        errorMsg = "无效的SSH会话";
        NppSSH_LogErrorAuto(errorMsg);
        return false;
    }

    libssh2_session_set_timeout(session, SSHConst::SSH_AUTH_TIMEOUT_MS);
    int authRet = libssh2_userauth_password(session, user.c_str(), pass.c_str());

    if (authRet != 0) {
        errorMsg = "SSH密码认证失败，用户：" + user + "，错误：" + GetLibssh2ErrorMsg(session);
        NppSSH_LogErrorAuto(errorMsg);
        return false;
    }

    NppSSH_LogInfoAuto("SSH认证成功：用户=" + user);
    return true;
}

// 读取登录Banner和登录时间
void SSHConnection::ReadLoginBanner(LIBSSH2_SESSION* session) {
    if (!session) { // 必须有伪终端通道才继续
        return;
    }
    if (m_shellChannel.load(std::memory_order_acquire) == nullptr) {
        NppSSH_LogInfoAuto("【没有伪终端】");
        return;
    }
    //配置home
    //std::string homeDir = GetHomeDir();
    //m_dirFile = homeDir;
    //NppSSH_LogInfoAuto("当前登录用户的home完整路径1===="+homeDir);
    //NppSSH_LogInfoAuto("当前登录用户的home完整路径2===="+ m_homeDir);
    int panelId = SSHConnection_GetPanelId(this);
    // 读取Banner
    std::string loginBanner = "\r\n";
    const char* banner = libssh2_session_banner_get(session);
    if (banner) {
        loginBanner += banner;
        loginBanner += "\r\n";
    }
    
    // 获取登录时间  使用 伪终端 m_shellChannel 
    std::string currentLoginTime;
    char buf[SSHConst::BUF_SIZE_MEDIUM] = { 0 };
    int bytesRead = 0;
    // 读取伪终端输出（获取 Last login 信息）
    while ((bytesRead = libssh2_channel_read(
        m_shellChannel.load(std::memory_order_acquire),
        buf,
        sizeof(buf) - 1,
        LIBSSH2_CHANNEL_EXTENDED_DATA_NORMAL
    )) > 0)
    {
        buf[bytesRead] = 0;
        currentLoginTime += buf;
        memset(buf, 0, sizeof(buf));
    }
    // 将获取到的登录信息拼接到欢迎语
    if (!currentLoginTime.empty()) {
        loginBanner += currentLoginTime;
    }
    m_prompt = extractLastLine(currentLoginTime);
    if (m_prompt.empty()) {
        NppSSH_LogInfoAuto("【注意】===========提示词为空");
        m_prompt = "";
    }

    // 备用：本地时间
    if (loginBanner.find("Last login:") == std::string::npos) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char localTime[64] = { 0 };
        sprintf_s(localTime, "Last login: %04d-%02d-%02d %02d:%02d:%02d (本地时间)",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        loginBanner += localTime;
        loginBanner += "\r\n";
        loginBanner += m_prompt;
    }
    
    if (panelId >= 0) {
        SSH_AppendOutputText(panelId, loginBanner);
        NppSSH_LogInfoAuto("【欢迎语获取退出】调用SSHTerminal_PanelPrompt函数赋值私有成员变量 _prompt = " + m_prompt);
        SSH_PanelPrompt(panelId, m_prompt);
        SSH_SetIsCommandRunning(panelId, false);
    }
}

bool SSHConnection::CreatePtyChannel() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_session || m_sock == INVALID_SOCKET) {
        NppSSH_LogErrorAuto("【CreatePtyChannel】会话/Socket无效");
        return false;
    }

    //一共尝试15次，总尝试次数*每次操作最大等待次数 = 5*3
    const int MAX_TOTAL_ATTEMPTS = 5;          // 总尝试次数
    const int MAX_WAIT_ATTEMPTS = 3;         // 每次操作最大等待次数
    const int BASE_WAIT_MS = 1000;           // 基础等待时间
    const int MAX_WAIT_MS = 10000;           // 最大等待时间

    // 每个类型尝试3次，可直接改变TERMINAL_TYPES的内容，不影响任何重试逻辑
    const std::vector<std::string> TERMINAL_TYPES = {
        "xterm-256color", "xterm", "vt100", "dumb", "linux"
    };

    // 第一步：创建通道
    LIBSSH2_CHANNEL* channel = nullptr;

    for (int attempt = 1; attempt <= MAX_TOTAL_ATTEMPTS; attempt++) {
        NppSSH_LogInfoAuto("【CreatePtyChannel】尝试创建通道 (" +
            std::to_string(attempt) + "/" +
            std::to_string(MAX_TOTAL_ATTEMPTS) + ")");

        // 尝试创建通道
        channel = libssh2_channel_open_session(m_session);

        if (channel) {
            NppSSH_LogInfoAuto("【CreatePtyChannel】通道创建成功");
            break;
        }

        int last_err = libssh2_session_last_errno(m_session);

        if (last_err == LIBSSH2_ERROR_EAGAIN) {
            // Socket正忙，需要等待
            NppSSH_LogInfoAuto("【CreatePtyChannel】通道创建EAGAIN，等待socket...");

            // 计算线性和指数退避等待时间
            // 原理：等待时间 = 重试次数 * 基本等待时间，如果等待时间大于最大等待时间，则直接用最大等待时间
            int wait_time;
            if (attempt <= 3) {
                wait_time = BASE_WAIT_MS * attempt;  // 前3次线性
            }
            else {
                wait_time = std::min(BASE_WAIT_MS * (1 << (attempt - 3)), MAX_WAIT_MS);  // 后面指数
            }
            if (WaitSocketWithBackoff(m_sock, m_session, wait_time, MAX_WAIT_ATTEMPTS)) {
                // 等待成功，继续下一次尝试
                continue;
            }
            else {
                NppSSH_LogErrorAuto("【CreatePtyChannel】等待socket超时，继续尝试...");

                if (attempt == MAX_TOTAL_ATTEMPTS) {
                    NppSSH_LogErrorAuto("【CreatePtyChannel】达到最大尝试次数，通道创建失败");
                    return false;
                }
            }
        }
        else {
            // 其他错误
            std::string err = "通道创建失败: " + GetLibssh2ErrorMsg(m_session) +
                " (错误码: " + std::to_string(last_err) + ")";
            NppSSH_LogErrorAuto(err);

            if (attempt == MAX_TOTAL_ATTEMPTS) {
                return false;
            }

            // 如果不是致命错误，可以重试
            int wait_time = BASE_WAIT_MS * attempt;
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        }
    }

    if (!channel) {
        NppSSH_LogErrorAuto("【CreatePtyChannel】最终通道创建失败");
        return false;
    }

    // 第二步：设置PTY伪终端
    std::string used_terminal = "";
    bool pty_success = false;

    for (const auto& term_type : TERMINAL_TYPES) {
        NppSSH_LogInfoAuto("【CreatePtyChannel】尝试PTY终端类型: " + term_type);

        for (int attempt = 1; attempt <= MAX_WAIT_ATTEMPTS; attempt++) {
            int ret = libssh2_channel_request_pty(
                channel,
                term_type.c_str(),
                nullptr,     // 终端模式（默认）
                80, 24,      // 行列数
                0, 0         // 像素宽高（忽略）
            );

            if (ret == 0) {
                // PTY设置成功
                pty_success = true;
                used_terminal = term_type;
                NppSSH_LogInfoAuto("【CreatePtyChannel】PTY终端类型 " + term_type + " 设置成功");
                break;
            }
            else if (ret == LIBSSH2_ERROR_EAGAIN) {
                NppSSH_LogInfoAuto("【CreatePtyChannel】PTY设置EAGAIN，等待socket (" +
                    std::to_string(attempt) + "/" +
                    std::to_string(MAX_WAIT_ATTEMPTS) + ")");

                int wait_time = BASE_WAIT_MS * attempt;
                if (WaitSocketWithBackoff(m_sock, m_session, wait_time, 1)) {
                    // 等待后继续重试
                    continue;
                }
                else {
                    NppSSH_LogWarnAuto("【CreatePtyChannel】PTY等待超时，尝试下一个终端类型");
                    break;
                }
            }
            else {
                // 其他错误，记录并尝试下一个终端类型
                std::string err_msg = GetLibssh2ErrorMsg(m_session);
                NppSSH_LogInfoAuto("【CreatePtyChannel】PTY终端类型 " + term_type +
                    " 失败: " + err_msg + " (错误码: " + std::to_string(ret) + ")");
                break;
            }
        }

        if (pty_success) {
            break;
        }
    }

    if (!pty_success) {
        std::string err = "所有PTY终端类型设置失败: " + GetLibssh2ErrorMsg(m_session);
        NppSSH_LogErrorAuto(err);
        libssh2_channel_free(channel);
        return false;
    }

    // 第三步：启动shell
    NppSSH_LogInfoAuto("【CreatePtyChannel】准备启动shell...");

    for (int attempt = 1; attempt <= MAX_TOTAL_ATTEMPTS; attempt++) {
        int ret = libssh2_channel_shell(channel);

        if (ret == 0) {
            // Shell启动成功
            m_shellChannel.store(channel, std::memory_order_release);

            std::string log_msg = "【CreatePtyChannel】伪终端创建成功";
            if (!used_terminal.empty()) {
                log_msg += " (终端类型: " + used_terminal + ")";
            }
            NppSSH_LogInfoAuto(log_msg);
            return true;
        }
        else if (ret == LIBSSH2_ERROR_EAGAIN) {
            NppSSH_LogInfoAuto("【CreatePtyChannel】Shell启动EAGAIN，等待socket (" +
                std::to_string(attempt) + "/" +
                std::to_string(MAX_TOTAL_ATTEMPTS) + ")");

            int wait_time = std::min(BASE_WAIT_MS * attempt, MAX_WAIT_MS);

            if (WaitSocketWithBackoff(m_sock, m_session, wait_time, MAX_WAIT_ATTEMPTS)) {
                // 等待后继续尝试
                continue;
            }
            else {
                NppSSH_LogWarnAuto("【CreatePtyChannel】Shell启动等待超时");

                if (attempt == MAX_TOTAL_ATTEMPTS) {
                    NppSSH_LogErrorAuto("【CreatePtyChannel】Shell启动达到最大尝试次数");
                    break;
                }
            }
        }
        else {
            // 其他错误
            std::string err = "Shell启动失败: " + GetLibssh2ErrorMsg(m_session) +
                " (错误码: " + std::to_string(ret) + ")";
            NppSSH_LogErrorAuto(err);
            break;
        }
    }

    // 启动shell失败
    std::string err = "Shell启动最终失败: " + GetLibssh2ErrorMsg(m_session);
    NppSSH_LogErrorAuto(err);
    libssh2_channel_free(channel);
    return false;
}
bool SSHConnection::Connect(const char* host, int port, const char* user, const char* pass) {
    //NppSSH_LogInfoAuto("开始进行连接==========1");

    if (m_connected.load(std::memory_order_acquire)) {
        NppSSH_LogInfoAuto("面板已处于连接状态，无需重复连接");
        return true;
    }

    try {
        // 创建promise/future，用于获取异步连接结果
        std::promise<bool> connPromise;
        std::future<bool> connFuture = connPromise.get_future();

        // 调用异步连接函数（传入promise）
        //NppSSH_LogInfoAuto("调用ConnectAsync进入异步连接核心逻辑");
        ConnectAsync(host, port, user, pass, std::move(connPromise));
        std::future_status status = connFuture.wait_for(std::chrono::seconds(SSHConst::MAX_MAIN_THREAD_WAIT_MS)); // 30秒超时
        if (status == std::future_status::ready) {
            m_connected.store(connFuture.get(), std::memory_order_release);
            return m_connected.load(std::memory_order_acquire);
        }
        else {
            NppSSH_LogErrorAuto("连接超时（30秒），终止连接");
            return false;
        }
    }
    catch (const std::exception& e) {
        NppSSH_LogErrorAuto(std::string("连接过程异常：") + e.what());
        return false;
    }
}
// 异步连接核心逻辑（线程执行体）
void SSHConnection::ConnectAsync(const char* host, int port, const char* user, const char* pass, std::promise<bool> promise) {
    bool ok = false;
    std::string err;
    NppSSH_LogInfoAuto("进入异步连接核心逻辑");
    auto guard = [&]() {
        try { promise.set_value(ok); }
        catch (...) {}
        };

    try {
        // 步骤1：参数赋值
        std::string l_host = host ? host : "";
        int l_port = (port >= 1 && port <= 65535) ? port : 22;
        std::string l_user = user ? user : "";
        std::string l_pass = pass ? pass : "";

        NppSSH_LogInfoAuto("步骤1：参数已接收 host=" + l_host + " port=" + std::to_string(l_port));

        // 新增：检查是否取消连接
        if (m_cancelConnect.load(std::memory_order_acquire)) {
            NppSSH_LogErrorAuto("步骤1：连接已取消，终止执行");
            guard();
            return;
        }

        // 步骤2：初始化WSA
        WSADATA wsa;
        if (!InitWSA(wsa)) {
            NppSSH_LogErrorAuto("步骤2：WSA初始化失败");
            guard();
            return;
        }
        NppSSH_LogInfoAuto("步骤2：WSA初始化成功");

        // 新增：检查是否取消连接
        if (m_cancelConnect.load(std::memory_order_acquire)) {
            NppSSH_LogErrorAuto("步骤2后：连接已取消，释放WSA资源");
            WSACleanup(); // 释放WSA资源
            guard();
            return;
        }

        // 步骤3：创建Socket
        SOCKET sock = CreateAndConnectSocket(l_host, l_port, err);
        if (sock == INVALID_SOCKET) {
            NppSSH_LogErrorAuto("步骤3：Socket失败 → " + err);
            guard();
            return;
        }
        NppSSH_LogInfoAuto("步骤3：Socket连接成功");

        // 新增：检查是否取消连接
        if (m_cancelConnect.load(std::memory_order_acquire)) {
            NppSSH_LogErrorAuto("步骤3后：连接已取消，关闭Socket");
            closesocket(sock);
            WSACleanup();
            guard();
            return;
        }

        // 步骤4：SSH握手
        LIBSSH2_SESSION* session = InitSSHSession(sock, l_host, l_port, err);
        if (!session) {
            
            if (!IsSocketValid(sock)) {
                NppSSH_LogErrorAuto("握手过程中Socket已失效");
            }
            NppSSH_LogErrorAuto("步骤4：SSH握手失败 → " + err);

            closesocket(sock);
            WSACleanup();
            guard();
            return;
        }
        NppSSH_LogInfoAuto("步骤4：SSH握手成功");

        // 新增：检查是否取消连接
        if (m_cancelConnect.load(std::memory_order_acquire)) {
            NppSSH_LogErrorAuto("步骤4后：连接已取消，释放SSH会话和Socket");
            libssh2_session_free(session);
            closesocket(sock);
            WSACleanup();
            guard();
            return;
        }

        // 步骤5：认证
        if (!AuthenticateSSH(session, l_user, l_pass, err)) {
            libssh2_session_free(session);
            closesocket(sock);
            WSACleanup();
            NppSSH_LogErrorAuto("步骤5：认证失败 → " + err);
            guard();
            return;
        }
        NppSSH_LogInfoAuto("步骤5：SSH认证成功");

        // 新增：检查是否取消连接
        if (m_cancelConnect.load(std::memory_order_acquire)) {
            NppSSH_LogErrorAuto("步骤5后：连接已取消，释放所有资源");
            libssh2_session_free(session);
            closesocket(sock);
            WSACleanup();
            guard();
            return;
        }

        // 步骤6：赋值到成员（优化锁逻辑）
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_host = l_host;
            m_port = l_port;
            m_user = l_user;
            m_pass = l_pass;
            m_sock = sock;
            m_session = session;
            m_connected.store(true, std::memory_order_release);
        }

        //连接成功后，申请 PTY 伪终端
        bool isReqPTY = CreatePtyChannel();
        if (!isReqPTY) {
            ReleaseResources();//申请失败，释放资源
            return;
        }
        // 步骤7：读取Banner和启动心跳（锁外执行）
        // 增加 3次重试机制，确保通道完全就绪
        StartHeartbeat();

        NppSSH_LogInfoAuto("SSH连接成功！");


        int retryCount = 0;
        const int MAX_RETRY = 10;    // 增加重试次数
        const int RETRY_DELAY_MS = 200; // 每次重试延迟200ms
        while (retryCount < MAX_RETRY) {
            if (IsShellReady()) {
                NppSSH_LogInfoAuto("伪终端就绪成功！");
                ReadLoginBanner(session);
                break;
            }
            retryCount++;
            NppSSH_LogInfoAuto("【重试】等待伪终端就绪：第" + std::to_string(retryCount) + "次");
            // 增加延迟，避免高频重试
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
        }

        if (retryCount >= MAX_RETRY) {
            NppSSH_LogErrorAuto("【错误】" + std::to_string(MAX_RETRY) + "次重试后，伪终端仍未就绪");
            // 清理资源
            ReleaseResources();
            return;
        }
        ok = true;
    }
    catch (const std::exception& e) {
        std::string msg = "连接异常：";
        msg += e.what();
        NppSSH_LogErrorAuto(msg.c_str());
        ok = false;
    }
    catch (...) {
        NppSSH_LogErrorAuto("连接未知异常");
        ok = false;
    }

    guard();
    m_connecting.store(false, std::memory_order_release);
}

bool SSHConnection_Connect(int panelId, const char* host, int port, const char* user, const char* pass) {
    NppSSH_LogInfoAuto("面板="+std::to_string(panelId) +",绑定连接信息");
    // 创建/覆盖面板ID对应的连接实例 
    SSHConnection* conn = nullptr;
    {
        std::lock_guard<std::mutex> mapLock(g_panelConnMutex);
        // 无论是否存在，直接创建新实例覆盖（存在则旧实例被智能指针自动析构）
        auto newConn = std::make_unique<SSHConnection>();
        g_panelConnections[panelId] = std::move(newConn);
        // 获取新实例指针
        conn = g_panelConnections[panelId].get();
    }

    // 空指针防御
    if (!conn) {
        NppSSH_LogErrorAuto("创建/覆盖SSHConnection实例失败，panelId=" + std::to_string(panelId));
        // 失败时清理当前面板ID数据
        if (IsPanelIdExists(panelId)) {
            std::lock_guard<std::mutex> mapLock(g_panelConnMutex);
            g_panelConnections.erase(panelId);
        }
        return false;
    }


    // 第二步：调用Connect（实例锁）
    bool connectResult = false;
    try {
        connectResult = conn->Connect(host, port, user, pass); // Connect内部已加锁，无需外层锁
    }
    catch (const std::exception& e) {
        NppSSH_LogErrorAuto("调用Connect异常: " + std::string(e.what()));
        connectResult = false;
    }
    catch (...) {
        NppSSH_LogErrorAuto("调用Connect未知异常");
        connectResult = false;
    }

    // 连接失败时兜底清理数据 
    if (!connectResult) {
        NppSSH_LogInfoAuto("面板" + std::to_string(panelId) + "连接失败，清理对应数据");
        // 检查面板ID是否存在，存在则删除整条数据（无需调用Disconnect，直接清除）
        if (IsPanelIdExists(panelId)) {
            std::lock_guard<std::mutex> mapLock(g_panelConnMutex);
            g_panelConnections.erase(panelId);
        }
    }
    else {
        conn ->SetPanelHwnd(NppSSH__getPanelHwnd(panelId));
    }
    return connectResult;
}
// 断开连接 + 彻底删除面板数据
void SSHConnection_Disconnect(int panelId) {
    // 第一步：使用工具函数判断面板ID是否存在
    if (!IsPanelIdExists(panelId)) {
        NppSSH_LogInfoAuto("面板" + std::to_string(panelId) + "不存在，无需断开");
        return;
    }

    // 第二步：加锁操作 map（安全获取实例）
    SSHConnection* conn = GetSSHConnectionByPanelId(panelId);

    // 第三步：存在实例并且已经连接，则执行内部断开逻辑
    if (conn && conn->Getconnected()) {
        std::lock_guard<std::mutex> connLock(conn->GetMutex());
        NppSSH_LogInfoAuto("面板" + std::to_string(panelId) + "准备执行内部断开");
        conn->Disconnect();
    }

    // 第四步：彻底从 map 中删除整条 key-value 数据（最关键）
    {
        std::lock_guard<std::mutex> mapLock(g_panelConnMutex);
        g_panelConnections.erase(panelId);
        NppSSH_LogInfoAuto("面板" + std::to_string(panelId) + "已从全局map中彻底移除");
    }
}
// 判断是否连接（外部接口）
bool SSHConnection_IsConnected(int panelId) {
    // 第一步：使用工具函数判断面板ID是否存在
    if (!IsPanelIdExists(panelId)) {
        return false;
    }

    // 第二步：加锁安全获取连接实例
    SSHConnection* conn = GetSSHConnectionByPanelId(panelId);

    // 第三步：实例存在，直接调用类内部的 IsConnected()
    if (conn) {
        return conn->IsConnected();
    }

    // 兜底：实例不存在返回 false
    return false;
}

void SSHConnection_ResetState(int panelId) {
    // 第一步：使用工具函数判断 panelId 是否存在，不存在直接返回
    if (!IsPanelIdExists(panelId)) {
        return;
    }

    // 第二步：使用工具函数获取连接实例
    SSHConnection* conn = GetSSHConnectionByPanelId(panelId);

    // 第三步：实例存在则调用重置方法
    if (conn) {
        std::lock_guard<std::mutex> connLock(conn->GetMutex());
        conn->ResetState();
    }
}

bool SSHConnection_ExecuteCommand(int panelIndex, const std::string& cmd) {
    // 1. 工具函数：判断面板ID是否存在
    if (!IsPanelIdExists(panelIndex)) {
        NppSSH_LogErrorAuto("命令执行失败，当前面板连接异常");
        return false;
    }

    // 2. 工具函数：获取连接实例
    SSHConnection* conn = GetSSHConnectionByPanelId(panelIndex);

    // 3. 实例为空 → 返回异常
    if (!conn) {
        NppSSH_LogErrorAuto("命令执行失败，当前面板连接异常");
        return false;
    }

    // 4. 判断是否已连接
    if (!conn->IsConnected()) {//true已连接
        NppSSH_LogErrorAuto("命令执行失败，当前未连接");
        return false;
    }
    //NppSSH_LogInfoAuto("准备执行命令！！！！！！！");
    // 5. 已连接 → 执行命令并返回结果
    return conn->ExecuteCommand(cmd);
}

std::string SSHConnection_Prompt(int panelIndex) {
    // 1. 工具函数判断面板是否存在
    if (!IsPanelIdExists(panelIndex)) {
        NppSSH_LogInfoAuto("当前未连接，不能获取提示符");
        return "";
    }

    // 2. 工具函数获取实例
    SSHConnection* conn = GetSSHConnectionByPanelId(panelIndex);

    // 3. 实例不存在 → 返回默认提示符
    if (!conn) {
        NppSSH_LogInfoAuto("当前实例不存在，不能获取提示符");
        return "";
    }

    // 4. 未连接 → 返回默认提示符
    if (!conn->IsConnected()) {
        NppSSH_LogInfoAuto("当前实例未连接，不能获取提示符");
        return "";
    }
    NppSSH_LogInfoAuto("【调试】SSHConnection_Prompt获取提示符="+ conn->GetPrompt());

    // 5. 已连接 → 返回真实提示符
    return conn->GetPrompt();
}