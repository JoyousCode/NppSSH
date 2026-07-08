#pragma once
#include "SSHWindow.h"
//#include "SSHClassUtil.h"
#include "DockingFeature/DockingDlgInterface.h"

class SSHBasePanel : public DockingDlgInterface{
    //GEN_GET_SET_ONLY(int, _panelrealId)
    //    GEN_GET_SET_ONLY(HWND, _panelHwnd)
    //    GEN_GET_SET_ONLY(int, _isConnected)
    //    GEN_GET_SET_ONLY(int, _panelSeqId)
public:
    SSHBasePanel(int panelSeqId, int panelrealId);
    //virtual ~SSHBasePanel() = default;
    virtual ~SSHBasePanel();
    // 纯虚接口，所有面板必须实现
    //virtual void UpdateUI() = 0;//0代表 只有声明，如果某个子类没有重写全部纯虚函数，该子类也会变成抽象类，无法new实例
    //virtual void ClosePanel() = 0;

    //Getting/Setting
    int Get_panelrealId() const { return _panelrealId; }
    void Set_panelrealId(int panelrealId) { _panelrealId = panelrealId; }
    int Get_panelSeqId() const { return _panelSeqId; }
    void Set_panelSeqId(int panelSeqId) { _panelSeqId = panelSeqId; }
    int Get_isConnected() const { return _isConnected; }
    void Set_isConnected(int isConnected) { _isConnected = isConnected; }
    HWND Get_panelHwnd() const { return _panelHwnd; }
    void Set_panelHwnd(HWND panelHwnd) { _panelHwnd = panelHwnd; }
        //GEN_GET_SET_Protected(int, _panelSeqId)

        void setBackgroundColor(COLORREF color) override;
        // 重写前景文字色
        void setForegroundColor(COLORREF color) override;
        //HWND GetHwndSelf() const { return _hSelf; };
        
protected://只能被子类用
    HWND GetHwndSelf() const { return _hSelf; }
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    int _panelSeqId;        
    int _panelrealId;  
    wchar_t _titleBuf[64];  // 面板标题缓冲区（成员变量，非静态！）
    tTbData _dockData;      // 原生停靠数据结构体（需声明）
    HICON _hTabIcon;       // 持久化标签图标句柄
    HWND _panelHwnd;       // 持久化面板句柄
    bool _isConnected;   //当前面板连接状态
private:
    
};

HWND SSHPanel_GetPanelHwnd(int panelId);