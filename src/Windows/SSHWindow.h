/* SSHWindow.h 仅保留分发逻辑，无具体实现
* 具体转发：
*     前提：头文件包含该头文件；
*     变量：变量可以随意调用改变内容，如有在某个文件中要设置初始值，则在某个文件中定义函数并返回值作为该变量值，具体转发的调用函数在Cpp文件中设置；
*     函数：函数只能调用，具体转发的调用函数在Cpp文件中
*     容器：外部无法直接操作容器，只能调用转发函数
*     自己文件可以调用自己的函数，调用别的文件函数，必须通过此文件转发调用
* 第三方库可以在自己文件的头文件声明
* 注意不能改变顺序：
*    #include <winsock2.h>
*    #include <ws2tcpip.h>
*    #include <Windows.h>
* 
*/
#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "Notepad_plus_msgs.h"
#include "DockingFeature/DockingDlgInterface.h"
#include "DockingFeature/dockingResource.h"
#include "Resource.h"
#include "DockingFeature/Window.h"  

#include <libssh2.h> 
#include <tchar.h>
#include <string>

#include <vector>
#include <atomic>

#include <locale>
#include <fstream> 
#include <codecvt>

#include <ctime>
#include <sstream>
#include <iomanip>
#include <shlwapi.h>
#include <queue>

#include <Commdlg.h>//操作文件选择
#include <gdiplus.h>

#include <CommCtrl.h>// 登录对话框

#include <mutex>
#include <thread>
#include <stdexcept>
#include <future>
#include <memory>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <stdarg.h>

#include <processthreadsapi.h> // 进程/线程API
#include <processenv.h>
#include <iterator>

#include <consoleapi.h>      // ConPTY API
#include <consoleapi2.h>    // 控制台API扩展
#include <wincon.h>          // 控制台常量
#include <richedit.h>       // RichEdit 核心头文件

#include <unordered_map>
#include "SSHUtil.h"

//#define WM_SSH_CONNECT_RESULT (WM_USER + 100)
// 自定义SSH消息体系（完全替代WM_）
//PostMessage(hWnd, SSH_KEYDOWN, wParam, lParam);
#define SSH_KEYDOWN        (WM_USER + 1000)
#define SSH_KEYUP          (WM_USER + 1001)
#define SSH_CHAR           (WM_USER + 1002)
#define SSH_PASTE          (WM_USER + 1003)
#define SSH_DEADCHAR       (WM_USER + 1004)
#define SSH_SYSKEYDOWN     (WM_USER + 1005)
#define SSH_SYSCHAR        (WM_USER + 1006)
#define SSH_SETFOCUS       (WM_USER + 1007)
#define SSH_KILLFOCUS      (WM_USER + 1008)
#define SSH_FIX_STATE      (WM_USER + 1009)
#define SSH_SET_READONLY   (WM_USER + 1010)
#define WM_APPEND_OUTPUT_TEXT (WM_USER + 2001)
#define MSG_FIX_SELECT_TRAIL_NEWLINE (WM_USER + 2002)
#define TIMER_ID_RESIZE_PTY (WM_USER + 2003)
#define WM_USER_RESIZE_PTY (WM_USER + 2004)

class SSHBasePanel;
class SSHTermPanel;
class SSHAppPanel;
class SSHTerminal;
enum class PanelType {
    SSHTermPanel = 1,        // SSHTermPanel面板类型
    SSHAppPanel = 2,         // SSHAppPanel面板类型
};

// 全局变量转发
struct NppData;
extern NppData& g_nppData;
extern HINSTANCE& g_hInst;
extern int& iconSize;
extern bool isSubclassTopWnd;

// 工具函数
void SSH_PanelVecBySeqIdUpdate(int startIndex);			// 根据删除的序列，后面的序列ID实例内容统一都向前移动
void SSH_PanelVecClearAll();							// 释放g_SSHPanelVec集合中所有面板


// 全局处理
SSHBasePanel* SSH_PanelVecBySeqId(int panelSeqId);
void SSH_PanelVecBySeqIdRemove(int panelSeqId, int panelrealId);			// 根据序列ID移除集合中的内容
SSHTermPanel* SSH_PanelVecBySeqIdGetSSHTermPanel(int panelSeqId);// 根据序列ID获得集合中的面板实例
SSHAppPanel* SSH_PanelVecBySeqIdGetSSHBtnPanel(int panelSeqId); // 根据序列ID获得集合中的SSHAppPanel面板实例
int SSH_PanelVecSize();									// 所有面板数量
int SSH_PanelVecGetInvalidSeqId();						// 查找缺失的第一个 序列ID 或者返回下一个最大 序列ID
bool SSH_PanelVecIsHasConnection();						// 检查所有面板中是否有连接
void SSH_HandAllFree();									// 关闭软件正确释放所有内容
template<typename Func>
//根据序列ID直接内部执行类中中的函数
void SSH_PanelVecBySeqIdExecFunc(int panelSeqId, Func&& func)
{
    SSHTermPanel* p = SSH_PanelVecBySeqIdGetSSHTermPanel(panelSeqId);
    if (p) func(p);
}

