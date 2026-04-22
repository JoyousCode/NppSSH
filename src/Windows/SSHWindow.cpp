//SSHWindow.cpp（仅分发调用，无具体逻辑）
#include "SSHWindow.h"
#include "SSHPanel.h"
#include "SSHConnection.h"
#include "SSHLog.h"

// 全局变量转发（实际定义在SSHPanel中）
std::vector<NppSSHDockPanel*>& g_sshPanels = SSHPanel_GetGlobalPanels();
std::atomic<int>& g_panelCounter = SSHPanel_GetGlobalPanelCounter();
NppData& g_nppData = SSHPanel_GetGlobalNppData();
HINSTANCE& g_hInst = SSHPanel_GetGlobalHInst();
int& getPanelId = SSH_GetPanelId(); //获取点击连接图标面板索引

// SSH连接全局状态转发（实际定义在SSHConnection中）
LIBSSH2_SESSION*& sshSession = SSHConnection_GetSession();
SOCKET& sock = SSHConnection_GetSocket();
bool& connected = SSHConnection_GetConnectedState();
const char*& host = SSHConnection_GetHost();
int& port = SSHConnection_GetPort();
const char*& user = SSHConnection_GetUser();
const char*& pass = SSHConnection_GetPass();
std::string& g_loginBanner = SSHConnection_loginBanner();

/**************（实际定义在SSHPanel中）***************/
void SavePanelCountToIni(int count) {  
    SSHPanel_SavePanelCountToIni(count); // INI操作转发
}

int LoadPanelCountFromIni() {
    return SSHPanel_LoadPanelCountFromIni();
}

void DeletePanelCountFromIni() {
    SSHPanel_DeletePanelCountFromIni();
}

void RecreatePanelsOnNppStart() {
    SSHPanel_RecreatePanelsOnNppStart();    // NPP启动重建面板转发
}
// 新增：焦点设置转发
void NppSSH_SetCommandEditFocus(int panelIndex) {
    SSHPanel_SetCommandEditFocus(panelIndex);
}

// 新增：键盘事件处理转发（核心：Panel检测到按键后，通过Window中转给Connection）
bool NppSSH_HandleCommandKeyEvent(int panelIndex, WPARAM wParam, LPARAM lParam) {
    return SSHPanel_HandleCommandKeyEvent(panelIndex, wParam, lParam);
}

void NppSSH_AppendPanelOutput(int panelIndex, const std::string& text) {
    SSHPanel_AppendOutput(panelIndex, text);
}
HWND NppSSH_getLoginPanel() {
    return SSHPanel_getLoginPanel();        //获得登录面板句柄
}


/**************（实际定义在SSHConnection中）***************/
bool NppSSH_Connect(const char* host, int port, const char* user, const char* pass) {
    return SSHConnection_Connect(host, port, user, pass);   // SSH连接操作转发
}

void NppSSH_Disconnect() {
    SSHConnection_Disconnect();
}

bool NppSSH_IsConnected() {
    return SSHConnection_IsConnected();
}

void NppSSH_ResetConnectionState() {
    SSHConnection_ResetState();
}


void DisconnectPanel(int panelIndex) {      // 唯一转发：Panel → Window → Connection
    SSHConnection_DisconnectByPanelIndex(panelIndex);// 2. 转发断开：窗口只做中转，不存数据
}

void OnSSHConnected(int panelIndex) {   //连接成功后，窗口转发绑定面板ID
    SSHConnection_BindPanelIndex(panelIndex);
}

std::string NppSSH_ExecuteCommand(int panelIndex, const std::string& cmd) {
    return SSHConnection_ExecuteCommand(panelIndex, cmd);   // 命令执行转发
}

/**************（实际定义在SSHLog中）***************/
// 日志转发实现：调试级
void NppSSH_LogDebug(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_DEBUG, event, content);
}

// 日志转发实现：Info级别
void NppSSH_LogInfo(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_INFO, event, content);
}

// 日志转发实现：警告级
void NppSSH_LogWarn(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_WARN, event, content);
}

// 日志转发实现：Error级别
void NppSSH_LogError(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_ERROR, event, content);  
}
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


