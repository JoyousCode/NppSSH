//SSHConnection.h（SSH 连接核心逻辑声明）
#pragma once
#include "SSHWindow.h"
#include <unordered_map>
#include <libssh2.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tchar.h>
#include <mutex>
#include <thread>
#include <stdexcept>
#include <string>
#include <future>
#include <atomic>
#include <memory>
#include <chrono>
#include <functional>

// 前置声明
class SSHConnection;

// 常量定义（统一管理魔法值）
namespace SSHConst {
    // 超时配置（毫秒）
    constexpr int CONNECT_SOCKET_TIMEOUT_MS = 1000;
    constexpr int SSH_HANDSHAKE_TIMEOUT_MS = 1000;
    constexpr int SSH_AUTH_TIMEOUT_MS = 1000;
    constexpr int MAIN_THREAD_WAIT_INTERVAL_MS = 50;
    constexpr int MAX_MAIN_THREAD_WAIT_MS = 10000; // 10秒（原3秒）
    constexpr int HEARTBEAT_INTERVAL_MS = 25000;
    constexpr int MAX_HEART_BEAT_WAIT_MS = 30;//心跳间隔时间(秒)

    // 端口范围
    constexpr int MIN_PORT = 1;
    constexpr int MAX_PORT = 65535;

    // 缓冲区大小
    constexpr int BUF_SIZE_SMALL = 256;
    constexpr int BUF_SIZE_MEDIUM = 1024;
    constexpr int BUF_SIZE_LARGE = 4096;
}

// 全局管理：面板索引 -> SSHConnection实例（改用智能指针）
extern std::unordered_map<int, std::unique_ptr<SSHConnection>> g_panelConnections;
extern std::mutex g_panelConnMutex; // 保护全局面板映射的锁
// 检查指定面板ID是否存在于全局连接映射中（线程安全）
// IsPanelIdExists(panelId) → 判断面板是否存在
// GetSSHConnectionByPanelId(panelId) → 获取实例指针
inline bool IsPanelIdExists(int panelId) {
    std::lock_guard<std::mutex> lock(g_panelConnMutex);
    return g_panelConnections.find(panelId) != g_panelConnections.end();
}
// 工具函数：根据 panelId 安全获取 SSHConnection 实例（线程安全）
SSHConnection* GetSSHConnectionByPanelId(int panelId);
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
    bool Connect(const char* host, int port, const char* user, const char* pass);
    void ConnectAsync(const char* host, int port, const char* user, const char* pass, std::promise<bool> promise);



    // 核心功能：断开连接
    void Disconnect();

    // 判断是否已连接（线程安全）
    bool IsConnected() const;

    // 执行命令（线程安全）
    std::string ExecuteCommand(const std::string& cmd);

    // 获取提示符（线程安全）
    std::string GetPrompt() const;

    // 启动/停止心跳线程
    void StartHeartbeat();
    void StopHeartbeat();

    // 重置连接状态
    void ResetState();

    // 更新当前目录（供cd命令使用）
    void UpdateCurrentDir(const std::string& cmd);

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

    void SetDirFile(const std::string& dir) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dirFile = dir;
    }
    std::string GetDirFile() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_dirFile;
    }

    void SetShowDir(const std::string& dir) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_showDir = dir;
    }
    std::string GetShowDir() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_showDir;
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

private:
    // 私有工具函数
    void ReleaseResources(); // 释放资源（内部复用）
    void ResolveCdTarget(const std::string& cmd); // 解析cd命令目标路径
    std::string GetCurrentWorkingDir(); // 获取服务器当前工作目录
    std::string GetHomeDir(); // 获取服务器家目录
    std::string ReplaceTildeWithHome(const std::string& path); // 替换~为真实家目录

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

private:
    // 连接核心资源（RAII管理，避免裸指针）
    LIBSSH2_SESSION* m_session = nullptr;
    SOCKET m_sock = INVALID_SOCKET;
    std::atomic<bool> m_connected = false; // 连接状态
    std::atomic<bool> m_connecting = false; // 连接中标记
    std::atomic<bool> m_cancelConnect = false;//标记是否取消连接

    // 连接参数（改用std::string，消除手动free）
    std::string m_host;
    std::string m_user;
    std::string m_pass;
    int m_port = 22;

    // 目录和提示符
    std::string m_prompt = "[unknown@unknown ~]# ";// 面板上的命令提示符，只有该提示符才能进行命令操作。
    std::string m_dirFile = "~"; // 面板上所在文件夹的全路径
    std::string m_showDir = "~"; // 面板上显示的当前文件夹名称
    std::string m_homeDir = "";//连接后的用户home目录，执行~,执行历史命令用到
    bool isCdCommand = false;
    bool isPwdCommand = false;
    LIBSSH2_CHANNEL* m_shellChannel = nullptr;


    // 心跳相关
    // 心跳线程控制：条件变量+互斥锁
    std::mutex m_heartbeatMtx;
    std::condition_variable m_heartbeatCv;
    std::atomic<bool> m_isAlive = true;
    void HeartbeatThreadFunc();
    std::atomic<bool> m_stopHeartbeat = false;
    std::thread m_heartbeatThread;
    std::thread* m_pConnectThread = nullptr; // 连接线程

    // 实例级锁（保护当前面板的资源访问）
    mutable std::mutex m_mutex;
};

// SSH连接全局状态封装
LIBSSH2_SESSION*& SSHConnection_GetSession();
SOCKET& SSHConnection_GetSocket();
bool& SSHConnection_GetConnectedState();
int& SSHConnection_GetPort();
const char* SSHConnection_GetHost();
const char* SSHConnection_GetUser();
const char* SSHConnection_GetPass();
std::string& SSHConnection_loginBanner();

// SSH连接操作具体声明
bool SSHConnection_Connect(int panelId, const char* host, int port, const char* user, const char* pass);
void SSHConnection_Disconnect(int panelId);
bool SSHConnection_IsConnected(int panelId);
void SSHConnection_ResetState(int panelId);
std::string SSHConnection_ExecuteCommand(int panelIndex, const std::string& cmd);
std::string SSHConnection_Prompt(int panelIndex);

// 工具函数声明
inline std::wstring GBKToWstring(const std::string& str);
inline std::string GetLibssh2ErrorMsg(LIBSSH2_SESSION* session);
static void ReleaseConnectionResources(SOCKET sock, LIBSSH2_SESSION* session);
static bool ValidatePort(int port);