// 其他文件调用SSHSettings中的函数
std::wstring SSH_SettingsGetPluginsDir();//获取插件所在文件夹绝对路径(_T("%s\\plugins"))
void SSH_SettingsSavePanelCount(int count);				// Ini文件保存面板数量
int SSH_SettingsLoadPanelCount();						// Ini文件读取面板数量
void SSH_SettingsDeleteFile();							// Ini文件直接删除
bool SSH_SettingsSavePanelType(int panelRealId, PanelType type);// Ini文件根据面板标题ID保存面板类型
//PanelType SSH_LoadPanelTypeFromIni(int panelId);		// 读取INI：加载指定面板的类型
void SSH_SettingsInitRecreatePanels();					// 重建面板,根据不同的type创建不同的面板
void SSH_SettingsByRealIdRemove(int panelRealId);		// 删除指定面板ID对应的类型配置项，不存在直接返回不报错
//std::vector<PanelIdTypeItem> SSH_GetAllPanelIdTypeList(); // 获取全部面板ID与类型有序集合
void SSH_SettingsSaveConfigTmpFile(const std::wstring& ExceFile, const std::wstring& ExceComd);// 保存配置目录临时文件
std::wstring SSH_SettingsGetConfigFileExistPath(const std::wstring& ExceFile);// 查询配置目录文件，存在返回绝对路径，不存在返回空
void SSH_SettingsDeleteConfigFile(const std::wstring& ExceFile);// 直接删除配置目录指定文件（无判空、无返回值）


// 其他文件调用SSHTermPanel中的函数
void SSH_PanelInitRecreateSSHTermPanel(int panelSeqId, int panelRealId);	// 自动重建面板
void SSH_PanelInitRecreateSSHAppPanel(int panelSeqId, int panelrealId);
//HWND SSH_PanelGetLoginPanelHwnd();									//获得每次登录面板创建的句柄
HWND SSH_PanelGetPanelHwnd(int panelSeqId);							//根据面板ID获得面板句柄


// 其他文件调用SSHConnection中的函数
bool SSH_ConnectionHandle(int panelSeqId,const char* host, int port, const char* user, const char* pass);	// 连接操作
void SSH_ConnectionOnDisconn(int panelSeqId);				// 断开SSH连接
bool SSH_ConnectionIsConn(int panelSeqId);					// 判断是否连接
void SSH_ConnectionResetConn(int panelSeqId);				// 重置连接状态（暂未使用）
bool SSH_ConnectionExecuteCommand(int panelSeqId, const std::string& cmd); // 执行SSH命令
std::string SSH_ConnectionPanelPrompt(int panelSeqId);		// 获取命令提示词
void SSH_ConnectionPtySize(int panelSeqId, int cols, int rows);// 设置申请的Pty大小


// 其他文件调用SSHLog中的函数
// 日志转发接口（核心：只转发，不处理逻辑）
void NppSSH_Log_Init();                                                      // 初始化队列，准备日志打印
void NppSSH_LogDebug(const std::string& event, const std::string& content);  // 日志转发实现：调试级
void NppSSH_LogInfo(const std::string& event, const std::string& content);	 // 日志转发实现：Info级别
void NppSSH_LogWarn(const std::string& event, const std::string& content);   // 日志转发实现：警告级
void NppSSH_LogError(const std::string& event, const std::string& content);  // 日志转发实现：Error级别
// 简化封装：自动传入当前调用函数名作为事件（无需手动传event）
#define NppSSH_LogDebugAuto(content) NppSSH_LogDebug(__FUNCTION__, content)
#define NppSSH_LogInfoAuto(content) NppSSH_LogInfo(__FUNCTION__, content)
#define NppSSH_LogWarnAuto(content) NppSSH_LogWarn(__FUNCTION__, content)
#define NppSSH_LogErrorAuto(content) NppSSH_LogError(__FUNCTION__, content)


// 其他文件调用SSHTerminal中的函数
HWND SSH_TerminalInitControlPanel(HWND hParent, int panelSeqId);		// 初始化伪终端面板
void SSH_TerminalDisconnectHandle(int panelSeqId);						// 断开伪终端面板
void SSH_TerminalAppendTextHandle(int panelSeqId, const std::string& text);// 输出文本到伪终端，isPrompt设置追加后是否追加提示词
void SSH_TerminalSetPanelPrompt(int panelSeqId, const std::string prompt);// 设置伪终端面板命令提示词
void SSH_TerminalSetCommandRunning(int panelSeqId, bool isCommandRunning);// 设置伪终端命令执行中
void SSH_TerminalSetEnglishType(int panelSeqId);						// 第一次连接成功后默认是中文模式，修改为英文模式
void SSH_TerminalExecuteClear(int panelSeqId);							// 清空伪终端面板内容
std::string SSH_TerminalPanelPrompt(int panelSeqId);					// 获取终端命令提示符
void SSH_TerminalBySeqIdRemove(int panelSeqId);							// 根据序列ID移除面板
void SSH_TerminalBySeqIdReset(int panelSeqId);							// 重置面板（暂未使用）
void SSH_TerminalResize(HWND hParent, int panelSeqId);					// 调整伪终端面板大小