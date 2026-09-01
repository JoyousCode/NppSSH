//SSHConnection.h（SSH 连接核心逻辑声明）
#pragma once
#include "SSHWindow.h"

// 前置声明
class SSHConnection;

// 常量定义（统一管理魔法值）
namespace SSHConst {
    // 超时配置（毫秒）
    constexpr int CONNECT_SOCKET_TIMEOUT_MS = 1000;
    constexpr int SSH_HANDSHAKE_TIMEOUT_MS = 1000;
    constexpr int SSH_AUTH_TIMEOUT_MS = 1000;
    constexpr int MAIN_THREAD_WAIT_INTERVAL_MS = 50;//主线程间隔时间
    constexpr int MAX_MAIN_THREAD_WAIT_MS = 30; // 主线程最大等待时间(秒)
    //实际发送心跳HEARTBEAT_INTERVAL_MS * MAX_HEART_BEAT_WAIT_MS = 30 * 1
    constexpr int HEARTBEAT_INTERVAL_MS = 30;//心跳线程间隔时间(秒)
    constexpr int MAX_HEART_BEAT_WAIT_MS = 1;//心跳最大等待时间(秒)

    // 端口范围
    constexpr int MIN_PORT = 1;
    constexpr int MAX_PORT = 65535;

    // 缓冲区大小
    constexpr int BUF_SIZE_SMALL = 256;
    constexpr int BUF_SIZE_MEDIUM = 1024;
    constexpr int BUF_SIZE_LARGE = 4096;
}

// 全局管理：面板索引 -> SSHConnection实例（改用智能指针）
extern std::unordered_map<int, std::shared_ptr<SSHConnection>> g_panelConnections;
extern std::mutex g_panelConnMutex; // 保护全局面板映射的锁
// 检查指定面板ID是否存在于全局连接映射中（线程安全）
// IsPanelIdExists(panelId) → 判断面板是否存在
// GetSSHConnectionByPanelId(panelId) → 获取实例指针
inline bool IsPanelIdExists(int panelId) {
    std::lock_guard<std::mutex> lock(g_panelConnMutex);
    return g_panelConnections.find(panelId) != g_panelConnections.end();
}
//工具函数，通过 this或者实例对象 指针查找对应的 面板ID（key）
int SSHConnection_GetPanelId(SSHConnection* self);
// SSH连接类（封装单个面板的连接数据与逻辑）
class SSHConnection {
public:
    // 构造函数（初始化默认值）
    SSHConnection();

    // 禁用拷贝（避免资源重复释放）
    SSHConnection(const SSHConnection&) = delete;
    SSHConnection& operator=(const SSHConnection&) = delete;

    // 移动语义（支持容器存储）
    SSHConnection(SSHConnection&& other) noexcept;
    SSHConnection& operator=(SSHConnection&& other) noexcept;

    // 析构函数（自动释放资源）
    ~SSHConnection();

    // 核心功能：连接SSH服务器
    bool Connect(const char* host, int port, const char* user, const char* pass, const char* director);
    void ConnectAsync(const char* host, int port, const char* user, const char* pass, std::promise<bool> promise);

    // 核心功能：断开连接
    void Disconnect();

    // 判断是否已连接（线程安全）
    bool IsConnected() const;

    // 执行命令（线程安全）
    bool ExecuteCommand(const std::string& cmd);

    // 获取提示符（线程安全）
    std::string GetPrompt() const;

    // 启动/停止心跳线程
    void StartHeartbeat();
    //void StopHeartbeat();

    // 重置连接状态
    void ResetState();

    // 成员变量的访问器（线程安全）
    void SetHost(const char* host);
    std::string GetHost() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_host;
    }

    void SetPort(int port) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_port = (port >= SSHConst::MIN_PORT && port <= SSHConst::MAX_PORT) ? port : 22;
    }
    int GetPort() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_port;
    }

    void SetUser(const char* user);
    std::string GetUser() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_user;
    }

    void SetPass(const char* pass);
    std::string GetPass() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_pass;
    }

    void SetSession(LIBSSH2_SESSION* session) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_session = session;
    }
    LIBSSH2_SESSION* GetSession() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_session;
    }

    void SetSocket(SOCKET sock) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sock = sock;
    }
    SOCKET GetSocket() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sock;
    }

    void SetShellChannel(LIBSSH2_CHANNEL* channel) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shellChannel = channel;
    }
    LIBSSH2_CHANNEL* GetShellChannel() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_shellChannel;
    }

    // 心跳控制
    bool IsHeartbeatStopped() const { return m_stopHeartbeat.load(std::memory_order_acquire); }
    std::mutex& GetMutex() { return m_mutex; }
    void Set_isAlive(bool isAlive){return m_isAlive.store(isAlive);}

    //设置面板句柄
    void SetPanelHwnd(HWND panelHwnd) {m_panelHwnd = panelHwnd;}

    //获取连接状态
    bool Getconnected() {return m_connected.load();}

    //后台持续读（官方poll）
    void StartShellReader();
    void StopShellReader();

    void SetPTYSize(int cols, int rows);
    
