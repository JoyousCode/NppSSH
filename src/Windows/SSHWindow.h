#pragma once
#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "Notepad_plus_msgs.h"
#include "DockingFeature/DockingDlgInterface.h"
#include "DockingFeature/dockingResource.h"
#include "Resource.h"
#include "DockingFeature/Window.h"  
#include <Windows.h>
#include <libssh2.h> 
#include <tchar.h>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <atomic>

// 注册表持久化相关宏定义（NPP插件默认注册表路径）
#define NPP_SSH_REG_PATH _T("Software\\Notepad++\\Plugins\\NppSSH")//regedit→计算机\HKEY_CURRENT_USER\Software\Notepad++\Plugins\NppSSH
#define NPP_SSH_PANEL_COUNT _T("PanelCount")

// 资源ID定义
#define IDD_SSH_PANEL 1001
#define IDC_OUTPUT_EDIT 1002

// 这里必须包含，否则 NppSSHDockPanel 是未定义类型
//#include "SSHPanel.h"//////////////////////////////////////////////
// SSHWindow.h 仅保留分发逻辑，无具体实现
// 全局变量声明（供SSHClient调用）
extern std::vector<class NppSSHDockPanel*>& g_sshPanels;
extern std::atomic<int>& g_panelCounter;
extern NppData& g_nppData;
extern HINSTANCE& g_hInst;

// SSH连接全局状态（声明，具体定义在SSHConnection中）
extern LIBSSH2_SESSION*& sshSession;
extern SOCKET& sock;
extern bool& connected;
extern const char*& host;
extern int& port;
extern const char*& user;
extern const char*& pass;

// 核心：可停靠面板类（声明，具体实现在SSHPanel中）
class NppSSHDockPanel;

// 注册表操作函数（声明，具体实现在SSHPanel中）
void SavePanelCountToReg(int count);
int LoadPanelCountFromReg();
void DeletePanelCountFromReg();

// NPP启动时自动重建面板（分发到SSHPanel）
void RecreatePanelsOnNppStart();

// SSH连接操作函数（声明，具体实现在SSHConnection中）
bool NppSSH_Connect(const char* host, int port, const char* user, const char* pass);
void NppSSH_Disconnect();
bool NppSSH_IsConnected();
void NppSSH_ResetConnectionState();

////创建面板的转发函数声明
//NppSSHDockPanel* CreateNewSSHDockPanel(int panelId);