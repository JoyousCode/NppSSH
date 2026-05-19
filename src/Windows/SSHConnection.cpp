//SSHConnection.cpp（SSH 连接具体实现）
#include "SSHConnection.h"
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <stdarg.h>

// 全局变量（改用智能指针管理）
std::unordered_map<int, std::unique_ptr<SSHConnection>> g_panelConnections;
std::mutex g_panelConnMutex;
std::string loginBanner;

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
    if (!errmsg) return "未知错误（错误码：" + std::to_string(code) + "）";
    return std::string(errmsg) + "（错误码：" + std::to_string(code) + "）";
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
// 释放连接资源
static void ReleaseConnectionResources(SOCKET sock, LIBSSH2_SESSION* session) {
    // 释放shell通道（补充原有逻辑）
    //if (session) {
    //    LIBSSH2_CHANNEL* channel = libssh2_session_get_channel(session);
    //    if (channel) {
    //        libssh2_channel_wait_closed(channel);
    //        libssh2_channel_close(channel);
    //        libssh2_channel_free(channel);
    //    }
    //}

    // 释放SSH会话
    if (session) {
        libssh2_session_disconnect(session, "Connection closed");
        libssh2_session_free(session);
    }

    // 关闭Socket
    if (sock != INVALID_SOCKET) {
        shutdown(sock, SD_BOTH);
        closesocket(sock);
    }
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
    m_connected = other.m_connected.load();
    m_connecting = other.m_connecting.load();
    m_cancelConnect = other.m_cancelConnect.load();
    m_host = std::move(other.m_host);
    m_user = std::move(other.m_user);
    m_pass = std::move(other.m_pass);
    m_port = other.m_port;
    m_prompt = std::move(other.m_prompt);
    m_dirFile = std::move(other.m_dirFile);
    m_showDir = std::move(other.m_showDir);
    m_homeDir = std::move(other.m_homeDir);
    m_shellChannel = other.m_shellChannel;
    m_stopHeartbeat = other.m_stopHeartbeat.load();
    m_heartbeatThread = std::move(other.m_heartbeatThread);
    m_pConnectThread = other.m_pConnectThread;
    isCdCommand = other.isCdCommand;
    isPwdCommand = other.isPwdCommand;

    // 源对象置空
    other.m_session = nullptr;
    other.m_sock = INVALID_SOCKET;
    other.m_connected = false;
    other.m_connecting = false;
    other.m_cancelConnect = false;
    other.m_port = 22;
    other.m_shellChannel = nullptr;
    other.m_stopHeartbeat = true;
    other.m_pConnectThread = nullptr;
    other.isCdCommand = false;
    other.isPwdCommand = false;
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
        m_connected = other.m_connected.load();
        m_connecting = other.m_connecting.load();
        m_cancelConnect = other.m_cancelConnect.load();
        m_host = std::move(other.m_host);
        m_user = std::move(other.m_user);
        m_pass = std::move(other.m_pass);
        m_port = other.m_port;
        m_prompt = std::move(other.m_prompt);
        m_dirFile = std::move(other.m_dirFile);
        m_showDir = std::move(other.m_showDir);
        m_homeDir = std::move(other.m_homeDir);
        m_shellChannel = other.m_shellChannel;
        m_stopHeartbeat = other.m_stopHeartbeat.load();
        m_heartbeatThread = std::move(other.m_heartbeatThread);
        m_pConnectThread = other.m_pConnectThread;
        isCdCommand = other.isCdCommand;
        isPwdCommand = other.isPwdCommand;

        // 源对象置空
        other.m_session = nullptr;
        other.m_sock = INVALID_SOCKET;
        other.m_connected = false;
        other.m_port = 22;
        other.m_shellChannel = nullptr;
        other.m_stopHeartbeat = true;
        other.m_pConnectThread = nullptr;
        other.isCdCommand = false;
        other.isPwdCommand = false;
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
}

