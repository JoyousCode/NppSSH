#pragma once
#include "SSHBasePanel.h"
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
    bool isWindowTop;
    bool isNeedDelete;
    

    PuTTYSession(int _panelSeqId)
		: hProcess(NULL), hWnd(NULL), stopFlag(true), hMonitorThread(nullptr), panelSeqId(_panelSeqId), tmpFile(L""), isWindowTop(false), isNeedDelete(false)
    {}
    ~PuTTYSession()
    {
        // 只在句柄还没被置空的时候关闭
        if (hMonitorThread != nullptr)
        {
            NppSSH_LogInfoAuto("PuTTYSession析构：关闭hMonitorThread");
            CloseHandle(hMonitorThread);
            hMonitorThread = nullptr;
        }
        CleanHandle(); // 进程句柄也在这里兜底释放
    }
    DWORD WINAPI PuTTYSessionMonitor();
    bool FindPuTTYWindowByPid(DWORD pid, HWND& outHwnd);
    void StopMonitor(){// 终止监控线程（暂未使用，已废弃，在SSHAppPanel析构函数中安全停止）
        stopFlag.store(true, std::memory_order_release);
        cv.notify_all();//cv.notify_one();
        if (hMonitorThread != nullptr){
            WaitForSingleObject(hMonitorThread, INFINITE);
            CloseHandle(hMonitorThread);
            hMonitorThread = nullptr;
        }
    }
    // 释放进程句柄
    void CleanHandle(){
        if (hProcess != NULL){
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
    // 面板独有工具封装
    static DWORD WINAPI MonitorThreadProxy(LPVOID lpParam);// 静态代理，仅作PuTTYSessionMonitor合法入口，不写业务逻辑
	bool RemoveTerminatedSession(PuTTYSession* pSess);  // 移除已终止的会话
    bool isHandleHasActiveThread();                     // 检查句柄是否有活动线程
    bool hasConnection();                               // 检查是否有活动线程

    // 必须重写的函数
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    void setBackgroundColor(COLORREF color) override;
    void setForegroundColor(COLORREF color) override;

    // 面板独有功能函数
    void initPanel();                           //初始化面板
    void createButtonBar();                     //初始化创建按钮控件
    void createToolTip();                        //初始化创建工具提示
	void SetButtonIconOnly(HWND btn, int iconId);//设置按钮图标
	HICON LoadCustomIcon(int iconId, int size); //加载自定义图标
	void OpenPuttyFileDialog();                 // 打开文件选择对话框
    void SetPathControlFontSize(int fontSize);  //初始化统一设置路径区域所有控件字体大小
	void UpdateToolTipMessage(HWND _hHWND, const WCHAR* tipText);// 更新指定控件的工具提示

	void SetBackgroundImage(const WCHAR* imgPath);// 设置面板背景图片
	HBITMAP LoadImageByGdiPlus(const WCHAR* filePath);// GDI+加载任意格式图片

    //void ShowPuttyLoginWindow_Modal();          //点击连接Putty按钮
	bool puttyLoginPathHandle();                  //点击连接Putty按钮
    bool SSHAppPanel_PuttyLoginHandle(std::wstring host, std::wstring port, std::wstring user, std::wstring pass, std::wstring director);
    void CloseSoftWare();                       //点击销毁按钮，关闭所有PuTTY会话

	void UpdateWinTopBtnUI_FromMemState();// 根据内存状态更新窗口置顶按钮UI
private:
    std::vector<PuTTYSession*> _sessionList;// 所有PuTTY会话容器
    std::mutex _sessionListMtx;             // 保护会话列表并发读写

    COLORREF _textColor = RGB(255, 255, 255);          // 字体颜色
    COLORREF _bgColor = GetSysColor(COLOR_WINDOW);      // 背景颜色
	COLORREF _fgColor = GetSysColor(COLOR_WINDOWTEXT);  // 前景颜色
	HBITMAP _hBgImage;                                 // 背景图片句柄

    HWND _hStaticPuttyTip;          // 显示静态文字句柄
    HWND _hEditPuttyPath;           // 路径输入框句柄
    HWND _hBtnSelectFile;           // 选择文件按钮句柄
    std::wstring _strPuttyFullPath; // 存储选择文件按钮选中的完整路径
    HWND _hBtnPutty;                // 连接Putty按钮句柄
    HWND _hBtnDestroy;              // 销毁所有连接Putty窗口按钮句柄
    HWND _hBtnWinTop;               // 窗口置顶按钮句柄
	bool _winTopState;              // 窗口置顶状态
    HWND _hToolTip;                 // 工具提示控件

    HICON _hIconPutty;              // 连接Putty按钮图标句柄
    HICON _hIconDestroy;            // 销毁所有连接Putty窗口按钮图标句柄
	HICON _hIconWinTop;             // 窗口置顶按钮图标句柄
    HICON _hIconSelectFile;         // 选择文件按钮图标句柄
    int _editLabelFontSize;         // 文字大小
    
    HWND _hBtnWinScp;// 待定
};

void windowTopBtnHandle(HWND hWnd, bool isWinTop);              //窗口置顶按钮处理
void SSHAppPanel_InitRecreatePanel(SSHBasePanel* pNewPanel);    // NPP启动重建面板对外调用
