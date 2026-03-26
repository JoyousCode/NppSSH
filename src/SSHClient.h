//#define WIN32_LEAN_AND_MEAN
//#define _WINSOCKAPI_
//#define NOMINMAX
#pragma once
//#ifndef SSHCLIENT_H
//#define SSHCLIENT_H
// 
/**
防止头文件被重复包含
#ifndef SSHCLIENT_H
#define SSHCLIENT_H
....具体内容

#endif

最新写法：#pragma once
*/
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
class NppSSHDockPanel;
extern std::vector<NppSSHDockPanel*> g_sshPanels;
//// 资源ID定义（无需rc文件，直接宏定义，避免新建文件）
//#define IDD_SSH_PANEL 1001  // 面板对话框唯一ID
//#define IDC_OUTPUT_EDIT 1002// 输出文本框ID

//// 前置声明：避免循环依赖
//#ifdef __cplusplus
//extern "C" {
//#endif
//extern NppData g_nppData;
//extern HINSTANCE g_hInst;
//
//// 核心：基于原生DockingDlgInterface的可停靠面板类（多实例/多标签）
class NppSSHDockPanel : public DockingDlgInterface {
public:
    // 构造函数：传入面板ID，初始化对话框ID
    //NppSSHDockPanel(int panelId) : DockingDlgInterface(IDD_SSH_PANEL), _panelId(panelId) {}
    //~NppSSHDockPanel() override = default;
    // 构造函数
    //NppSSHDockPanel(int panelId) : DockingDlgInterface(IDD_SSH_PANEL), _panelId(panelId), _hOutputEdit(NULL) {}
    NppSSHDockPanel(int panelId) : DockingDlgInterface(IDD_SSH_PANEL),
        _dockData(),
        _panelId(panelId),
        _hOutputEdit(NULL),
        _titleBuf(),
        _isSSHConnected(false) {// 测试：true
    }

    bool isSSHConnected() const;
    void setSSHConnected(bool state);
    void disconnectSSH();   // 断开当前面板的SSH连接


    // 面板初始化（纯原生接口，无多余字段）
    void initPanel();
    // 重写原生窗口过程：处理UI创建/消息
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    tTbData _dockData;     // 原生停靠数据结构体（需声明）
    int _panelId;               // 面板唯一ID，区分多标签
    HWND _hOutputEdit;          // 输出编辑框句柄,面板内输出文本框
    wchar_t _titleBuf[64] = { 0 };// 面板标题缓冲区（成员变量，非静态！）
    bool _isSSHConnected = false;  //当前面板是否SSH登录成功// 测试：true
};



void CreateNppSSHTerminal();



//#ifdef __cplusplus
//}
//#endif
//#endif
