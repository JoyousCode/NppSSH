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
    //if (::IsWindow(_panelHwnd))::DestroyWindow(_panelHwnd);//会直接销毁，导致delete失效，析构函数不会被调用，导致内存泄漏，需要最后释放
	//delete this;会先执行派生类的析构函数，之后执行基类的析构函数。
	
    NppSSH_LogInfoAuto("执行SSHBasePanel析构函数2");
    GlobalUnsubclassTopWnd();
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

// ==== 挂载子类化 ====
bool SSHBasePanel::GlobalSubclassTopWnd() {
    if (isSubclassTopWnd && ::IsWindow(_panelHwnd)) {
        _hTopPanelHwnd = _panelHwnd;
        while (true)
        {
            HWND hTmpParent = ::GetParent(_hTopPanelHwnd);
            if (hTmpParent == nullptr)
                break;
            _hTopPanelHwnd = hTmpParent;
        }
        // 防重复子类化：判断当前WndProc是否已经是自定义过程
        WNDPROC curProc = (WNDPROC)GetWindowLongPtrW(_hTopPanelHwnd, GWLP_WNDPROC);
        if (curProc == SSHBasePanel::GlobalTopWndProc)
        {
            NppSSH_LogInfoAuto("面板已完成子类化，跳过");
        }
        else {
            // 获取原生窗口过程
            _oldTopPanelWndProc = (WNDPROC)::GetWindowLongPtrW(_hTopPanelHwnd, GWLP_WNDPROC);

            // 绑定当前面板实例到窗口属性
            swprintf_s(_titleParentBuf, _countof(_titleParentBuf), L"SSHTermPanel-%p", _hTopPanelHwnd);
            ::SetPropW(_hTopPanelHwnd, _titleParentBuf, (HANDLE)this);

            // 设置新窗口过程
            ::SetWindowLongPtrW(_hTopPanelHwnd, GWLP_WNDPROC, (LONG_PTR)GlobalTopWndProc);
        }
        NppSSH_LogInfoAuto("Notepad++软件子类化完成！hWnd=" + PtrToHexStr(_hTopPanelHwnd)
            + " 原过程：" + PtrToHexStr(_oldTopPanelWndProc)
            + " 新过程：" + PtrToHexStr(GlobalTopWndProc));
        isSubclassTopWnd = false;
    }
}
std::mutex g_panelVecMtx;
static thread_local bool s_bProcessingMsg = false;
LRESULT CALLBACK SSHBasePanel::GlobalTopWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    //bool isOk = WM_SETFONT || WM_INITDIALOG || WM_GETDLGCODE || WM_KILLFOCUS || WM_IME_SETCONTEXT || WM_SETFOCUS;
    char mbuf[64] = { 0 };
    sprintf(mbuf, "0x%04X", msg);
    std::string msgStr(mbuf);
    //NppSSH_LogInfoAuto("【拦截GlobalTopWndProc】消息message===" + msgStr);

    LRESULT res = 0;
    wchar_t buf[128]{};
    swprintf_s(buf, _countof(buf), L"SSHTermPanel-%p", hWnd);
    SSHBasePanel* dockPanel = reinterpret_cast<SSHBasePanel*>(GetProp(hWnd, buf));
    WNDPROC oldWndProc = dockPanel->Get_oldTopPanelWndProc();

    if (!dockPanel || s_bProcessingMsg) {
        NppSSH_LogInfoAuto("GlobalTopWndProc未找到终端！hWnd=" + PtrToHexStr(hWnd) + " msg=" + msgStr);
        //WNDPROC realCurWndProc = (WNDPROC)GetWindowLongPtrW(hWnd, GWLP_WNDPROC);
        res = DefWindowProcW(hWnd, msg, wParam, lParam);
        if (oldWndProc != nullptr)
        {
            res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
        }
        return res;
    }

    
    if (!oldWndProc) {
        NppSSH_LogInfoAuto("原始的窗口过程未找到！hWnd=" + PtrToHexStr(hWnd) + " msg=" + msgStr);
        res = DefWindowProc(hWnd, msg, wParam, lParam);
        s_bProcessingMsg = true;
        return res;
    }
    // 区分需要拦截的非客户区消息，其余直接放行（和TerminalEditProc筛选逻辑对齐）
    bool isNCInterceptMsg = (msg == WM_CLOSE);
    if (!isNCInterceptMsg) {
        return CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
    }
    switch (msg)
    {
    case WM_CLOSE:
    {
        bool hasActiveConn = SSH_PanelVecIsHasConnection();
        if (hasActiveConn)
        {
            int closeResult = ::MessageBoxW(hWnd,
                L"存在活跃SSH连接，关闭Notepad++将全部断开，确认退出？",
                L"NppSSH 连接提示",
                MB_YESNO | MB_ICONWARNING);
            // 用户取消：拦截关闭，不传递WM_CLOSE给原生窗口
            if (closeResult != IDYES)
            {
                s_bProcessingMsg = true;
                return 0;
            }
        }
        // 用户确认放行，执行原生关闭逻辑
        res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
        s_bProcessingMsg = true;
        return res;
    }
    default: {// 其余消息交给原始窗口过程处理
        res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
        s_bProcessingMsg = true;//放重入
        return res;
    }
    }
    // 其余消息交给原始窗口过程处理
    res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
    s_bProcessingMsg = true;
    return res;
}

void SSHBasePanel::GlobalUnsubclassTopWnd() {
    // 1. 反子类化，恢复原始窗口过程
    if (::IsWindow(_hTopPanelHwnd) && _oldTopPanelWndProc != nullptr)
    {
        WNDPROC curProc = (WNDPROC)GetWindowLongPtrW(_hTopPanelHwnd, GWLP_WNDPROC);
        // 仅当前是自定义过程才恢复，防止重复恢复
        if (curProc == GlobalTopWndProc)
        {
            ::SetWindowLongPtrW(_hTopPanelHwnd, GWLP_WNDPROC, (LONG_PTR)_oldTopPanelWndProc);
            NppSSH_LogInfoAuto("面板析构：恢复顶层窗口原始WndProc");
        }

        // 2. 删除窗口绑定的面板指针Prop，彻底清除野指针关联
        swprintf_s(_titleParentBuf, _countof(_titleParentBuf), L"SSHTermPanel-%p", _hTopPanelHwnd);
        ::RemovePropW(_hTopPanelHwnd, _titleParentBuf);
        NppSSH_LogInfoAuto("面板析构：移除窗口Prop绑定"+ IntToStr(SSH_PanelVecSize()));
        if(SSH_PanelVecSize()==1)isSubclassTopWnd = true;
    }
}