private:
    // 私有工具函数
    void ReleaseResources(); // 释放资源（内部复用）
    // 子函数：初始化WSA
    bool InitWSA(WSADATA& wsaData);

    // 子函数：创建并连接Socket
    SOCKET CreateAndConnectSocket(const std::string& host, int port, std::string& errorMsg);

    // 子函数：初始化SSH会话并握手
    LIBSSH2_SESSION* InitSSHSession(SOCKET sock, const std::string& host, int port, std::string& errorMsg);

    // 子函数：SSH密码认证
    bool AuthenticateSSH(LIBSSH2_SESSION* session, const std::string& user, const std::string& pass, std::string& errorMsg);

    // 子函数：读取登录Banner和登录时间
    void ReadLoginBanner(LIBSSH2_SESSION* session);

    // 子函数：申请Pty伪终端读取登录欢迎语
    bool SSHConnection::CreatePtyChannel();

    // 工具函数：提取字符串最后一行
    std::string extractLastLine(const std::string& str);
    // 工具函数：判断是否以指定字符串开头
    bool SSHConnection::startsWith(const std::string& str, const std::string& prefix);

    // 工具函数：判断伪终端是否就绪
    bool SSHConnection::IsShellReady();

    // 工具函数：根据阻塞方向等待socket
    bool SSHConnection::WaitSocketWithBackoff(SOCKET sock, LIBSSH2_SESSION* session,int base_wait_ms, int max_attempts);

    // 工具函数：检查socket是否有效
    bool SSHConnection::IsSocketValid(SOCKET sock);

    //工具函数，InitSSHSession函数初始化SSH会话并握手，检查socket是否有效
    bool SSHConnection::IsSocketAlive(SOCKET sock);

    bool SSHConnection::IsSocketWritable(SOCKET sock) {
        fd_set wfds;
        struct timeval tv = { 0, 100000 }; // 100us

        FD_ZERO(&wfds);
        FD_SET(sock, &wfds);

        int ret = select(0, nullptr, &wfds, nullptr, &tv);
        if (ret <= 0) {
            return false;
        }

        // 再检查 SO_ERROR
        int err = 0;
        int len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        return err == 0;
    }
    enum class ShellExitReason {
        Unknown,
        PromptReceived,
        StoppedByUser,
        SocketDead,
        RetryExhausted
    };

    ShellExitReason exitReason = ShellExitReason::Unknown;

private:

    // 实例级锁（保护当前面板的资源访问）
    mutable std::mutex m_mutex;//除心跳线程和伪终端线程以外的变量公共锁
    std::thread* m_pConnectThread = nullptr; // 连接线程
    // 连接核心资源（RAII管理，避免裸指针）
    LIBSSH2_SESSION* m_session = nullptr;
    SOCKET m_sock = INVALID_SOCKET;
    std::atomic<bool> m_connected = { false }; // 连接状态
    std::atomic<bool> m_connecting = { false }; // 连接中标记
    std::atomic<bool> m_cancelConnect = { false };//标记是否取消连接

    // 连接参数（改用std::string，消除手动free）
    std::string m_host;
    std::string m_user;
    std::string m_pass;
    int m_port = 22;
    HWND m_panelHwnd;

    std::string m_prompt = "";// 面板上的命令提示符，只有该提示符才能进行命令操作。
    std::atomic<LIBSSH2_CHANNEL*> m_shellChannel{ nullptr };//使用stomic保证内存的可见性

    
    // 心跳线程控制：条件变量+互斥锁
    std::mutex m_heartbeatMtx;
    std::thread m_heartbeatThread;
    void HeartbeatThreadFunc();
    std::condition_variable m_heartbeatCv;
    std::atomic<bool> m_isAlive = { true };
    std::atomic<bool> m_stopHeartbeat = { false };
    
    
    // 后台读线程，线程控制锁
    std::mutex m_readerMutex;
    std::thread m_shellReaderThread;
    void ShellReaderLoop();
    std::condition_variable m_readerCv;
    std::atomic<bool> m_waitingForPrompt{ false };//标记是否等待命令提示符（仅执行命令时为true）
    std::atomic<bool> m_stopReader{ false };
    std::atomic<bool> m_commandFinished{ false };
    std::atomic<bool> m_isReadingOutput{ false };//是否是持续输出
    std::string m_currentCommand;// 用于过滤命令回显
    
};

// SSH连接操作具体声明
bool SSHConnection_Handle(int panelId, std::wstring host, std::wstring port, std::wstring user, std::wstring pass, std::wstring director);
void SSHConnection_OnDisconn(int panelId);
bool SSHConnection_IsConn(int panelId);
void SSHConnection_ResetConn(int panelId);
bool SSHConnection_ExecuteCommand(int panelIndex, const std::string& cmd);
std::string SSHConnection_PanelPrompt(int panelIndex);
void SSHConnection_PtySize(int panelId, int cols, int rows);
void SSHConnection_ClearAllSSHConnections();

// 工具函数声明
inline std::string GetLibssh2ErrorMsg(LIBSSH2_SESSION* session);
inline std::string GetLibssh2ErrorExplanation(int error_code);
static bool ValidatePort(int port);

inline bool isCmdSeparator(char c);     // 辅助函数：判断字符是否为命令分隔符
inline size_t skipWhitespace(const std::string& str, size_t pos);   // 辅助函数：跳过连续的空白字符（空格/制表符）
inline size_t findArgEnd(const std::string& str, size_t pos);   // 辅助函数：查找命令参数的结束位置（分隔符/空白符）
inline std::string TrimTrailingNewlines(std::string str);   // 辅助函数：清理字符串末尾所有 \r\n，直到最后一个字符不是换行 / 回车。
inline std::string TrimTrailingWhitespace(std::string str); // 工具函数：去除字符串末尾所有空白（空格、\t、\n、\r）
inline bool EndsWithSemicolonAfterTrim(const std::string& cmd); // 工具函数：判断命令【去除末尾空白后】是否以 ; 结尾
// 工具函数：根据 panelId 安全获取 SSHConnection 实例（线程安全）
SSHConnection* GetSSHConnectionByPanelId(int panelId);
