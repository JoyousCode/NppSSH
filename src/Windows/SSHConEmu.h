#pragma once
//#include "SSHWindow.h"
#include "SSHBasePanel.h"
#include <Commdlg.h>//操作文件选择
#include <gdiplus.h>
#include <atomic>

class SSHConEmu : public SSHBasePanel{
public:
    SSHConEmu(int panelSeqId, int panelrealId);
    ~SSHConEmu() override;
    void initPanel();
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    void setBackgroundColor(COLORREF color) override;
    // 重写前景文字色
    void setForegroundColor(COLORREF color) override;
    void createButtonBar();
    HICON LoadCustomIcon(int iconId, int size);
    void SetButtonIconOnly(HWND btn, int iconId);
    void ShowPuttyLoginWindow_Modal();
    void SSHConEmu::CloseSoftWare();
    void OpenPuttyFileDialog();
    // 封装：统一设置路径区域所有控件字体大小（初始化自动调用）
    void SetPathControlFontSize(int fontSize);

    HBITMAP LoadImageByGdiPlus(const WCHAR* filePath);
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
    bool Set_hPuTTYWnd();

    void StartSeachPutty();
    void StopSeachPutty();
private:
    COLORREF m_textColor = RGB(255, 255, 255); // 文字白色适配图片
    COLORREF _bgColor = GetSysColor(COLOR_WINDOW);
    COLORREF _fgColor = GetSysColor(COLOR_WINDOWTEXT);
    HBITMAP m_hBgImage;
    HANDLE _hConEumProcess = nullptr;// ConEmu进程句柄
    HWND _hConEmuWnd = nullptr;      // ConEmu主窗口句柄（用于GuiMacro定位）

    
    HWND _hStaticPuttyTip;     // 静态文字：设置Putty路径：
    HWND _hEditPuttyPath;      // 路径输入框
    HWND _hBtnSelectFile;    // 浏览选择按钮
    std::wstring _strPuttyFullPath; // 存储选中的Putty完整路径
    HWND _hBtnPutty;        // Putty按钮句柄
    HWND _hBtnDestroy;      // 销毁按钮句柄
    HANDLE _hPuttyProcess; // 保存当前面板启动的PuTTY进程句柄
    HWND _hPuTTYWnd;
    HICON _hIconPutty;    // 持久化连接图标句柄
    HICON _hIconDestroy; // 持久化销毁按钮句柄
    HICON _hIconSelectFile; // 持久化销毁按钮句柄
    int _editLabelFontSize;//文字大小
    
    HWND _hBtnWinScp;// 

    // 后台搜索Putty线程，线程控制锁
    std::mutex _SeachPuttyMutex;
    std::thread _seachPuttyThread;
    void SeachPuttyThread();
    std::condition_variable _seachPuttyCv;
    std::atomic<bool> _stopSeachPutty{ false };

    std::wstring _TempExceFile;


};

// NPP启动重建面板具体实现
void SSHConEmu_InitRecreatePanel(SSHConEmu* pNewPanel);