// 释放资源
void SSHConnection::ReleaseResources() {
    //std::lock_guard<std::mutex> lock(m_mutex);//打开会发生重复加锁
    NppSSH_LogInfoAuto("释放资源.............");
    //先废掉所有资源（线程会自动退出）
    SOCKET oldSock = m_sock;
    m_sock = INVALID_SOCKET;

    LIBSSH2_SESSION* oldSession = m_session;
    m_session = nullptr;

    LIBSSH2_CHANNEL* oldChannel = m_shellChannel;
    m_shellChannel = nullptr;

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
            libssh2_channel_wait_closed(oldChannel);
            libssh2_channel_close(oldChannel);
            libssh2_channel_free(oldChannel);
        }
        catch (...) {}
    }

    // 释放SSH会话
    if (oldSession) {
        try {
            NppSSH_LogInfoAuto("释放资源.............释放SSH会话");
            libssh2_session_disconnect(oldSession, "Connection closed");
            libssh2_session_free(oldSession);
        }
        catch (...) {}
    }

    // 关闭Socket
    if (oldSock != INVALID_SOCKET) {
        try {
            NppSSH_LogInfoAuto("释放资源.............关闭Socket");
            shutdown(oldSock, SD_BOTH);
            closesocket(oldSock);
        }
        catch (...) {}
    }
    NppSSH_LogInfoAuto("释放资源.............重置状态");
    // 重置状态
    m_connected = false;
    m_connecting = false;
    m_cancelConnect = false;
    m_prompt = "[unknown@unknown ~]# ";
    m_dirFile = "~"; 
    m_showDir = "~";
    m_homeDir = "";
    isCdCommand = false;
    isPwdCommand = false;
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
        //NppSSH_LogInfoAuto("心跳线程已启动循环，第" + std::to_string(secondCount) + "次" +
        //    std::to_string(m_stopHeartbeat.load(std::memory_order_acquire)));

        // 第四步：心跳逻辑（增加空指针检测，避免访问已释放资源）
        if (secondCount >= SSHConst::HEARTBEAT_INTERVAL_MS) {
            secondCount = 0;
            // 双重检测：停止信号+资源有效性
            if (m_stopHeartbeat.load(std::memory_order_acquire)) {
                goto THREAD_EXIT;
            }
            if (m_session != nullptr && m_connected) {
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

    m_stopHeartbeat = false;
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
    ReleaseResources();
    m_connected.store(false, std::memory_order_release);
}

// 重置状态
void SSHConnection::ResetState() {
    // 标记取消连接
    m_cancelConnect.store(true, std::memory_order_release);

    std::lock_guard<std::mutex> lock(m_mutex);

    // 停止心跳
    m_stopHeartbeat = true;
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
    m_connected = false;
    m_connecting = false;
    m_session = nullptr;
    m_sock = INVALID_SOCKET;
    m_dirFile = "~";
    m_showDir = "~";
    m_prompt = "[unknown@unknown ~]# ";
    m_homeDir = "";
    isCdCommand = false;
    isPwdCommand = false;
}
// 获取家目录
std::string SSHConnection::GetHomeDir() {
    // 1. 已缓存 → 直接返回（避免重复执行命令）
    if (!m_homeDir.empty()) {
        return m_homeDir;
    }

    // 2. 未连接 → 返回空
    if (!m_connected || !m_session) {
        return "";
    }

    // 3. 创建通道（智能指针自动释放，安全无泄漏）
    std::unique_ptr<LIBSSH2_CHANNEL, decltype(&libssh2_channel_free)>
        ch(libssh2_channel_open_session(m_session), libssh2_channel_free);
    if (!ch) {
        return "";
    }

    // 4. 执行获取 HOME 命令
    if (libssh2_channel_exec(ch.get(), "echo $HOME") != 0) {
        libssh2_channel_close(ch.get());
        return "";
    }

    // 5. 读取返回数据
    std::string home;
    char buf[SSHConst::BUF_SIZE_SMALL] = { 0 };
    int bytesRead = 0;

    while ((bytesRead = libssh2_channel_read(ch.get(), buf, sizeof(buf) - 1)) > 0) {
        home.append(buf, bytesRead);
        memset(buf, 0, sizeof(buf));
    }

    // 6. 关闭通道
    libssh2_channel_close(ch.get());

    // 7. 清理换行、回车符（标准清理）
    home.erase(std::remove(home.begin(), home.end(), '\n'), home.end());
    home.erase(std::remove(home.begin(), home.end(), '\r'), home.end());

    // 8. 缓存到家目录成员变量（持久化存储）
    m_homeDir = std::move(home);

    // 9. 返回最终路径（/root 或 /home/xxx）
    return m_homeDir;
}

// 替换~为真实家目录，检测PWD命令
std::string SSHConnection::ReplaceTildeWithHome(const std::string& cmd) {
    // 初始化标记为默认值
    isCdCommand = false;
    isPwdCommand = false;
    std::string result = cmd;
    std::string home = m_homeDir;
    std::string fileDir = m_dirFile;//用于拼接当前所在目录
    
    // ========== 预处理：统一fileDir格式（末尾确保有/） ==========
    if (!fileDir.empty() && fileDir.back() != '/') {
        fileDir += "/";
    }
    //NppSSH_LogInfoAuto("预处理命令，处理前====1111===isCdCommand====" + std::to_string(isCdCommand) +
    //    ",isPwdCommand.load()======" + std::to_string(isPwdCommand)
    //    + "，m_dirFile====" + m_dirFile + "，m_homeDir====" + m_homeDir + "，fileDir====" + fileDir + "，result====" + result);
    // ========== 第一步：替换~和~/（严格匹配规则） ==========
    bool hasTildeReplaced = false; // 标记是否执行了~替换
    size_t pos = 0;
    while (pos < result.size()) {
        // 匹配~/：开头或前面是空格，且后接/
        if (result[pos] == '~' && pos + 1 < result.size() && result[pos + 1] == '/') {
            bool frontOk = (pos == 0) || std::isspace(static_cast<unsigned char>(result[pos - 1]));
            if (frontOk) {
                result.replace(pos, 2, home + "/");
                hasTildeReplaced = true;
                pos += home.size() + 1; // 跳过替换后的字符，避免重复匹配
                continue;
            }
        }
        // 匹配独立~：前后为空/空格/结束
        else if (result[pos] == '~') {
            bool frontOk = (pos == 0) || std::isspace(static_cast<unsigned char>(result[pos - 1]));
            bool backOk = (pos == result.size() - 1) || std::isspace(static_cast<unsigned char>(result[pos + 1]));
            if (frontOk && backOk) {
                result.replace(pos, 1, home);
                hasTildeReplaced = true;
                pos += home.size();
                continue;
            }
        }
        pos++;
    }
    NppSSH_LogInfoAuto("预处理命令，第一步结束====2222===isCdCommand====" + std::to_string(isCdCommand) +
        ",isPwdCommand.load()======" + std::to_string(isPwdCommand)
        + "，m_dirFile====" + m_dirFile + "，m_homeDir====" + m_homeDir + "，fileDir====" + fileDir + "，result====" + result);
    // ========== 第二步：检测pwd命令 + 处理cd命令路径拼接 ==========
    pos = 0;
    while (pos < result.size()) {
        // 跳过空白字符，简化命令检测逻辑
        pos = skipWhitespace(result, pos);
        if (pos >= result.size()) break;

        // ---------- 检测独立pwd命令 ----------
        if (result.compare(pos, 3, "pwd") == 0) {
            // 验证pwd是独立命令（后接分隔符/空白/结束）
            bool rightOk = (pos + 3 == result.size()) ||
                std::isspace(static_cast<unsigned char>(result[pos + 3])) ||
                isCmdSeparator(result[pos + 3]);
            if (rightOk) {
                isPwdCommand = true;
                pos += 3; // 跳过pwd
                continue;
            }
        }

        // ---------- 检测独立cd命令并处理路径拼接 ----------
        if (result.compare(pos, 2, "cd") == 0) {
            // 验证cd是独立命令（后接分隔符/空白/结束）
            bool rightOk = (pos + 2 == result.size()) ||
                std::isspace(static_cast<unsigned char>(result[pos + 2])) ||
                isCmdSeparator(result[pos + 2]);
            if (rightOk) {
                // 只要检测到【独立的 cd 命令】，直接赋值 true
                isCdCommand = true;
                // 找到cd参数的起始位置（跳过cd后的空白）
                size_t argStart = skipWhitespace(result, pos + 2);
                if (argStart == std::string::npos || argStart >= result.size()) {
                    pos = argStart;
                    continue;
                }

                // 找到cd参数的结束位置（分隔符/空白）
                size_t argEnd = findArgEnd(result, argStart);
                std::string cdArg = result.substr(argStart, argEnd - argStart);

                // 不处理的情况：空参数、绝对路径、~、~/
                if (cdArg.empty() || cdArg[0] == '/' || cdArg == "~" || (cdArg.size() >= 2 && cdArg.substr(0, 2) == "~/")) {
                    pos = argEnd;
                    continue;
                }

                // ========== 执行相对路径拼接 ==========
                std::string newPath = fileDir + cdArg;
                // 替换原参数为拼接后的路径
                result.replace(argStart, cdArg.size(), newPath);
                //isCdCommand = true;
                // 更新pos，跳过替换后的路径
                pos = argStart + newPath.size();
                continue;
            }
        }

        // ---------- 处理非cd/pwd的字符/分隔符 ----------
        if (isCmdSeparator(result[pos])) {
            // 跳过分隔符（支持&&/||等连续分隔符）
            pos++;
            if (pos < result.size() && result[pos] == result[pos - 1]) {
                pos++;
            }
        }
        else {
            pos++;
        }
    }
    NppSSH_LogInfoAuto("预处理命令，最终第二步结束====3333====isCdCommand===" + std::to_string(isCdCommand) +
        ",isPwdCommand.load()======" + std::to_string(isPwdCommand)
        + "，m_dirFile====" + m_dirFile + "，m_homeDir====" + m_homeDir + "，fileDir====" + fileDir + "，result====" + result);
    return result;
}

// 解析cd命令目标路径
void SSHConnection::ResolveCdTarget(const std::string& pwdDir) {
    // 步骤1：执行远程服务器命令（pwd）获取当前所在文件夹
    std::string pwdOutput = pwdDir;

    // 执行失败直接退出函数，不赋值
    if (pwdOutput.empty()) {
        return;
    }
    pwdOutput.erase(std::remove(pwdOutput.begin(), pwdOutput.end(), '\n'), pwdOutput.end());
    pwdOutput.erase(std::remove(pwdOutput.begin(), pwdOutput.end(), '\r'), pwdOutput.end());

    // 去掉末尾多余的 /（例如 /usr/ → /usr，/ → 保持 /）
    while (pwdOutput.size() > 1 && pwdOutput.back() == '/') {
        pwdOutput.pop_back();
    }


    NppSSH_LogInfoAuto("准备赋值的pwd输出=========="+pwdOutput);
    // 步骤2：将获取到的当前文件夹赋值给m_dirFile
    m_dirFile = pwdOutput;

    // 步骤3：根据m_dirFile给m_showDir赋值
    std::string homeDir = GetHomeDir();
    // 3.1 处理主目录情况（m_dirFile等于主目录时，显示~）
    if (pwdOutput == homeDir) {
        //NppSSH_LogInfoAuto("处理主目录情况（m_dirFile等于主目录时，显示~）pwd输出==========" + m_showDir);
        
        m_showDir = "~";
    }
    // 3.2 处理根目录情况（/）
    else if (pwdOutput == "/") {
        //NppSSH_LogInfoAuto("根目录情况（/）pwd输出==========" + m_showDir);

        m_showDir = "/";
    }
    // 3.3 处理其他目录（提取最后一个/后的部分）
    else {
        //NppSSH_LogInfoAuto("处理其他目录（提取最后一个/后的部分）pwd输出==========" + pwdOutput);

        size_t lastSlash = pwdOutput.find_last_of("/");
        // 确保路径格式正确（如/usr的lastSlash是0，/usr/的lastSlash是4）
        if (lastSlash == std::string::npos) {
            m_showDir = pwdOutput;  // 极端情况（理论上不会出现）
        }
        else {
            std::string dirName = pwdOutput.substr(lastSlash + 1);
            // 处理路径末尾带/的情况（如/usr/devFile/）
            m_showDir = dirName.empty() ? "/" : dirName;
        }
    }

    // 步骤4：赋值m_prompt
    if (!m_user.empty() && !m_host.empty()) {
        m_prompt = "[" + m_user + "@" + m_host + " " + m_showDir + "]# ";
    }
    else {
        m_prompt = "[unknown@unknown " + m_showDir + "]# ";
    }
}

// 执行命令
std::string SSHConnection::ExecuteCommand(const std::string& cmd) {
    NppSSH_LogInfoAuto("处理命令前，原生执行命令=========="+cmd+"，命令提示符显示的目录=="+ m_showDir+"，全路径=="+ m_dirFile);
    // 1. 先检查连接状态（加锁）
    bool isConnected = false;
    LIBSSH2_SESSION* session = nullptr;
    std::string runCmd;
    {
        isConnected = m_connected && m_session != nullptr && m_sock != INVALID_SOCKET;
        session = m_session;
        runCmd = ReplaceTildeWithHome(cmd); // 将“~”符号转换为home真是路径,拼接当前所在文件夹
    }
    if (!isConnected || !session) {
        return "❌ 面板未连接\n";
    }

    // 3. 创建命令通道（无锁）
    std::unique_ptr<LIBSSH2_CHANNEL, decltype(&libssh2_channel_free)>
        ch(libssh2_channel_open_session(session), libssh2_channel_free);
    if (!ch) {
        NppSSH_LogErrorAuto("创建命令通道失败");
        return "无法创建命令通道";
    }

    // 4. 构造执行命令
    bool cdFlag = isCdCommand;
    bool pwdFlag = isPwdCommand;
    bool isAppendPwd = !(cdFlag == false && pwdFlag == true);
    //realDir = "~/";//【\"】引号
    //runCmd = "cd \"" + realDir + "\" && ls && pwd";
    NppSSH_LogInfoAuto("处理命令后，准备构造执行命令前runCmd==========" + runCmd + "，命令提示符显示的目录==" + m_showDir + "，全路径==" + m_dirFile+ "isCdCommand===========" + std::to_string(cdFlag) + ",isPwdCommand.load()======" + std::to_string(pwdFlag));

    // 规则1：非cd命令 → 先cd到当前目录，再判断是否加pwd
    if (!cdFlag) {
        //NppSSH_LogInfoAuto("构造执行命令前===========" + runCmd);
        runCmd = "cd " + m_dirFile + " && " + runCmd;
    }
    // 规则2：cd命令 → 无pwd则追加 pwd
    if (isAppendPwd) {
        // 强制换行 + 最后一行输出pwd，方便解析和删除
        //runCmd += " && echo \"pwd=$PWD\"";
        bool endWithSemi = EndsWithSemicolonAfterTrim(runCmd);
        if (endWithSemi) {
            // 末尾是 ; → 用;
            runCmd += "     echo \"pwd=$PWD\"";
        }
        else {
            // 末尾不是 ; → 用 &&
            runCmd += "    &&   echo \"pwd=$PWD\"";
        }
    }
    NppSSH_LogInfoAuto("构造执行命令后=====runCmd======" + runCmd);

    // 5. 执行命令（无锁）
    int execRet = libssh2_channel_exec(ch.get(), runCmd.c_str());
    //[执行命令结束====cd ~&& cd / && pwd====执行命令的结果 == = 0]
    NppSSH_LogInfoAuto("执行命令结束===="+ runCmd+"====执行命令的结果==="+std::to_string(execRet));
    if (execRet != 0) {
        std::string errMsg = "命令执行失败 [错误码：" + std::to_string(execRet) + "] " + GetLibssh2ErrorMsg(session);
        NppSSH_LogErrorAuto(errMsg);
        libssh2_channel_close(ch.get());
        return "❌ " + errMsg + "\r\n";
    }

    // 6. 读取输出（无锁，耗时操作不持锁）
    char buf[SSHConst::BUF_SIZE_LARGE] = { 0 };
    std::string out, err;
    int bytesRead = 0;

    while ((bytesRead = libssh2_channel_read(ch.get(), buf, sizeof(buf) - 1)) > 0) {
        out.append(buf, bytesRead);
        memset(buf, 0, sizeof(buf));
        if (out.size() > 1024 * 1024) {
            out += "/r/n输出内容过长，已截断";
            break;
        }
    }

    while ((bytesRead = libssh2_channel_read_stderr(ch.get(), buf, sizeof(buf) - 1)) > 0) {
        err.append(buf, bytesRead);
        memset(buf, 0, sizeof(buf));
    }
    
    // 7. 关闭通道（无锁）
    libssh2_channel_send_eof(ch.get());
    libssh2_channel_wait_eof(ch.get());
    libssh2_channel_wait_closed(ch.get());
    libssh2_channel_close(ch.get());

    NppSSH_LogInfoAuto("执行命令输出原始内容====" + out);
    //去除末尾换行
    
    std::string finalOutput = out;

    
    //isCdCommand     isPwdCommand
    //true              true    更新
    //true              false   更新
    //false             true    不更新
    //false             false   更新
    if (execRet == 0 && isAppendPwd) {
        // 查找最后一行 pwd= 格式内容
        std::string pwdOutput = out;
        size_t lastPwdPos = out.rfind("pwd=");
        if (lastPwdPos != std::string::npos) {
            // 截取 pwd= 后的路径
            size_t lineEnd = out.find_first_of("\r\n", lastPwdPos);
            pwdOutput = out.substr(lastPwdPos + 4);
            if (lineEnd != std::string::npos) {
                pwdOutput = out.substr(lastPwdPos + 4, lineEnd - lastPwdPos - 4);
            }
            // 删除最后一行pwd，不返回给界面
            finalOutput = out.substr(0, lastPwdPos);
            // 去除末尾多余的换行/空格
            while (!finalOutput.empty() && (finalOutput.back() == '\n' || finalOutput.back() == '\r')) {
                finalOutput.pop_back();
            }

        }
        NppSSH_LogInfoAuto("更新持久化存储，解析后的pwd路径==========" + pwdOutput);
        if (!pwdOutput.empty()) {
            //更新持久化存储的路径
            //NppSSH_LogInfoAuto("更新持久化存储的路径");
            ResolveCdTarget(pwdOutput);
        }
        NppSSH_LogInfoAuto("返回给界面的最终内容====" + finalOutput);
    }
    // 8. 拼接结果  目前的逻辑最终内容的末尾不能有回车或者换行符，否则会多一行。TrimTrailingNewlines去除末尾该内容
    std::string result;
    if (!finalOutput.empty()) { result = TrimTrailingNewlines(finalOutput);}
    if (!finalOutput.empty()) { result += "\r\n当前所在全路径：\""; result += m_dirFile + "\"\r\n"; }
    if (!err.empty()) result += err;
    return result.empty() ? "当前全路径：\""+m_dirFile +"\"\r\n" : result;
}

// 获取提示符
std::string SSHConnection::GetPrompt() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_prompt.empty() ? "[unknown@unknown ~]# " : m_prompt;
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
    // 创建Socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        int err = WSAGetLastError();
        errorMsg = "Socket创建失败（错误码：" + std::to_string(err) + "）";
        return INVALID_SOCKET;
    }

    // 域名/IP解析
    addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    int ret = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
    if (ret != 0 || !result) {
        errorMsg = "IP解析失败（错误码：" + std::to_string(ret) + "）：" + host + ":" + std::to_string(port);
        closesocket(sock);
        return INVALID_SOCKET;
    }

    // 设置非阻塞模式
    u_long nonblock = 1;
    ioctlsocket(sock, FIONBIO, &nonblock);

    // 非阻塞连接
    int connectRet = connect(sock, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);

    // 处理非阻塞连接的立即失败
    if (connectRet == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            errorMsg = "Socket连接立即失败（错误码：" + std::to_string(err) + "）";
            closesocket(sock);
            return INVALID_SOCKET;
        }
    }

    // Select超时检测
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);

    timeval tv = { 0 };
    tv.tv_sec = SSHConst::CONNECT_SOCKET_TIMEOUT_MS / 1000;
    tv.tv_usec = (SSHConst::CONNECT_SOCKET_TIMEOUT_MS % 1000) * 1000;

    int select_ret = select(0, nullptr, &wfds, nullptr, &tv);
    if (select_ret <= 0) {
        errorMsg = "Socket连接超时（" + std::to_string(SSHConst::CONNECT_SOCKET_TIMEOUT_MS) + "ms）：" + host + ":" + std::to_string(port);
        closesocket(sock);
        return INVALID_SOCKET;
    }

    // 检查连接结果
    int err_code = 0;
    int len = sizeof(err_code);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err_code, &len);
    if (err_code != 0) {
        errorMsg = "Socket连接失败（错误码：" + std::to_string(err_code) + "）";
        if (err_code == 10061) errorMsg += "（端口未开放/拒绝连接）";
        if (err_code == 10065) errorMsg += "（网络不可达）";
        closesocket(sock);
        return INVALID_SOCKET;
    }

    // 恢复阻塞模式
    nonblock = 0;
    ioctlsocket(sock, FIONBIO, &nonblock);

    return sock;
}

