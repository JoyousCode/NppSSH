#include "SSHBasePanel.h"
#include <Resource.h>

SSHBasePanel::SSHBasePanel(int panelSeqId, int panelrealId)
    : DockingDlgInterface(IDD_SSH_PANEL),
    _dockData(),
    _panelSeqId(panelSeqId),
    _panelrealId(panelrealId),
    _hTabIcon(NULL),
    _isConnected(false)
{
    ZeroMemory(_titleBuf, sizeof(_titleBuf));
}
SSHBasePanel::~SSHBasePanel()
{
    // 仅释放基类自己的图标/句柄
    if (_hTabIcon) ::DestroyIcon(_hTabIcon);
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