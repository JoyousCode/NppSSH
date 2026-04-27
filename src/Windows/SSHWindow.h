/* SSHWindow.h 仅保留分发逻辑，无具体实现
* 具体转发：
*     前提：头文件包含该头文件；
*     变量：变量可以随意调用改变内容，如有在某个文件中要设置初始值，则在某个文件中定义函数并返回值作为该变量值，具体转发的调用函数在Cpp文件中设置；
*     函数：函数只能调用，具体转发的调用函数在Cpp文件中
* 自己文件可以调用自己的函数，调用别的文件函数，必须通过此文件转发调用
* 第三方库可以在自己文件的头文件声明
*/
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

//#define WM_SSH_CONNECT_RESULT (WM_USER + 100)

/////////////////////////////////////////////////////////////初始值在SSHPanel
// 全局变量声明（供SSHClient调用）
extern std::vector<class NppSSHDockPanel*>& g_sshPanels;
extern std::atomic<int>& g_panelCounter;
extern NppData& g_nppData;
extern HINSTANCE& g_hInst;

/////////////////////////////////////////////////////////////初始值在SSHConnection
// SSH连接全局状态（声明，具体定义在SSHConnection中）
extern LIBSSH2_SESSION*& sshSession;
extern SOCKET& sock;
extern bool& connected;
extern const char*& host;
extern int& port;
extern const char*& user;
extern const char*& pass;
extern int& getPanelId;				//获取点击连接图标面板索引
extern std::string& g_loginBanner;	//登录成功执行命令的欢迎内容


//////////////////////////////////////////////////////////////////////////别的文件调用SSHPanel的函数内容
// 核心：可停靠面板类（声明，具体实现在SSHPanel中）
class NppSSHDockPanel;

// ini操作函数（声明，具体实现在SSHPanel中）
void SavePanelCountToIni(int count);
int LoadPanelCountFromIni();
void DeletePanelCountFromIni();

// NPP启动时自动重建面板（分发到SSHPanel）
void RecreatePanelsOnNppStart();

HWND NppSSH_getLoginPanel();	//获得每次登录面板创建的句柄


//////////////////////////////////////////////////////////////////////////别的文件调用SSHConnection的函数内容
// SSH连接操作函数（声明，具体实现在SSHConnection中）
bool NppSSH_Connect(const char* host, int port, const char* user, const char* pass);
void NppSSH_Disconnect();				// 断开SSH连接
bool NppSSH_IsConnected();				// 判断是否连接
void NppSSH_ResetConnectionState();		// 重置连接状态（暂未使用）

// 窗口类中添加转发函数
void DisconnectPanel(int panelIndex);
void OnSSHConnected(int panelIndex);
// 新增：命令执行中转接口声明
std::string NppSSH_ExecuteCommand(int panelIndex, const std::string& cmd); // 执行SSH命令



//////////////////////////////////////////////////////////////////////////别的文件调用SSHLog的函数内容
// 日志转发接口（核心：只转发，不处理逻辑）
void NppSSH_LogDebug(const std::string& event, const std::string& content);  // 调试级
void NppSSH_LogInfo(const std::string& event, const std::string& content);	
void NppSSH_LogWarn(const std::string& event, const std::string& content);   // 警告级
void NppSSH_LogError(const std::string& event, const std::string& content);

// 简化封装：自动传入当前调用函数名作为事件（无需手动传event）
#define NppSSH_LogDebugAuto(content) NppSSH_LogDebug(__FUNCTION__, content)
#define NppSSH_LogInfoAuto(content) NppSSH_LogInfo(__FUNCTION__, content)
#define NppSSH_LogWarnAuto(content) NppSSH_LogWarn(__FUNCTION__, content)
#define NppSSH_LogErrorAuto(content) NppSSH_LogError(__FUNCTION__, content)

