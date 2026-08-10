#include "SSHBasePanel.h"

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
// 面板初始化：纯原生接口
bool SSHBasePanel::initDockData() {
    // 检查资源是否存在
    HRSRC hRes = ::FindResource(g_hInst, MAKEINTRESOURCE(IDD_SSH_PANEL), RT_DIALOG);
    if (hRes == NULL) {
        wchar_t errMsg[256] = { 0 };
        swprintf_s(errMsg, L"找不到IDD_SSH_PANEL资源！GetLastError: %d", ::GetLastError());
        ::MessageBoxW(g_nppData._nppHandle, errMsg, L"NppSSH资源错误", MB_OK | MB_ICONERROR);
        return false;
    }

    DockingDlgInterface::init(g_hInst, g_nppData._nppHandle);   // 调用DockingDlgInterface原生init：绑定NPP实例和父窗口
    ZeroMemory(&_dockData, sizeof(tTbData));                    // 初始化原生tTbData结构体（完全按Docking.h定义，无多余成员）

    // 面板标签名（多标签区分：NppSSH-1、NppSSH-2...，NPP底部标签栏显示）
    std::wstring panelTitle = L"NppSSH-" + std::to_wstring(_panelrealId);
    wcscpy_s(_titleBuf, _countof(_titleBuf), panelTitle.c_str());

    _hTabIcon = (HICON)::LoadImage(
        g_hInst,
        MAKEINTRESOURCE(IDI_ICON_NPPSSH),
        IMAGE_ICON,
        16, 16,
        LR_DEFAULTCOLOR | LR_SHARED
    );
    if (_hTabIcon == NULL) {
        _hTabIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    _dockData.pszName = _titleBuf;                           // 原生成员：面板名称
    _dockData.uMask = DWS_DF_CONT_BOTTOM | DWS_DF_FLOATING | DWS_ICONTAB;  // 面板默认停靠在底部和允许面板浮动为独立窗口
    _dockData.iPrevCont = CONT_BOTTOM;                       // 原生要求：记录上一次停靠位置为底部
    _dockData.dlgID = IDD_SSH_PANEL;                        // 原生成员：对话框ID
    _dockData.pszModuleName = this->getPluginFileName();    // 原生方法：获取插件模块名（NPP识别用）
    _dockData.hIconTab = _hTabIcon;                           // 标签图标
    _dockData.pszAddInfo = nullptr;                         // 无额外信息，设为null
    //wchar_t* pFlag = new wchar_t[32];
    //wcscpy_s(pFlag, 32, L"NO_CLOSE_BUTTON");
    //_dockData.pszAddInfo = pFlag;

    // 调用DockingDlgInterface原生create：绑定停靠数据，创建面板窗口
    StaticDialog::create(_dlgID, false);

    //DockingDlgInterface::create(&_dockData);
    //StaticDialog::create(IDD_SSH_PANEL);//固定面板，适合单一 SSH 面板

    DWORD dwStyle = ::GetWindowLongPtrW(_hSelf, GWL_STYLE);
    SetWindowLongPtrW(_hSelf, GWL_STYLE, dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_SYSMENU);

    _dockData.hClient = _hSelf;//_hSelf
    if (!_hSelf) {
        ::MessageBoxW(g_nppData._nppHandle, L"面板窗口创建失败！", L"NppSSH错误", MB_OK | MB_ICONERROR);
        return false;
    }
    _panelHwnd = _hSelf;
    // 注册面板到NPP停靠管理器
    ::SendMessage(g_nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&_dockData));
    ::SendMessage(g_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD, reinterpret_cast<LPARAM>(_hSelf));
    return true;
}

// 挂载Notepad++软件子类化
bool SSHBasePanel::GlobalSubclassTopWnd() {
    if (isSubclassTopWnd && ::IsWindow(_panelHwnd)) {
        _hTopPanelHwnd = _panelHwnd;
        while (true)//TODO存在已完成子类化，跳过前赋值的问题。
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
    return true;
}
//static thread_local bool s_bProcessingMsg = false;
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
    bool isNCInterceptMsg = (msg == WM_CLOSE);
    if (!isNCInterceptMsg) {
        //res = DefWindowProcW(hWnd, msg, wParam, lParam);
        //if (oldWndProc)
        //{
        //    NppSSH_LogInfoAuto("isNCInterceptMsg原始的窗口过程未找到！hWnd=" + PtrToHexStr(hWnd) + " msg=" + msgStr);
        //    res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
        //}
        res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
        return res;
    }
    //处理isNCInterceptMsg设置的拦截消息
    if ((!dockPanel || !oldWndProc)) {
        if (!dockPanel) { NppSSH_LogInfoAuto("GlobalTopWndProc未找到终端！hWnd=" + PtrToHexStr(hWnd) + " msg=" + msgStr); }
        res = DefWindowProcW(hWnd, msg, wParam, lParam);
        if (oldWndProc)
        {
            res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
        }
        else {
            NppSSH_LogInfoAuto("原始的窗口过程未找到！hWnd=" + PtrToHexStr(hWnd) + " msg=" + msgStr);

        }
        return res;
    }

    //if (s_bProcessingMsg) {
    //    s_bProcessingMsg = false;
    //    res = 0;
    //    return res;
    //}
    NppSSH_LogInfoAuto("Notepad++窗口过程！hWnd=" + PtrToHexStr(hWnd) + " msg=" + msgStr);

    switch (msg)
    {
    case WM_CLOSE:
    {
        bool hasActiveConn = SSH_PanelVecIsHasConnection();
        if (hasActiveConn)
        {
            int closeResult = ::MessageBoxW(hWnd,
                L"存在活跃SSH连接，关闭Notepad++软件将全部断开!\n\n请确认是否继续退出？",
                L"NppSSH 提示",
                MB_YESNO | MB_ICONWARNING);
            // 用户取消：拦截关闭，不传递WM_CLOSE给原生窗口
            if (closeResult != IDYES)
            {
                res = 0;
                return res;
            }
        }
        // 用户确认放行，执行原生关闭逻辑
        res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
        
        return res;
    }
    default: {// 其余消息交给原始窗口过程处理
        res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
        return res;
    }
    }
    // 其余消息交给原始窗口过程处理
    res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
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