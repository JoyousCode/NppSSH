// SSHTermPanel.h（面板 + INI操作核心逻辑）
#pragma once
#include "SSHWindow.h"
//#include "DockingFeature/DockingDlgInterface.h"
#include "SSHBasePanel.h"
#include <shlwapi.h>
#include <algorithm>
#include <windowsx.h>
#include <gdiplus.h>

// 兼容普通Edit的GETTEXTRANGE

#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <consoleapi2.h>
#include <processenv.h>
//#include <commctrl.h>
//#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

// 可停靠面板类（具体实现）
class SSHTermPanel : public SSHBasePanel {
public:
    SSHTermPanel(int panelSeqId, int panelrealId);
    ~SSHTermPanel() override;
    // 窗口句柄获取（原有）
    HWND getHSelf() const { return  GetHwndSelf(); } // 需确保_hSelf已声明
    // 焦点状态设置
    void SetFocused(bool focused) { _isFocused = focused; };
   
    HWND GetOutputEditHandle() const { return _hOutputEdit; }
    int getIconSize() { return _iconSize; }

    //bool isSSHConnected() const;
    void setSSHConnected(bool state);
    void disconnectSSH();// 断开当前面板的SSH连接
    void initPanel();// 面板初始化（纯原生接口，无多余字段）
    //INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    void resetPanelToInit();//面板重置为初始状态（仅清空连接，不销毁）
    void UpdateToolbarIconSize();
    HICON LoadCustomIcon(int iconId, int size);
    void SetButtonIconOnly(HWND btn, int iconId);
    void OnConnect(HWND hWnd, SSHTermPanel* pPanel);
    
    // 官方标准模态登录窗口（修复NPP置底）
    void ShowSSHLoginWindow_Modal();
    void setLoginPanel(HWND hLoginPanel) {
        _hLoginPanel = hLoginPanel;
    }
    HWND getLoginPanel() {
        return _hLoginPanel;
    }
    HWND Get_hTopParent() const { return _hTopParent; }
    WNDPROC Get_oldPanelWndProc() const { return _oldPanelWndProc; }
    void Set_oldPanelWndProc(WNDPROC oldPanelWndProc) { _oldPanelWndProc = oldPanelWndProc; }
    // 子类化Notepad++软件面板过程监听
    static LRESULT CALLBACK PanelSubclassWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // 重写背景色
    void setBackgroundColor(COLORREF color) override
    {
        _bgColor = color;
        // 刷新面板，触发WM_ERASEBKGND、WM_PAINT重绘
        ::InvalidateRect(GetHwndSelf(), nullptr, TRUE);
    }
    // 重写前景文字色
    void setForegroundColor(COLORREF color) override
    {
        _fgColor = color;
        ::InvalidateRect(GetHwndSelf(), nullptr, TRUE);
    }
    // 加载位图函数，写在类内，调用时用 this->LoadBackgroundImage
    HBITMAP LoadBackgroundImage(const wchar_t* filePath)
    {
        return (HBITMAP)::LoadImage(
            NULL,
            filePath,
            IMAGE_BITMAP,
            0, 0,
            LR_LOADFROMFILE | LR_CREATEDIBSECTION
        );
    }
    HBITMAP LoadImageByGdiPlus(const WCHAR* filePath);
    // 加载背景图，传入图片完整路径
    void SetBackgroundImage(const WCHAR* imgPath)
    {
        // 释放旧图片
        if (m_hBgImage)
        {
            DeleteObject(m_hBgImage);
            m_hBgImage = NULL;
        }
        // GDI+加载任意格式图片
        m_hBgImage = LoadImageByGdiPlus(imgPath);
        InvalidateRect(GetHwndSelf(), nullptr, TRUE);
    }
protected://只能被子类用
    HBITMAP m_hBgImage;
    COLORREF m_textColor = RGB(255, 255, 255); // 文字白色适配图片
    COLORREF _bgColor = GetSysColor(COLOR_WINDOW);
    COLORREF _fgColor = GetSysColor(COLOR_WINDOWTEXT);
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    // 官方对话框过程
    static INT_PTR CALLBACK SSH_LoginDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
private:
    tTbData _dockData;      // 原生停靠数据结构体（需声明）
    
    
    HWND _hOutputEdit;      // 输出编辑框句柄,面板内输出文本框
    bool _isFocused;        // 标记当前面板是否获焦
    HWND _hBtnConnectSSH;   // 连接SSH按钮句柄
    HWND _hBtnDisconnectSSH;// 断开SSH按钮句柄
    
    HWND _hTopParent;       //notepad++软件句柄
    HWND _hLoginPanel;      //登录面板句柄
    //wchar_t _titleBuf[64];  // 面板标题缓冲区（成员变量，非静态！）
    wchar_t _titleParentBuf[64];
    //bool _isSSHConnected;   //当前面板是否SSH登录成功  测试：true
    
    void createTopButtonBar();// 创建面板顶部按钮栏

    
    HICON _hIconConnect;    // 持久化连接图标句柄
    HICON _hIconDisconnect; // 持久化断开图标句柄
    
    

    WNDPROC _oldPanelWndProc = nullptr; // 传统子类化保存旧过程

};

// 全局变量封装
NppData& SSHTermPanel_GetGlobalNppData();
HINSTANCE& SSHTermPanel_GetGlobalHInst();

// NPP启动重建面板具体实现
void SSHTermPanel_InitRecreatePanel(SSHBasePanel* pNewPanel);

// 获取面板索引进行转发

int& SSHTermPanel_iconSize();