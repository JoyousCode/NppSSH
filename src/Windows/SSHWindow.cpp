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

// SSH连接全局状态转发（实际定义在SSHConnection中）
LIBSSH2_SESSION*& sshSession = SSHConnection_GetSession();
SOCKET& sock = SSHConnection_GetSocket();
bool& connected = SSHConnection_GetConnectedState();
const char*& host = SSHConnection_GetHost();
int& port = SSHConnection_GetPort();
const char*& user = SSHConnection_GetUser();
const char*& pass = SSHConnection_GetPass();

// INI操作转发（替换原注册表）
void SavePanelCountToIni(int count) {
    SSHPanel_SavePanelCountToIni(count);
}

int LoadPanelCountFromIni() {
    return SSHPanel_LoadPanelCountFromIni();
}

void DeletePanelCountFromIni() {
    SSHPanel_DeletePanelCountFromIni();
}

// NPP启动重建面板转发
void RecreatePanelsOnNppStart() {
    SSHPanel_RecreatePanelsOnNppStart();
}

// SSH连接操作转发
bool NppSSH_Connect(const char* host, int port, const char* user, const char* pass) {
    return SSHConnection_Connect(host, port, user, pass);
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


// 日志转发实现：调试级（新增）
void NppSSH_LogDebug(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_DEBUG, event, content);
}

// 日志转发实现：Info级别
void NppSSH_LogInfo(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_INFO, event, content);
}

// 日志转发实现：警告级（新增）
void NppSSH_LogWarn(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_WARN, event, content);
}

// 日志转发实现：Error级别
void NppSSH_LogError(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_ERROR, event, content);  
}