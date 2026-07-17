#pragma once
#include "SSHBasePanel.h"
#include <Commdlg.h>//操作文件选择
#include <gdiplus.h>
#include <atomic>
// 单个PuTTY会话完整资源包
struct PuTTYSession
{
	int panelSeqId;// 面板序列ID（用于关联面板设置是否存在连接）
    HANDLE hProcess;
    HWND hWnd;
    HANDLE hMonitorThread;
    std::atomic<bool> stopFlag;
    std::mutex mtx;
    std::condition_variable cv;
    std::wstring tmpFile;
    //std::atomic<bool> closeFlag;

    PuTTYSession(int _panelSeqId)
        : hProcess(NULL), hWnd(NULL), stopFlag(false), hMonitorThread(nullptr), panelSeqId(_panelSeqId)
    {
    }
    DWORD WINAPI PuTTYSessionMonitor();
    bool FindPuTTYWindowByPid(DWORD pid, HWND& outHwnd);
    // 终止监控线程
    void StopMonitor()
    {
        stopFlag.store(true, std::memory_order_release);
        //cv.notify_one();
        cv.notify_all();
        if (hMonitorThread != nullptr)
        {
            WaitForSingleObject(hMonitorThread, INFINITE);
            CloseHandle(hMonitorThread);
            hMonitorThread = nullptr;
        }
    }

    // 释放进程句柄
    void CleanHandle()
    {
        if (hProcess != NULL)
        {
            CloseHandle(hProcess);
            hProcess = NULL;
        }
        hWnd = NULL;
    }
};
class SSHAppPanel : public SSHBasePanel{
public:
    SSHAppPanel(int panelSeqId, int panelrealId);
    ~SSHAppPanel() override;
    void initPanel();
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    void setBackgroundColor(COLORREF color) override;
    // 重写前景文字色
    void setForegroundColor(COLORREF color) override;
    void createButtonBar();
    HICON LoadCustomIcon(int iconId, int size);
    void SetButtonIconOnly(HWND btn, int iconId);
    void ShowPuttyLoginWindow_Modal();
    void CloseSoftWare();
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

    // 静态代理，仅作为CreateThread合法入口，不写业务逻辑
    static DWORD WINAPI MonitorThreadProxy(LPVOID lpParam);
    void CleanInvalidSession();
    bool isHandleHasActiveThread();
private:
    COLORREF m_textColor = RGB(255, 255, 255); // 文字白色适配图片
    COLORREF _bgColor = GetSysColor(COLOR_WINDOW);
    COLORREF _fgColor = GetSysColor(COLOR_WINDOWTEXT);
    HBITMAP m_hBgImage;

    HWND _hStaticPuttyTip;     // 静态文字：设置Putty路径：
    HWND _hEditPuttyPath;      // 路径输入框
    HWND _hBtnSelectFile;    // 浏览选择按钮
    std::wstring _strPuttyFullPath; // 存储选中的Putty完整路径
    HWND _hBtnPutty;        // Putty按钮句柄
    HWND _hBtnDestroy;      // 销毁按钮句柄

    HICON _hIconPutty;    // 持久化连接图标句柄
    HICON _hIconDestroy; // 持久化销毁按钮句柄
    HICON _hIconSelectFile; // 持久化销毁按钮句柄
    int _editLabelFontSize;//文字大小
    
    HWND _hBtnWinScp;// 


    // 所有PuTTY会话容器
    std::vector<PuTTYSession*> _sessionList;
    std::mutex _sessionListMtx;// 保护会话列表并发读写
};

// NPP启动重建面板具体实现
void SSHAppPanel_InitRecreatePanel(SSHBasePanel* pNewPanel);