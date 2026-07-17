#pragma once
#include "SSHWindow.h"
#include "SSHClassUtil.h"
#include "DockingFeature/DockingDlgInterface.h"

class SSHBasePanel : public DockingDlgInterface{
    //Getting/Setting 自动生成Getting/Setting方法
    GEN_GET_SET_ONLY(int, _panelrealId);
    GEN_GET_SET_ONLY(HWND, _panelHwnd);
    GEN_GET_SET_ONLY(int, _isConnected);
    GEN_GET_SET_ONLY(WNDPROC, _oldTopPanelWndProc);
    GEN_GET_SET_Protected(int, _panelSeqId);
public:
    SSHBasePanel(int panelSeqId, int panelrealId);
    //virtual ~SSHBasePanel() = default;
    virtual ~SSHBasePanel();

    
    // 纯虚接口，所有面板必须实现
    //virtual void UpdateUI() = 0;//0代表 只有声明，如果某个子类没有重写全部纯虚函数，该子类也会变成抽象类，无法new实例
    //virtual void ClosePanel() = 0;

    //Getting/Setting 自动生成Getting/Setting方法

        
protected://只能被子类用
    void setBackgroundColor(COLORREF color) override;
    // 重写前景文字色
    void setForegroundColor(COLORREF color) override;
    HWND GetHwndSelf() const { return _hSelf; }
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;

    bool GlobalSubclassTopWnd();
    // 子类化Notepad++软件面板过程监听
    static LRESULT CALLBACK GlobalTopWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    // 派生面板调用：计数归零后恢复原生窗口过程
    void GlobalUnsubclassTopWnd();

    //int _panelSeqId;        //已由GEN_GET_SET_Protected进行自动生成
    int _panelrealId;  
    wchar_t _titleBuf[64];  // 面板标题缓冲区（成员变量，非静态！）
    tTbData _dockData;      // 原生停靠数据结构体（需声明）
    HICON _hTabIcon;       // 持久化标签图标句柄
    HWND _panelHwnd;       // 持久化面板句柄
    HWND _hTopPanelHwnd;       //notepad++软件句柄
    WNDPROC _oldTopPanelWndProc = nullptr; // 传统子类化保存旧过程
    wchar_t _titleParentBuf[64];// 存储绑定当前面板实例到窗口属性

    int _iconSize = 28;     // 面板中按钮大小
    bool _isConnected;   //当前面板连接状态
private:
    
};

HWND SSHPanel_GetPanelHwnd(int panelId);