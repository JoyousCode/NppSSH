// SSHPanel.h（面板 + INI操作核心逻辑）
#pragma once
#include "SSHWindow.h"
#include "DockingFeature/DockingDlgInterface.h"
#include <shlwapi.h>
#include <algorithm>
#include <windowsx.h>
#pragma comment(lib, "shlwapi.lib")
//#define DWS_ICONTAB 0x0008
// 可停靠面板类（具体实现）
class NppSSHDockPanel : public DockingDlgInterface {
public:
    NppSSHDockPanel(int panelId);
    ~NppSSHDockPanel();

    bool isSSHConnected() const;
    void setSSHConnected(bool state);
    void disconnectSSH();// 断开当前面板的SSH连接
    void initPanel();// 面板初始化（纯原生接口，无多余字段）
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    void resetPanelToInit();//面板重置为初始状态（仅清空连接，不销毁）
    void UpdateToolbarIconSize();
    HICON LoadCustomIcon(int iconId, int size);
    void SetButtonIconOnly(HWND btn, int iconId);
    void OnConnect(HWND hWnd, NppSSHDockPanel* pPanel);
    
    // 官方标准模态登录窗口（修复NPP置底）
    void ShowSSHLoginWindow_Modal();

private:
    tTbData _dockData;      // 原生停靠数据结构体（需声明）
    int _panelId;           // 面板唯一ID，区分多标签
    int _iconSize = 28;     // 面板中按钮大小
    HWND _hOutputEdit;      // 输出编辑框句柄,面板内输出文本框
    HWND _hBtnConnectSSH;   // 连接SSH按钮句柄
    HWND _hBtnDisconnectSSH;// 断开SSH按钮句柄
    wchar_t _titleBuf[64];  // 面板标题缓冲区（成员变量，非静态！）
    bool _isSSHConnected;   //当前面板是否SSH登录成功  测试：true
    
    void createTopButtonBar();// 创建面板顶部按钮栏

    HICON _hIconConnect;    // 持久化连接图标句柄
    HICON _hIconDisconnect; // 持久化断开图标句柄
    HICON _hTabIcon;       // 持久化标签图标句柄
    // 官方对话框过程
    static INT_PTR CALLBACK SSH_LoginDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

};

// 全局变量封装（供SSHWindow调用）
std::vector<NppSSHDockPanel*>& SSHPanel_GetGlobalPanels();
std::atomic<int>& SSHPanel_GetGlobalPanelCounter();
NppData& SSHPanel_GetGlobalNppData();
HINSTANCE& SSHPanel_GetGlobalHInst();

// INI操作具体实现（替换原注册表函数）
void SSHPanel_SavePanelCountToIni(int count);
int SSHPanel_LoadPanelCountFromIni();
void SSHPanel_DeletePanelCountFromIni();

// NPP启动重建面板具体实现
void SSHPanel_RecreatePanelsOnNppStart();