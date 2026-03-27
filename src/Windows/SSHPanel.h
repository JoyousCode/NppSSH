// SSHPanel.h（面板 + 注册表核心逻辑）
#pragma once
#include "SSHWindow.h"
#include "DockingFeature/DockingDlgInterface.h"
#include <shlwapi.h>
#include <algorithm>
#include <windowsx.h>
#pragma comment(lib, "shlwapi.lib")

// 核心：可停靠面板类（具体实现）
class NppSSHDockPanel : public DockingDlgInterface {
public:
    NppSSHDockPanel(int panelId);
    ~NppSSHDockPanel() override = default;

    bool isSSHConnected() const;
    void setSSHConnected(bool state);
    void disconnectSSH();// 断开当前面板的SSH连接
    void initPanel();// 面板初始化（纯原生接口，无多余字段）
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    void resetPanelToInit();//面板重置为初始状态（仅清空连接，不销毁）

private:
    tTbData _dockData;      // 原生停靠数据结构体（需声明）
    int _panelId;           // 面板唯一ID，区分多标签
    HWND _hOutputEdit;      // 输出编辑框句柄,面板内输出文本框
    wchar_t _titleBuf[64];  // 面板标题缓冲区（成员变量，非静态！）
    bool _isSSHConnected;   //当前面板是否SSH登录成功  测试：true
};

// 全局变量封装（供SSHWindow调用）
std::vector<NppSSHDockPanel*>& SSHPanel_GetGlobalPanels();
std::atomic<int>& SSHPanel_GetGlobalPanelCounter();
NppData& SSHPanel_GetGlobalNppData();
HINSTANCE& SSHPanel_GetGlobalHInst();

// 注册表操作具体实现
void SSHPanel_SavePanelCountToReg(int count);
int SSHPanel_LoadPanelCountFromReg();
void SSHPanel_DeletePanelCountFromReg();

// NPP启动重建面板具体实现
void SSHPanel_RecreatePanelsOnNppStart();