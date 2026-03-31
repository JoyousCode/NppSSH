//SSHWindow.cpp（仅分发调用，无具体逻辑）
#include "SSHWindow.h"
#include "SSHPanel.h"
#include "SSHConnection.h"

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


//NppSSHDockPanel* CreateNewSSHDockPanel(int panelId) {
//    return new NppSSHDockPanel(panelId);
//}
//在 SSHWindow 中提供全局清理接口（如 NppSSH_Cleanup），
// 内部统一调用 SSHPanel 的面板清理和 SSHConnection 的连接清理，
// 让 NppPluginSSH 的 DLL_PROCESS_DETACH、NPPN_SHUTDOWN 仅调用此一个接口，避免清理逻辑分散导致的泄漏：
//void NppSSH_Cleanup() {
//    // 清理SSH连接
//    NppSSH_Disconnect();
//    // 清理面板
//    for (auto* panel : g_sshPanels) {
//        if (panel) delete panel;
//    }
//    g_sshPanels.clear();
//    // 清理注册表
//    DeletePanelCountFromReg();
//}