#include "SSHConEmu.h"

static bool initPanle;//防止未初始化完成就调用面板
SSHConEmu::SSHConEmu(int panelSeqId, int panelrealId)
    :SSHBasePanel(panelSeqId, panelrealId){
}
SSHConEmu::~SSHConEmu() {

}
// 面板初始化：纯原生接口
void SSHConEmu::initPanel() {
    if (initPanle) initPanle = false;//标记正在初始化
    // 检查资源是否存在
    HRSRC hRes = ::FindResource(g_hInst, MAKEINTRESOURCE(IDD_SSH_PANEL), RT_DIALOG);
    if (hRes == NULL) {
        wchar_t errMsg[256] = { 0 };
        swprintf_s(errMsg, L"找不到IDD_SSH_PANEL资源！GetLastError: %d", ::GetLastError());
        ::MessageBoxW(g_nppData._nppHandle, errMsg, L"NppSSH资源错误", MB_OK | MB_ICONERROR);
        return;
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



    _dockData.hClient = _hSelf;
    if (!_hSelf) {
        ::MessageBoxW(g_nppData._nppHandle, L"面板窗口创建失败！", L"NppSSH错误", MB_OK | MB_ICONERROR);
        return;
    }
    _panelHwnd = _hSelf;
    // 注册面板到NPP停靠管理器
    ::SendMessage(g_nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&_dockData));
    ::SendMessage(g_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD, reinterpret_cast<LPARAM>(_hSelf));

    //createTopButtonBar();               // 调用创建顶部按钮栏

    //  从资源中获取EDIT控件句柄（不再手动CreateWindow）
    //_hOutputEdit = ::GetDlgItem(_hSelf, IDC_OUTPUT_EDIT);
    //_hOutputEdit = SSH_TerminalInitControlPanel(_hSelf, _panelSeqId);
    //if (!_hOutputEdit) {
    //    ::MessageBoxW(g_nppData._nppHandle, L"NPP插件环境_hOutputEdit初始化失败！", L"NppSSH调试提示", MB_OK);
    //}
    //SSH_TerminalAppendTextHandle(_panelSeqId, "✅NppSSH面板已创建\r\n等待SSH连接...");

    //if (_hSelf && ::IsWindow(_hSelf)) {         // 强制设置面板窗口样式，解决遮挡/闪烁问题
    //    DWORD dwStyle = ::GetWindowLongPtrW(_hSelf, GWL_STYLE);
    //    SetWindowLongPtrW(_hSelf, GWL_STYLE, dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    //    //::SetWindowPos(_hSelf, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);     // 确保面板在停靠容器的顶层，不被覆盖
    //}
    char bufSelf[64] = { 0 };
    sprintf(bufSelf, "_panelHwnd(_hSelf)=0x%p", _panelHwnd);
    NppSSH_LogInfoAuto(bufSelf);

    // ==== 挂载子类化 ====
    //if (::IsWindow(_panelHwnd)) {
    //    _hTopParent = _panelHwnd;
    //    while (true)
    //    {
    //        HWND hTmpParent = ::GetParent(_hTopParent);
    //        if (hTmpParent == nullptr)
    //            break;
    //        _hTopParent = hTmpParent;
    //    }
    //    // 1. 获取原窗口过程
    //    // 防重复子类化：判断当前WndProc是否已经是自定义过程
    //    WNDPROC curProc = (WNDPROC)GetWindowLongPtrW(_hTopParent, GWLP_WNDPROC);
    //    if (curProc == SSHPanel::PanelSubclassWndProc)
    //    {
    //        NppSSH_LogInfoAuto("面板已完成子类化，跳过");
    //    }
    //    else {
    //        // 获取原生窗口过程，不再存入GWLP_USERDATA
    //        _oldPanelWndProc = (WNDPROC)::GetWindowLongPtrW(_hTopParent, GWLP_WNDPROC);
    //        //if (!_oldPanelWndProc)_oldPanelWndProc = DefWindowProcW;

    //        // 绑定当前面板实例到窗口属性
    //        swprintf_s(_titleParentBuf, _countof(_titleParentBuf), L"SSHPanel-%p", _hTopParent);
    //        ::SetPropW(_hTopParent, _titleParentBuf, (HANDLE)this);

    //        // 设置新窗口过程
    //        ::SetWindowLongPtrW(_hTopParent, GWLP_WNDPROC, (LONG_PTR)PanelSubclassWndProc);
    //    }
    //    NppSSH_LogInfoAuto("Notepad++软件子类化完成！hWnd=" + PtrToHexStr(_hTopParent)
    //        + " 原过程：" + PtrToHexStr(_oldPanelWndProc)
    //        + " 新过程：" + PtrToHexStr(PanelSubclassWndProc));
    //}
    // 13. 日志记录（调试/排查）
    NppSSH_LogInfoAuto("面板初始化完成 [序列ID: " + std::to_string(_panelSeqId) + "]");
    NppSSH_LogInfoAuto("面板初始化完成 [标题ID: " + std::to_string(_panelrealId) + "]");
    if (!initPanle) initPanle = true;//面板初始化完成
}
INT_PTR CALLBACK SSHConEmu::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {

    // 把消息值转宽字符
    wchar_t msgBuf[64] = { 0 };
    swprintf_s(msgBuf, L"当前窗口消息值: 0x%04X", message);
    bool isOk = WM_SETFONT || WM_INITDIALOG || WM_GETDLGCODE || WM_KILLFOCUS || WM_IME_SETCONTEXT || WM_SETFOCUS;
    char buf[64] = { 0 };
    sprintf(buf, "0x%04X", message);
    std::string msgStr(buf);
    if (!isOk) {
        //MessageBoxW(GetHwndSelf(), msgBuf, L"NppSSH", MB_OK | MB_TASKMODAL);
        NppSSH_LogInfoAuto("【拦截run_dlgProc】消息message===" + msgStr);
    }
    switch (message) {
    //case WM_ERASEBKGND:
    //{
    //    HDC hdc = (HDC)wParam;
    //    if (m_hBgImage) {

    //        RECT rcClient;
    //        GetClientRect(GetHwndSelf(), &rcClient);
    //        HDC hMemDC = CreateCompatibleDC(hdc);
    //        HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, m_hBgImage);

    //        // 获取图片原始尺寸
    //        BITMAP bmpInfo = { 0 };
    //        GetObject(m_hBgImage, sizeof(BITMAP), &bmpInfo);

    //        // 拉伸图片铺满面板窗口
    //        StretchBlt(
    //            hdc, 0, 0, rcClient.right, rcClient.bottom,
    //            hMemDC, 0, 0, bmpInfo.bmWidth, bmpInfo.bmHeight,
    //            SRCCOPY
    //        );

    //        SelectObject(hMemDC, hOldBmp);
    //        DeleteDC(hMemDC);
    //        return TRUE;
    //    }
    //    RECT rc;
    //    ::GetClientRect(GetHwndSelf(), &rc);
    //    HBRUSH hBrush = ::CreateSolidBrush(_bgColor);
    //    ::FillRect(hdc, &rc, hBrush);
    //    ::DeleteObject(hBrush);
    //    return TRUE;
    //}
    //case WM_CTLCOLORBTN:
    //case WM_CTLCOLOREDIT:
    //case WM_CTLCOLORSTATIC:
    //{
    //    HDC hdc = (HDC)wParam;
    //    ::SetTextColor(hdc, _fgColor);
    //    ::SetBkMode(hdc, TRANSPARENT);
    //    //WHITE_BRUSH    // 白色
    //    //    BLACK_BRUSH    // 黑色
    //    //    GRAY_BRUSH     // 灰色
    //    //    LTGRAY_BRUSH   // 浅灰
    //    //    NULL_BRUSH     // 透明空画刷（适配背景图必备）
    //    return (LRESULT)::GetStockObject(WHITE_BRUSH);
    //}

    case WM_INITDIALOG:
    {
        if (!GetHwndSelf()) {
            NppSSH_LogErrorAuto("面板窗口句柄无效！");
            ::MessageBox(NULL, TEXT("面板窗口句柄无效！"), TEXT("NppSSH错误提示"), MB_OK | MB_ICONERROR);
            return FALSE;
        }
        return TRUE;
    }
    // 面板大小变化时，自动适配输出文本框（防止遮挡/空白）（最小化关闭/打开notepad++会自动触发）
    //case WM_SIZE:
    //{
    //    UINT sizeType = (UINT)wParam;
    //    int nClientW = LOWORD(lParam);
    //    int nClientH = HIWORD(lParam);

    //    if (sizeType == SIZE_MINIMIZED || nClientW <= 0 || nClientH <= 0)
    //        break;
    //    SetProp(_hOutputEdit, L"NppSSH_PanelW", (HANDLE)(LONG_PTR)nClientW);
    //    SetProp(_hOutputEdit, L"NppSSH_PanelH", (HANDLE)(LONG_PTR)nClientH);

    //    // 只负责重置定时器
    //    SetTimer(GetHwndSelf(), TIMER_ID_RESIZE_PTY, 200, nullptr);
    //    //NppSSH_LogInfoAuto("WM_SIZE 面板新尺寸 -> 宽度:" + std::to_string(nClientW)
    //        //+ "  高度:" + std::to_string(nClientH));

    //    if (initPanle && GetHwndSelf() && ::IsWindow(GetHwndSelf()) && _hOutputEdit && ::IsWindow(_hOutputEdit))
    //    {
    //        //::MessageBoxW(s_nppData._nppHandle, L"SSH面板变化", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
    //        SSH_TerminalResize(GetHwndSelf(), this->_panelSeqId);

    //        //重绘【整个 SSH 面板】 + 面板里面所有的子控件（包括按钮、编辑框、滚动条等全部子窗口）RDW_ALLCHILDREN = 把面板里所有子控件全部刷新一遍
    //        ::RedrawWindow(GetHwndSelf(), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);// 刷新整个面板 + 所有子控件（解决最大化/还原/遮挡BUG）
    //    }
    //    return TRUE;
    //}
    //case WM_TIMER:
    //{
    //    if (wParam == TIMER_ID_RESIZE_PTY)
    //    {
    //        KillTimer(GetHwndSelf(), TIMER_ID_RESIZE_PTY);
    //        SendMessageW(_hOutputEdit, WM_USER_RESIZE_PTY, 0, 0);
    //    }
    //    break;
    //}
    // 处理按钮点击消息
    //case WM_COMMAND:
    //{
    //    UINT cmd = LOWORD(wParam);
    //    HWND hCtrl = (HWND)lParam;
    //    if (cmd == IDC_BTN_CONNECT_SSH) {
    //        NppSSH_LogInfoAuto("用户点击面板连接按钮，显示登录对话框");
    //        ShowSSHLoginWindow_Modal();
    //    }
    //    else if (cmd == IDC_BTN_DISCONNECT_SSH) {
    //        NppSSH_LogInfoAuto("用户点击面板断开按钮" + std::to_string(this->_panelSeqId));
    //        if (_isConnected) {
    //            disconnectSSH(); // 断开连接
    //        }
    //    }
    //    break;
    //}
    // 响应NPP停靠管理器的浮动/停靠消息，更新面板状态
    //case WM_NOTIFY:
    //{
    //    //std::string infoCheck = CheckHwndParentChildRelation(_panelHwnd,g_nppData._nppHandle);
    //    //NppSSH_LogInfoAuto(infoCheck);
    //    //char buf[64] = { 0 };
    //    //sprintf(buf, "0x%04X", message);
    //    //std::string msgStr(buf);
    //    //NppSSH_LogInfoAuto("【run_dlgProc】消息message===" + msgStr);

    //    LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
    //    // 存储hwndFrom、idFrom、code日志
    //    char bufNMHDR[128] = { 0 };
    //    sprintf(bufNMHDR,
    //        "hwndFrom=0x%p, idFrom=%llu, code=0x%04X(%u)",
    //        pnmh->hwndFrom,
    //        (unsigned long long)pnmh->idFrom,
    //        pnmh->code,
    //        pnmh->code
    //    );
    //    //NppSSH_LogInfoAuto("【NMHDR】" + std::string(bufNMHDR));
    //    // 提前预存关闭确认结果，避免在case内部模态阻塞
    //    int closeResult = IDNO;
    //    bool isCloseNotify = false;
    //    // 1.消息来源是面板：全部放行，交给编辑框子类处理
    //    if (pnmh->hwndFrom == g_nppData._nppHandle && pnmh->code == DMN_CLOSE)
    //    {
    //        NppSSH_LogInfoAuto("面板【准备】关闭，当前连接状态：" + std::to_string(_isConnected) + "【触发关闭，执行断开】" + std::string(bufNMHDR));
    //        const bool bHasActiveConn = this->Get_isConnected();

    //        // 检查当前面板是否有活跃SSH连接
    //        if (bHasActiveConn)
    //        {
    //            this->disconnectSSH();   // 断开连接
    //            this->display(false);//准备销毁，先隐藏防止不完整的面板出现影响效果
    //        }
    //        SendMessageW(GetHwndSelf(), WM_CLOSE, wParam, lParam);
    //    }

        // 2.消息来源是Terminal富文本：全部放行，交给编辑框子类处理
    //    if (pnmh->hwndFrom == this->_hOutputEdit)
    //    {
    //        //NppSSH_LogInfoAuto("父转发Terminal通知 code:" + std::to_string(pnmh->code));
    //        SendMessageW(_hOutputEdit, WM_NOTIFY, wParam, lParam);
    //    }
    //    return TRUE;
    //}
    // 面板关闭：原生NPP消息，自动清理资源，无内存泄漏
    //case WM_CLOSE:
    //{
    //    NppSSH_LogInfoAuto("面板【开始】关闭，当前连接状态：" + std::to_string(_isConnected));
    //    SSH_TerminalBySeqIdRemove(_panelSeqId);
    //    // 从NPP原生停靠管理器移除面板
    //    ::SendMessage(s_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, (LPARAM)getHSelf());
    //    ::SendMessage(s_nppData._nppHandle, NPPM_DMMHIDE, 0, (LPARAM)getHSelf());

    //    SSH_SettingsByRealIdRemove(this->_panelrealId);
    //    SSH_PanelVecBySeqIdRemove(_panelSeqId);
    //    SSH_SettingsSavePanelCount(SSH_PanelVecSize());

    //    ::DestroyWindow(this->_panelHwnd);
    //    this->destroy();
    //    delete this;
    //    return TRUE;
    //}

    //// 工具栏图标大小变化时更新按钮图标
    //case NPPN_TOOLBARICONSETCHANGED:
    //{
    //    UpdateToolbarIconSize();
    //    return TRUE;
    //}

    // 其他所有消息，交给DockingDlgInterface原生处理（避免NPP异常）
    default:
        return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
    }
    return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
}