// 初始化SSH会话并握手
LIBSSH2_SESSION* SSHConnection::InitSSHSession(SOCKET sock, const std::string& host, int port, std::string& errorMsg) {
    // 初始化libssh2
    if (libssh2_init(0) != 0) {
        errorMsg = "libssh2初始化失败";
        NppSSH_LogErrorAuto(errorMsg);
        return nullptr;
    }

    // 创建会话
    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        errorMsg = "libssh2_session_init失败";
        NppSSH_LogErrorAuto(errorMsg);
        return nullptr;
    }

    // 设置会话参数
    libssh2_session_set_blocking(session, 1);
    libssh2_session_set_timeout(session, SSHConst::SSH_HANDSHAKE_TIMEOUT_MS);

    // SSH握手
    if (libssh2_session_handshake(session, sock) != 0) {
        errorMsg = "SSH握手失败：" + GetLibssh2ErrorMsg(session);
        NppSSH_LogErrorAuto(errorMsg);
        libssh2_session_free(session);
        return nullptr;
    }

    NppSSH_LogInfoAuto("SSH握手成功：" + host + ":" + std::to_string(port));
    return session;
}

// SSH密码认证
bool SSHConnection::AuthenticateSSH(LIBSSH2_SESSION* session, const std::string& user, const std::string& pass, std::string& errorMsg) {
    if (!session) {
        errorMsg = "无效的SSH会话";
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
    if (!session) return;
    //配置home
    std::string homeDir = GetHomeDir();
    m_dirFile = homeDir;
    NppSSH_LogInfoAuto("当前登录用户的home完整路径1===="+homeDir);
    NppSSH_LogInfoAuto("当前登录用户的home完整路径2===="+ m_homeDir);

    // 读取Banner
    loginBanner = "\r\n";
    const char* banner = libssh2_session_banner_get(session);
    if (banner) {
        loginBanner += banner;
        loginBanner += "\r\n";
    }
    libssh2_session_banner_set(session,"20");
    // 获取登录时间
    std::unique_ptr<LIBSSH2_CHANNEL, decltype(&libssh2_channel_free)>
        timeChannel(libssh2_channel_open_session(session), libssh2_channel_free);

    if (timeChannel) {
        const char* timeCmd = "echo \"Last login: $(date '+%Y-%m-%d %H:%M:%S') from $(echo $SSH_CLIENT | awk '{print $1}')\"";
        if (libssh2_channel_exec(timeChannel.get(), timeCmd) == 0) {
            char timeBuf[SSHConst::BUF_SIZE_MEDIUM] = { 0 };
            std::string currentLoginTime;
            int bytesRead = 0;

            while ((bytesRead = libssh2_channel_read(timeChannel.get(), timeBuf, sizeof(timeBuf) - 1)) > 0) {
                currentLoginTime += timeBuf;
                memset(timeBuf, 0, sizeof(timeBuf));
            }

            if (!currentLoginTime.empty()) {
                loginBanner += currentLoginTime;
            }
            //NppSSH_LogInfoAuto("读取session的loginBanner=====" + loginBanner);
        }
        libssh2_channel_close(timeChannel.get());
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
    }

    if (!m_user.empty() && !m_host.empty()) {
        m_prompt = "[" + m_user + "@" + m_host + " " + m_showDir + "]# ";
    }
    else {
        m_prompt = "[unknown@unknown " + m_showDir + "]# ";
    }
}
bool SSHConnection::Connect(const char* host, int port, const char* user, const char* pass) {
    //NppSSH_LogInfoAuto("开始进行连接==========1");

    if (m_connected.load()) {
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
            m_connected = connFuture.get();
            return m_connected;
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
            closesocket(sock);
            WSACleanup();
            NppSSH_LogErrorAuto("步骤4：SSH握手失败 → " + err);
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
            m_connected = true;
        }

        // 步骤7：读取Banner和启动心跳（锁外执行）
        ReadLoginBanner(session);
        StartHeartbeat();

        NppSSH_LogInfoAuto("SSH连接成功！");
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
    m_connecting = false;
}


std::string& SSHConnection_loginBanner() { return loginBanner; }

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

std::string SSHConnection_ExecuteCommand(int panelIndex, const std::string& cmd) {
    // 1. 工具函数：判断面板ID是否存在
    if (!IsPanelIdExists(panelIndex)) {
        return "命令执行失败，当前面板连接异常";
    }

    // 2. 工具函数：获取连接实例
    SSHConnection* conn = GetSSHConnectionByPanelId(panelIndex);

    // 3. 实例为空 → 返回异常
    if (!conn) {
        return "命令执行失败，当前面板连接异常";
    }

    // 4. 判断是否已连接
    if (!conn->IsConnected()) {//true已连接
        return "命令执行失败，当前未连接";
    }
    //NppSSH_LogInfoAuto("准备执行命令！！！！！！！");
    // 5. 已连接 → 执行命令并返回结果
    return conn->ExecuteCommand(cmd);
}

std::string SSHConnection_Prompt(int panelIndex) {
    // 1. 工具函数判断面板是否存在
    if (!IsPanelIdExists(panelIndex)) {
        return "[unknown@unknown ~]# ";
    }

    // 2. 工具函数获取实例
    SSHConnection* conn = GetSSHConnectionByPanelId(panelIndex);

    // 3. 实例不存在 → 返回默认提示符
    if (!conn) {
        return "[unknown@unknown ~]# ";
    }

    // 4. 未连接 → 返回默认提示符
    if (!conn->IsConnected()) {
        return "[unknown@unknown ~]# ";
    }

    // 5. 已连接 → 返回真实提示符
    return conn->GetPrompt();
}