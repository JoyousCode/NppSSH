// SSHTermPanel.h（面板 + INI操作核心逻辑）
#pragma once
#include "SSHBasePanel.h"
// 可停靠面板类（具体实现）
class SSHTermPanel : public SSHBasePanel {
public:
    SSHTermPanel(int panelSeqId, int panelrealId);
    ~SSHTermPanel() override;

    // 面板独有工具封装
    void disconnectSSH();               // 断开当前面板的SSH连接
    void setSSHConnected(bool state);   // 设置SSH是否连接
    HWND GethEditTermHandle() const { return _hEditTerm; }
    void UpdateToolbarIconSize();       //暂未使用
    
    // 必须重写的函数
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    void setBackgroundColor(COLORREF color) override;// 重写背景色
    void setForegroundColor(COLORREF color) override;// 重写前景文字色

    // 面板独有功能函数
    void initPanel();                           //初始化面板
    void createTopButtonBar();                  //初始化创建按钮控件
    //void createToolTip();                        //初始化创建工具提示(待开发)
    void SetButtonIconOnly(HWND btn, int iconId);//设置按钮图标
    HICON LoadCustomIcon(int iconId, int size);//加载自定义图标

    void SetBackgroundImage(const WCHAR* imgPath);// 设置面板背景图片
    HBITMAP LoadImageByGdiPlus(const WCHAR* filePath);// GDI+加载任意格式图片
    
    // 官方标准模态登录窗口（修复NPP置底）
    void ShowSSHLoginWindow_Modal();//已经废除
    void setLoginPanel(HWND hLoginPanel) {
        _hLoginPanel = hLoginPanel;
    }
    //HWND getLoginPanel() {//暂未使用
    //    return _hLoginPanel;
    //}
    // 加载位图函数，写在类内，调用时用 this->LoadBackgroundImage
    //HBITMAP LoadBackgroundImage(const wchar_t* filePath)
    //{
    //    return (HBITMAP)::LoadImage(
    //        NULL,
    //        filePath,
    //        IMAGE_BITMAP,
    //        0, 0,
    //        LR_LOADFROMFILE | LR_CREATEDIBSECTION
    //    );
    //}

private:
    // 官方对话框过程
    static INT_PTR CALLBACK SSH_LoginDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);//已经废除

    COLORREF _textColor = RGB(255, 255, 255);           // 字体颜色
    COLORREF _bgColor = GetSysColor(COLOR_WINDOW);      // 背景颜色
    COLORREF _fgColor = GetSysColor(COLOR_WINDOWTEXT);  // 前景颜色
    HBITMAP _hBgImage;                                  // 背景图片句柄
    
    HWND _hEditTerm;        // 输出编辑框句柄,面板内输出文本框
    HWND _hBtnConnectSSH;   // 连接SSH按钮句柄
    HWND _hBtnDisconnectSSH;// 断开SSH按钮句柄
    HWND _hLoginPanel;      //登录面板句柄(已废除)

    HICON _hIconConnect;    // 持久化连接图标句柄
    HICON _hIconDisconnect; // 持久化断开图标句柄

};

void SSHTermPanel_InitRecreatePanel(SSHBasePanel* pNewPanel);// NPP启动重建面板