// NPP启动重建面板具体实现
void SSHConEmu_InitRecreatePanel(SSHConEmu* pNewPanel) {
    if (g_nppData._nppHandle == NULL || g_hInst == NULL) {
        ::MessageBoxW(g_nppData._nppHandle, L"NPP环境未初始化，无法重建面板！", L"NppSSH错误", MB_OK | MB_ICONWARNING);
        return;
    }
    //s_panelCounter++;// 同步计数器，保证新创建面板ID不重复
    //SSHPanel* pNewPanel = new SSHPanel(panelId);
    if (pNewPanel) {
        pNewPanel->initPanel();

        // 获取插件DLL目录，拼接图片路径
        //TCHAR szNppPath[MAX_PATH] = { 0 };
        //GetModuleFileName(NULL, szNppPath, MAX_PATH);
        //PathRemoveFileSpec(szNppPath);
        //TCHAR szConfigDir[MAX_PATH] = { 0 };
        //_stprintf_s(szConfigDir, MAX_PATH, _T("%s\\plugins\\config\\bg.png"), szNppPath);
        ////宽字符转ANSI打印路径
        //char logBuf[1024] = { 0 };
        //WideCharToMultiByte(CP_UTF8, 0, szConfigDir, -1, logBuf, 1024, NULL, NULL);
        //NppSSH_LogInfoAuto(std::string("当前拼接完整图片路径：") + logBuf);
        // 设置图片背景
        //pNewPanel->SetBackgroundImage(szConfigDir);

        // 设置面板背景色（黑色示例）
        //pNewPanel->setBackgroundColor(RGB(240, 240, 240));
        //pNewPanel->setForegroundColor(RGB(255, 0, 0));
        pNewPanel->display(true);
        //::SendMessage(s_nppData._nppHandle, NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(pNewPanel->getHSelf()));
        // 额外触发标签栏重绘（兜底）
        //::RedrawWindow(s_nppData._nppHandle, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
    }
}