#include "SSHBasePanel.h"
#include <Resource.h>

SSHBasePanel::SSHBasePanel(int panelSeqId, int panelrealId)
    : DockingDlgInterface(IDD_SSH_PANEL),
    _dockData(),
    _panelSeqId(panelSeqId),
    _panelrealId(panelrealId),
    _hTabIcon(NULL),
    _iconSize(24), // 默认工具栏图标尺寸
    _isConnected(false)
{
    ZeroMemory(_titleBuf, sizeof(_titleBuf));
}
SSHBasePanel::~SSHBasePanel()
{
    
    // 仅释放基类自己的图标/句柄
    if (_hTabIcon) ::DestroyIcon(_hTabIcon);
    //NppSSH_LogInfoAuto("执行SSHBasePanel析构函数1");
    if (::IsWindow(_panelHwnd))::DestroyWindow(_panelHwnd);//会直接销毁，导致delete失效，析构函数不会被调用，导致内存泄漏，需要最后释放
	//delete this;会先执行派生类的析构函数，之后执行基类的析构函数。
	
    //NppSSH_LogInfoAuto("执行SSHBasePanel析构函数2");
}
/*
* 根据面板ID获得面板句柄
*/
HWND SSHPanel_GetPanelHwnd(int panelSeqId) {
    SSHBasePanel* p = SSH_PanelVecBySeqId(panelSeqId);
    if (!p) return NULL;
    return p->Get_panelHwnd();
}
void SSHBasePanel::setBackgroundColor(COLORREF color) {

}
// 重写前景文字色
void SSHBasePanel::setForegroundColor(COLORREF color) {

}


INT_PTR CALLBACK SSHBasePanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) { return DockingDlgInterface::run_dlgProc(message, wParam, lParam); };