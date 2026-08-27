#include "SSHAppPanel.h"
SSHAppPanel::SSHAppPanel(int panelSeqId, int panelrealId)
    :SSHBasePanel(panelSeqId, panelrealId),
    _textColor(RGB(255, 255, 255)),
    _bgColor(GetSysColor(COLOR_WINDOW)),
    _fgColor(GetSysColor(COLOR_WINDOWTEXT)),
    _hBgImage(nullptr),
    _hStaticPuttyTip(nullptr),
    _hEditPuttyPath(nullptr),
    _hBtnSelectFile(nullptr),
    _strPuttyFullPath(L""),
    _hBtnPutty(nullptr),
    _hBtnDestroy(nullptr),
    _hBtnWinTop(nullptr),
    _hToolTip(nullptr),
    _hIconPutty(nullptr),
    _hIconDestroy(nullptr),
    _hIconWinTop(nullptr),
    _winTopState(false),
    _hIconSelectFile(nullptr),
    _editLabelFontSize(18){

}
SSHAppPanel::~SSHAppPanel() {
    isHandleHasActiveThread();
    // 关闭所有PuTTY会话
    {
        std::lock_guard<std::mutex> lock(_sessionListMtx);
        for (PuTTYSession* pSess : _sessionList)
        {
            if (!pSess) continue;
            pSess->StopMonitor();

            DWORD exitCode = 0;
            BOOL bGetExit = ::GetExitCodeProcess(pSess->hProcess, &exitCode);
            if (bGetExit && exitCode == STILL_ACTIVE && pSess->hMonitorThread != nullptr && WaitForSingleObject(pSess->hMonitorThread, 0) == WAIT_TIMEOUT)
            {
               
                pSess->CleanHandle();
                continue;
            }
            // 5. 释放句柄
            pSess->CleanHandle();
            if (!SSH_SettingsGetConfigFileExistPath(pSess->tmpFile).empty())
                SSH_SettingsDeleteConfigFile(pSess->tmpFile);
            delete pSess;
        }
        _sessionList.clear();
    }
    if (_hToolTip != nullptr)
    {
		::DestroyWindow(_hToolTip);//因为是子窗口，所以需要手动直接销毁
        _hToolTip = nullptr;
    }
    if (_hIconPutty)
    {
        ::DestroyIcon(_hIconPutty);
        _hIconPutty = nullptr;
    }
    if (_hIconDestroy)
    {
        ::DestroyIcon(_hIconDestroy);
        _hIconDestroy = nullptr;
    }
    if (_hIconWinTop)
    {
        ::DestroyIcon(_hIconWinTop);
        _hIconWinTop = nullptr;
    }
    if (_hIconSelectFile)
    {
        ::DestroyIcon(_hIconSelectFile);
        _hIconSelectFile = nullptr;
    }
    
    NppSSH_LogInfoAuto("执行【SSHAppPanel】析构函数");
}
DWORD WINAPI SSHAppPanel::MonitorThreadProxy(LPVOID lpParam)
{
    PuTTYSession* pSess = reinterpret_cast<PuTTYSession*>(lpParam);
    if (pSess == nullptr)
        return 1;
    try
    {
        // 无参调用自身成员监控函数
        return pSess->PuTTYSessionMonitor();
    }
    catch (...)
    {
        return 2;
    }
}
void SSHAppPanel::CleanInvalidSession()
{
    std::lock_guard<std::mutex> lock(_sessionListMtx);
    std::vector<PuTTYSession*> validList;

    for (PuTTYSession* pSess : _sessionList)
    {
        if (!pSess) continue;
        DWORD exitCode = 0;
        BOOL bGetExit = ::GetExitCodeProcess(pSess->hProcess, &exitCode);
        // 进程存活、线程正常，保留
        if (bGetExit && exitCode == STILL_ACTIVE && pSess->hMonitorThread != nullptr && WaitForSingleObject(pSess->hMonitorThread, 0) == WAIT_TIMEOUT)
        {
            validList.push_back(pSess);
            continue;
        }
        // 失效会话：释放资源+销毁
        pSess->StopMonitor();
        pSess->CleanHandle();
        if (!SSH_SettingsGetConfigFileExistPath(pSess->tmpFile).empty())
            SSH_SettingsDeleteConfigFile(pSess->tmpFile);
        delete pSess;
    }
    // 替换为仅存活跃会话的列表，彻底清除野指针
    _sessionList.swap(validList);
}
bool SSHAppPanel::isHandleHasActiveThread() {
    std::lock_guard<std::mutex> lock(_sessionListMtx);
    if (_isConnected) {
        int closeResult = ::MessageBoxW(_panelHwnd,
            L"存在已绑定的Putty窗口，需要给所有Putty窗口发送关闭消息吗？",
            L"NppSSH 提示",
            MB_YESNO | MB_ICONWARNING);
        // 用户取消：拦截关闭，不传递WM_CLOSE给原生窗口
        if (closeResult == IDYES)
        {
            for (PuTTYSession* pSess : _sessionList) {
                // 强制PuTTY窗口前置，弹窗会显示在桌面顶层，用户可见
                ::SetForegroundWindow(pSess->hWnd);
                ::BringWindowToTop(pSess->hWnd);

                // 异步投递关闭消息，主线程立刻返回，不会卡死NPP
                ::PostMessageW(pSess->hWnd, WM_CLOSE, 0, 0);
                NppSSH_LogInfoAuto("异步投递WM_CLOSE消息至PuTTY窗口，不阻塞主线程");
            }
        }
    }
    return _isConnected;
}
INT_PTR CALLBACK SSHAppPanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {

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
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        if (_hBgImage) {

            RECT rcClient;
            GetClientRect(GetHwndSelf(), &rcClient);
            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, _hBgImage);

            // 获取图片原始尺寸
            BITMAP bmpInfo = { 0 };
            GetObject(_hBgImage, sizeof(BITMAP), &bmpInfo);

            // 拉伸图片铺满面板窗口
            StretchBlt(
                hdc, 0, 0, rcClient.right, rcClient.bottom,
                hMemDC, 0, 0, bmpInfo.bmWidth, bmpInfo.bmHeight,
                SRCCOPY
            );

            SelectObject(hMemDC, hOldBmp);
            DeleteDC(hMemDC);
            return TRUE;
        }
        RECT rc;
        ::GetClientRect(GetHwndSelf(), &rc);
        HBRUSH hBrush = ::CreateSolidBrush(_bgColor);
        ::FillRect(hdc, &rc, hBrush);
        ::DeleteObject(hBrush);
        return TRUE;
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        ::SetTextColor(hdc, _fgColor);
        ::SetBkMode(hdc, TRANSPARENT);
        //WHITE_BRUSH    // 白色
        //    BLACK_BRUSH    // 黑色
        //    GRAY_BRUSH     // 灰色
        //    LTGRAY_BRUSH   // 浅灰
        //    NULL_BRUSH     // 透明空画刷（适配背景图必备）
        //return (LRESULT)::GetStockObject(WHITE_BRUSH);
        // 静态文字：返回空画刷，直接透明
        if (message == WM_CTLCOLORSTATIC)
        {
            return (LRESULT)::GetStockObject(NULL_BRUSH);
        }
        else
        {
            return (LRESULT)::GetStockObject(WHITE_BRUSH);
        }
    }

    case WM_INITDIALOG:
    {
        if (!GetHwndSelf()) {
            NppSSH_LogErrorAuto("面板窗口句柄无效！");
            ::MessageBox(NULL, TEXT("面板窗口句柄无效！"), TEXT("NppSSH错误提示"), MB_OK | MB_ICONERROR);
            return FALSE;
        }
        return TRUE;
    }
    // 处理按钮点击消息
    case WM_COMMAND:
    {
        UINT cmd = LOWORD(wParam);
        HWND hCtrl = (HWND)lParam;

        // 浏览Putty按钮点击
        if (cmd == IDC_BTN_BROWSE_PUTTY)
        {
            OpenPuttyFileDialog();
            break;
        }
        // 编辑框文本修改，同步到内存变量
        else if (cmd == IDC_EDIT_PUTTY_PATH && HIWORD(wParam) == EN_CHANGE)
        {
            WCHAR buf[MAX_PATH] = { 0 };
            ::GetWindowTextW(_hEditPuttyPath, buf, MAX_PATH);
            _strPuttyFullPath = buf;
            UpdateToolTipMessage(_hEditPuttyPath, _strPuttyFullPath.c_str());
            break;
        }
        else if (cmd == IDC_BTN_CONNECT_PUTTY) {
            if (puttyLoginPathHandle())
            {
                SSHLoginModal input{};
                SSH_LoginModalWindowsModal(&input);
                if (input.bOk)
                {
                    bool result = SSHAppPanel_PuttyLoginHandle(input.szHost, input.szPort, input.szUser, input.szPass, input.szDir);
                    if (result) SSH_SettingsSavePuttyExePath(_strPuttyFullPath);
                }
            }
        }
        else if (cmd == IDC_BTN_CLOSE_SSH) {
            NppSSH_LogInfoAuto("用户点击面板关闭Putty按钮" + std::to_string(this->_panelSeqId));
            CloseSoftWare();
        }
        else if (cmd == IDC_BTN_PUTTYTOP_SSH) {
            NppSSH_LogInfoAuto("用户点击面板置顶Putty按钮" + std::to_string(this->_panelSeqId));
            _winTopState = !_winTopState;
            //if (_isConnected) { }
            for (PuTTYSession* pSess : _sessionList) {
                pSess->isWindowTop = _winTopState;
                windowTopBtnHandle(pSess->hWnd, _winTopState);
            }
            UpdateWinTopBtnUI_FromMemState();
            //for (auto it = _sessionList.rbegin(); it != _sessionList.rend(); ++it)
            //{
            //    PuTTYSession* pSess = *it;
            //    pSess->isWindowTop = _winTopState;
            //    windowTopBtnHandle(pSess->hWnd, _winTopState);
            //}
            //// 正向：从开头到末尾
            //for (auto it = _sessionList.begin(); it != _sessionList.end(); ++it)
            //{
            //    PuTTYSession* pSess = *it;
            //    pSess->isWindowTop = _winTopState;
            //    windowTopBtnHandle(pSess->hWnd, _winTopState);
            //}
            
        }
        return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
    }
    // 响应NPP停靠管理器的浮动/停靠消息，更新面板状态
    case WM_NOTIFY:
    {
        //std::string infoCheck = CheckHwndParentChildRelation(_panelHwnd,g_nppData._nppHandle);
        //NppSSH_LogInfoAuto(infoCheck);
        //char buf[64] = { 0 };
        //sprintf(buf, "0x%04X", message);
        //std::string msgStr(buf);
        //NppSSH_LogInfoAuto("【run_dlgProc】消息message===" + msgStr);

        LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
        // 存储hwndFrom、idFrom、code日志
        char bufNMHDR[128] = { 0 };
        sprintf(bufNMHDR,
            "hwndFrom=0x%p, idFrom=%llu, code=0x%04X(%u)",
            pnmh->hwndFrom,
            (unsigned long long)pnmh->idFrom,
            pnmh->code,
            pnmh->code
        );
        if (pnmh->hwndFrom == g_nppData._nppHandle && pnmh->code == DMN_CLOSE)
        {
            SendMessageW(GetHwndSelf(), WM_CLOSE, wParam, lParam);
        }
        return TRUE;
    }
    // 面板关闭：原生NPP消息，自动清理资源，无内存泄漏
    case WM_CLOSE:
    {
        NppSSH_LogInfoAuto("面板【开始】关闭，当前连接状态：" + std::to_string(_isConnected));
        // 从NPP原生停靠管理器移除面板
        ::SendMessage(g_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, (LPARAM)getHSelf());
        ::SendMessage(g_nppData._nppHandle, NPPM_DMMHIDE, 0, (LPARAM)getHSelf());
        SSH_PanelVecBySeqIdRemove(_panelSeqId, _panelrealId);
        return TRUE;
    }
    // 其他所有消息，交给DockingDlgInterface原生处理（避免NPP异常）
    default:
        return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
    }
    return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
}


// 重写背景色
void SSHAppPanel::setBackgroundColor(COLORREF color) {
    _bgColor = color;
    // 刷新面板，触发WM_ERASEBKGND、WM_PAINT重绘
    ::InvalidateRect(GetHwndSelf(), nullptr, TRUE);
}
// 重写前景文字色
void SSHAppPanel::setForegroundColor(COLORREF color) {
    _fgColor = color;
    ::InvalidateRect(GetHwndSelf(), nullptr, TRUE);
}
// 面板初始化：纯原生接口
void SSHAppPanel::initPanel() {
    bool isDockDataInitialized = initDockData();
    if (!isDockDataInitialized && !::IsWindow(_panelHwnd)) {
        NppSSH_LogInfoAuto("面板停靠数据初始化失败！");
        return;
    }
    char bufSelf[64] = { 0 };
    sprintf(bufSelf, "_panelHwnd(_hSelf)=0x%p", _panelHwnd);
    NppSSH_LogInfoAuto(bufSelf);
    createButtonBar();
    createToolTip();

    bool isSubclass = GlobalSubclassTopWnd(); //挂载子类化
    _isConnected = false;
    // 日志记录（调试/排查）
    NppSSH_LogInfoAuto("面板初始化完成 [序列ID: " + std::to_string(_panelSeqId) + "]");
    NppSSH_LogInfoAuto("面板初始化完成 [标题ID: " + std::to_string(_panelrealId) + "]");
}
// 创建按钮栏
void SSHAppPanel::createButtonBar() {
    if (!GetHwndSelf() || !::IsWindow(GetHwndSelf()))
    {
        ::MessageBoxW(g_nppData._nppHandle, L"面板句柄无效，无法创建按钮", L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
        return;
    }
    _iconSize = 48;
    RECT rcClient;
    ::GetClientRect(GetHwndSelf(), &rcClient);

    const int btnMargin = 5;    // 左边距
    const int btnTop = 10;       // 上边距
    const int btnGap = 10;       // 按钮间距
    const int btnInitSize = _iconSize;  // 按钮初始尺寸
    const int marginLeft = 5;
    // 控件尺寸定义
    int staticW = 150;
    int editW = 420;
    int btnBrowseW = 90;
    //int ctrlH = 48;
    _hStaticPuttyTip = ::CreateWindowW(
        L"STATIC",
        L"Putty路径：",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        marginLeft, btnTop,
        staticW, btnInitSize,
        GetHwndSelf(),
        (HMENU)IDC_STATIC_PUTTY_TIP,
        g_hInst,
        NULL
    );
    //WCHAR initPath[] = L"D:\\software\\developer\\putty-x64-0.84-cn1\\putty.exe";
    std::wstring loadedPuttyPath = SSH_SettingsLoadPuttyExePath();
    WCHAR initPath[MAX_PATH] = { 0 };
    if (!loadedPuttyPath.empty()){
        wcscpy_s(initPath, MAX_PATH, loadedPuttyPath.c_str());
    }else{
        WCHAR dllFullPath[MAX_PATH] = { 0 };
        ::GetModuleFileNameW(g_hInst, dllFullPath, MAX_PATH);// 获取NppSSH.dll完整路径
        ::PathRemoveFileSpecW(dllFullPath);// 得到插件所在目录（去掉dll文件名，得到 ...\plugins\NppSSH\）

        WCHAR bundlePuttyExe[MAX_PATH] = { 0 };
        ::PathCombineW(bundlePuttyExe, dllFullPath, L"putty\\putty.exe");
        if (::PathFileExistsW(bundlePuttyExe) && IsRealPuttyGuiExe(bundlePuttyExe)){
            //插件目录下putty校验全部通过，作为兜底
            wcscpy_s(initPath, MAX_PATH, bundlePuttyExe);
            NppSSH_LogInfoAuto("使用插件内置putty作为兜底路径:" + WStringToLogStr(std::wstring(bundlePuttyExe)));
        }else {
            ::ZeroMemory(initPath, sizeof(initPath));
            NppSSH_LogInfoAuto("插件目录内置putty.exe无效，兜底路径为空");
        }
    }
    // Putty路径编辑框
    _hEditPuttyPath = ::CreateWindowW(
        L"EDIT",
        initPath, // 默认初始路径
        WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | WS_BORDER,
        marginLeft + staticW, btnTop,
        editW, btnInitSize,
        GetHwndSelf(),
        (HMENU)IDC_EDIT_PUTTY_PATH,
        g_hInst,
        NULL
    );
    WCHAR buf[MAX_PATH] = { 0 };
    ::GetWindowTextW(_hEditPuttyPath, buf, MAX_PATH);
    _strPuttyFullPath = buf;

    // 浏览选择按钮
    _hBtnSelectFile = ::CreateWindowW(
        L"BUTTON",
        L"",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER,
        marginLeft + staticW + editW + btnGap, btnTop,
        btnInitSize, btnInitSize,
        GetHwndSelf(),
        (HMENU)IDC_BTN_BROWSE_PUTTY,
        g_hInst,
        NULL
    );

    // 创建「putty连接」按钮（无文字）
    _hBtnPutty = ::CreateWindowW(
        L"BUTTON",
        L"", // 文字设为空
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER,
        marginLeft + staticW + editW + btnGap + btnInitSize + btnGap,
        btnTop,
        btnInitSize, btnInitSize, // 初始大小
        GetHwndSelf(),
        (HMENU)IDC_BTN_CONNECT_PUTTY,
        g_hInst, // 用全局插件实例句柄
        NULL
    );

    // 创建「关闭窗口」按钮（无文字）
    _hBtnDestroy = ::CreateWindowW(
        L"BUTTON",
        L"", // 文字设为空
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER,
        marginLeft + staticW + editW + btnGap + btnInitSize + btnGap + btnInitSize + btnGap, // 左坐标 =  + 间距
        btnTop,
        btnInitSize, btnInitSize, // 初始大小
        GetHwndSelf(),
        (HMENU)IDC_BTN_CLOSE_SSH,
        g_hInst,
        NULL
    );

    //窗口置顶按钮
    _hBtnWinTop = ::CreateWindowW(
        L"BUTTON",
        L"", // 文字设为空
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER,
        marginLeft + staticW + editW + btnGap + btnInitSize + btnGap + btnInitSize + btnGap + btnInitSize + btnGap, // 左坐标 =  + 间距
        btnTop,
        btnInitSize, btnInitSize, // 初始大小
        GetHwndSelf(),
        (HMENU)IDC_BTN_PUTTYTOP_SSH,
        g_hInst,
        NULL
    );

    SetPathControlFontSize(btnInitSize);

    // 将按钮设为纯图标模式（对接自定义图标）
    if (_hBtnSelectFile) {
        SetButtonIconOnly(_hBtnSelectFile, IDI_ICON_SELECT);
    }
    else {
        ::MessageBoxW(g_nppData._nppHandle, L"选择文件按钮创建失败", L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
    }

    if (_hBtnPutty) {
        SetButtonIconOnly(_hBtnPutty, IDI_ICON_PUTTY);
    }
    else {
        ::MessageBoxW(g_nppData._nppHandle, L"putty连接按钮创建失败", L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
    }

    if (_hBtnDestroy) {
        SetButtonIconOnly(_hBtnDestroy, IDI_ICON_CLOSE);
        //::EnableWindow(_hBtnDestroy, FALSE);// 初始状态：断开按钮置灰
    }
    else {
        ::MessageBoxW(g_nppData._nppHandle, L"断开按钮创建失败", L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
    }

    if (_hBtnWinTop) {
        SetButtonIconOnly(_hBtnWinTop, IDI_ICON_WINTOP);
        //::EnableWindow(_hBtnWinTop, FALSE);// 初始状态：置顶按钮置灰
    }
    else {
        ::MessageBoxW(g_nppData._nppHandle, L"置顶按钮创建失败", L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
    }
}

void SSHAppPanel::createToolTip() {
	// 创建工具提示控件
	_hToolTip = ::CreateWindowExW(
		WS_EX_TOPMOST,
		TOOLTIPS_CLASS,
		NULL,
		WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		GetHwndSelf(),
		NULL,
		g_hInst,
		NULL
	);
	if (_hToolTip == nullptr) {
		::MessageBoxW(g_nppData._nppHandle, L"工具提示控件创建失败", L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
		return;
	}
    // 设置悬浮延迟：500ms显示提示，100ms初始延迟
    ::SendMessageW(_hToolTip, TTM_SETDELAYTIME, TTDT_INITIAL, (LPARAM)500);
    ::SendMessageW(_hToolTip, TTM_SETMAXTIPWIDTH, 0, (LPARAM)450);//设置提示框最大宽度，长路径自动换行
	// 设置工具提示文本
	TOOLINFOW ti = { 0 };
	ti.cbSize = sizeof(TOOLINFOW);
	ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND; // 子类化按钮，使用按钮句柄作为ID
	ti.hwnd = GetHwndSelf();

    ti.uId = (UINT_PTR)_hEditPuttyPath; // 关联Putty路径输入框
    ti.lpszText = (LPWSTR) _strPuttyFullPath.c_str();;
    ::SendMessageW(_hToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

	ti.uId = (UINT_PTR)_hBtnSelectFile; // 关联选择文件按钮
	ti.lpszText = (LPWSTR)L"点击选择Putty可执行文件";
	::SendMessageW(_hToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

	ti.uId = (UINT_PTR)_hBtnPutty; // 关联连接按钮
	ti.lpszText = (LPWSTR)L"点击连接Putty";
	::SendMessageW(_hToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

	ti.uId = (UINT_PTR)_hBtnDestroy; // 关联断开按钮
	ti.lpszText = (LPWSTR)L"点击关闭所有Putty窗口";
	::SendMessageW(_hToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

	ti.uId = (UINT_PTR)_hBtnWinTop; // 关联置顶按钮
	ti.lpszText = (LPWSTR)L"点击置顶所有PuTTY窗口";
	::SendMessageW(_hToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

    //::SendMessageW(_hToolTip, TTM_SETTITLEW, TTI_INFO_LARGE, (LPARAM)L"提示");
}

// 把按钮变成纯图标模式
void SSHAppPanel::SetButtonIconOnly(HWND btn, int iconId)
{
    if (btn == nullptr || !::IsWindow(btn))
    {
        ::MessageBoxW(g_nppData._nppHandle, L"按钮句柄无效", L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
        return;
    }

    // 获取工具栏图标尺寸
    HICON hIcon = LoadCustomIcon(iconId, _iconSize);
    if (hIcon == NULL) {
        hIcon = LoadIcon(NULL, IDI_APPLICATION);// 图标加载失败时用系统默认图标（避免报错）
        ::MessageBoxW(g_nppData._nppHandle, L"图标加载失败，使用默认图标", L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
        return;
    }

    // 先移除所有原有样式，强制设置为纯图标
    ::SetWindowLongPtrW(btn, GWL_STYLE, WS_VISIBLE | WS_CHILD | BS_ICON | WS_BORDER);
    ::SetWindowPos(btn, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED); // 通知样式变更
    // 设置图标后，强制按钮持有句柄
    // 设置图标+按钮尺寸（与工具栏完全一致：图标尺寸+4px边距，匹配NPP工具栏按钮）
    ::SendMessage(btn, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIcon);

    // 双重刷新（确保样式和图标生效）
    ::InvalidateRect(btn, NULL, TRUE);
    ::UpdateWindow(btn);
    ::RedrawWindow(btn, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
}
// 加载自定义图标
HICON SSHAppPanel::LoadCustomIcon(int iconId, int size)
{
    // 校验基础参数
    if (g_hInst == NULL || iconId <= 0 || size <= 0) {
        ::MessageBoxW(g_nppData._nppHandle, L"图标加载参数无效", L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
        return LoadIcon(NULL, IDI_APPLICATION);
    }
    size = size * 0.8;
    // 核心：加载图标（移除LR_LOADFROMFILE，使用资源加载）
    HICON hIcon = (HICON)::LoadImage(
        g_hInst,                  // 全局插件实例句柄（已初始化）
        MAKEINTRESOURCE(iconId),  // 图标 ID（IDC_BTN_CONNECT_SSH/IDC_BTN_CLOSE_SSH）
        IMAGE_ICON,               // 资源类型为图标
        size, size,               // 图标大小
        LR_DEFAULTCOLOR  // 默认颜色 + 共享资源（避免内存泄漏）
    );
    
    if (hIcon == nullptr)
    {
        wchar_t errMsg[256] = { 0 };
        swprintf_s(errMsg, L"图标ID:%d 加载失败，错误码:%d", iconId, ::GetLastError());
        ::MessageBoxW(g_nppData._nppHandle, errMsg, L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
        hIcon = LoadIcon(NULL, IDI_APPLICATION);// 兜底：加载失败时返回系统默认图标
    }

    // 持久化到类成员，避免被系统回收
    if (iconId == IDI_ICON_CONNECT) {
        if (_hIconPutty) ::DestroyIcon(_hIconPutty); // 释放旧图标
        _hIconPutty = hIcon;
    }
    else if (iconId == IDI_ICON_CLOSE) {
        if (_hIconDestroy) ::DestroyIcon(_hIconDestroy); // 释放旧图标
        _hIconDestroy = hIcon;
    }
    else if (iconId == IDI_ICON_WINTOP) {
        if (_hIconWinTop) ::DestroyIcon(_hIconWinTop); // 释放旧图标
        _hIconWinTop = hIcon;
    }
    else if (iconId == IDI_ICON_CLOSEWINTOP) {
        if (_hIconWinTop) ::DestroyIcon(_hIconWinTop); // 释放旧图标
        _hIconWinTop = hIcon;
    }
    else if (iconId == IDI_ICON_SELECT) {
        if (_hIconSelectFile) ::DestroyIcon(_hIconSelectFile); // 释放旧图标
        _hIconSelectFile = hIcon;
    }
    return hIcon;

}
void SSHAppPanel::OpenPuttyFileDialog()
{
    WCHAR szFile[MAX_PATH] = { 0 };
    WCHAR szInitialDir[MAX_PATH] = { 0 };
    ZeroMemory(szInitialDir, sizeof(szInitialDir));
    if (!_strPuttyFullPath.empty())
    {
        wcscpy_s(szInitialDir, _strPuttyFullPath.c_str());
        ::PathRemoveFileSpecW(szInitialDir);
    }

    while (true)
    {
        OPENFILENAMEW ofn = { 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ZeroMemory(szFile, sizeof(szFile));

        ofn.lStructSize = sizeof(OPENFILENAMEW);
        ofn.hwndOwner = _panelHwnd;
        ofn.hInstance = g_hInst;
        // 过滤器：2个选项：exe文件 / 所有文件
        ofn.lpstrFilter = L"EXE可执行文件 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0\0";
        ofn.nFilterIndex = 1;               // 默认选中第1项：exe过滤
        ofn.lpstrInitialDir = szInitialDir; // 上次选择的文件夹作为初始目录
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"选择PuTTY主程序";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;

        BOOL bOk = ::GetOpenFileNameW(&ofn);
        if (!bOk)
        {
            // 用户点击取消，退出循环，结束选择
            break;
        }

        std::wstring tempPuttyPath = szFile;
        // 保存旧路径，临时替换用于校验
        std::wstring oldPath = _strPuttyFullPath;
        _strPuttyFullPath = tempPuttyPath;

        bool bValid = puttyLoginPathHandle();

        // 恢复旧路径
        _strPuttyFullPath.swap(oldPath);

        if (bValid)
        {
            // 校验通过，正式保存，退出循环
            _strPuttyFullPath = tempPuttyPath;
            ::SetWindowTextW(_hEditPuttyPath, _strPuttyFullPath.c_str());
            if (SSH_SettingsSavePuttyExePath(_strPuttyFullPath)) {
                NppSSH_LogInfoAuto("已选择Putty路径：" + WStringToLogStr(_strPuttyFullPath)+ ",Putty路径保存成功");
            }
            break;
        }
        else
        {
            // 校验失败：puttyLoginPathHandle已经弹出错误MessageBox
            // 不break，继续下一轮循环：重新打开文件对话框
            // 注意：szInitialDir会沿用本次选中文件所在目录，用户不用重新导航文件夹
            wcscpy_s(szInitialDir, tempPuttyPath.c_str());
            ::PathRemoveFileSpecW(szInitialDir);
            continue;
        }
    }
}
void SSHAppPanel::SetPathControlFontSize(int fontSize)
{
    // 更新私有字号变量
    _editLabelFontSize = fontSize * 0.8;

    // 创建标准字体 HFONT，自动适配系统DPI
    //_editLabelFontSize = -MulDiv(_editLabelFontSize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);// 字号转像素
    HFONT hFont = CreateFontW(_editLabelFontSize,
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI" // 系统通用无衬线字体
    );
    if (!hFont) return;

    // 批量给3个路径控件设置字体（静态文字、输入框、浏览按钮）
    HWND ctrlList[] = { _hStaticPuttyTip, _hEditPuttyPath };
    for (HWND hWnd : ctrlList)
    {
        if (hWnd != nullptr && ::IsWindow(hWnd))
        {
            ::SendMessageW(hWnd, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
    }

    // 字体句柄交给窗口托管，无需手动释放，窗口销毁系统自动回收
}

void SSHAppPanel::UpdateToolTipMessage(HWND _hHWND, const WCHAR*  tipText) {
    // 更新已有Tooltip信息（不销毁重建tooltip控件）
    TOOLINFOW ti = { 0 };
    ti.cbSize = sizeof(TOOLINFOW);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = GetHwndSelf();
    ti.uId = (UINT_PTR)_hHWND;
    ti.lpszText = (LPWSTR)tipText;
    ::SendMessageW(_hToolTip, TTM_SETTOOLINFO, 0, (LPARAM)&ti);
}
void SSHAppPanel::SetBackgroundImage(const WCHAR* imgPath)
{
    // 释放旧图片
    if (_hBgImage)
    {
        DeleteObject(_hBgImage);
        _hBgImage = NULL;
    }
    // GDI+加载任意格式图片
    _hBgImage = LoadImageByGdiPlus(imgPath);
    InvalidateRect(GetHwndSelf(), nullptr, TRUE);
}
HBITMAP SSHAppPanel::LoadImageByGdiPlus(const WCHAR* filePath)
{
    if (!PathFileExistsW(filePath))
    {
        NppSSH_LogInfoAuto("图片文件不存在！");
        return NULL;
    }
    Gdiplus::Bitmap* pBitmap = new Gdiplus::Bitmap(filePath);
    Gdiplus::Status stat = pBitmap->GetLastStatus();
    if (pBitmap == nullptr || pBitmap->GetLastStatus() != Gdiplus::Ok)
    {
        char err[128];
        sprintf(err, "GDI加载失败，状态码:%d", stat);
        NppSSH_LogInfoAuto(err);
        delete pBitmap;
        return NULL;
    }

    UINT width = pBitmap->GetWidth();
    UINT height = pBitmap->GetHeight();
    HBITMAP hBmp = NULL;
    HDC hScreenDC = GetDC(NULL);

    // 创建兼容32位位图（支持PNG透明通道）
    hBmp = CreateCompatibleBitmap(hScreenDC, width, height);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hOld = (HBITMAP)SelectObject(hMemDC, hBmp);

    // GDI+绘图到内存DC
    Gdiplus::Graphics graphics(hMemDC);
    graphics.DrawImage(pBitmap, 0, 0, width, height);

    // 资源释放
    SelectObject(hMemDC, hOld);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);
    delete pBitmap;
    return hBmp;
}

bool SSHAppPanel::puttyLoginPathHandle() {
    // 1. 取出Putty完整路径
    const std::wstring& puttyExePath = _strPuttyFullPath;

    // 校验路径非空
    if (puttyExePath.empty())
    {
        ::MessageBoxW(_panelHwnd, L"未选择 putty.exe 程序路径！\n请点击按钮指定PuTTY主程序", L"NppSSH 提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：putty路径为空");
        return false;
    }

    // 校验后缀必须是.exe，大小写兼容，不限制exe文件名
    bool bIsExeSuffix = false;
    size_t pathLen = puttyExePath.size();
    if (pathLen >= 4)
    {
        wchar_t c1 = towlower(puttyExePath[pathLen - 4]);
        wchar_t c2 = towlower(puttyExePath[pathLen - 3]);
        wchar_t c3 = towlower(puttyExePath[pathLen - 2]);
        wchar_t c4 = towlower(puttyExePath[pathLen - 1]);
        if (c1 == L'.' && c2 == L'e' && c3 == L'x' && c4 == L'e')
        {
            bIsExeSuffix = true;
        }
    }
    if (!bIsExeSuffix)
    {
        wchar_t errTip[1024] = { 0 };
        swprintf_s(errTip, L"指定程序必须为 .exe 可执行文件！\n\n当前路径：%s", puttyExePath.c_str());
        ::MessageBoxW(_panelHwnd, errTip, L"NppSSH 错误提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：不是exe后缀 -> " + WStringToLogStr(puttyExePath));
        return false;
    }

    // 校验文件是否真实存在
    if (!::PathFileExistsW(puttyExePath.c_str()))
    {
        wchar_t errTip[1024] = { 0 };
        swprintf_s(errTip, L"指定文件不存在：\n\n%s", puttyExePath.c_str());
        ::MessageBoxW(_panelHwnd, errTip, L"NppSSH 错误提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：文件不存在 -> " + WStringToLogStr(puttyExePath));
        return false;
    }

    // PE版本校验：读不到版本资源直接放行；读到才拦截plink和其他exe
    if (!IsRealPuttyGuiExe(puttyExePath))
    {
        wchar_t errTip[1024] = { 0 };
        swprintf_s(errTip, L"该EXE不是合法PuTTY‑GUI程序，请选择PuTTY主程序。\n\n路径：%s", puttyExePath.c_str());
        ::MessageBoxW(_panelHwnd, errTip, L"NppSSH 错误提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：不是合法PuTTY GUI程序 -> " + WStringToLogStr(puttyExePath));
        return false;
    }
    return true;
}

bool SSHAppPanel::SSHAppPanel_PuttyLoginHandle(std::wstring host, std::wstring port, std::wstring user, std::wstring pass, std::wstring director)
{
    // 1. 取出Putty完整路径
    const std::wstring& puttyExePath = _strPuttyFullPath;
    // 2. 初始化进程启动参数
    STARTUPINFOW si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL; // 正常窗口显示，和双击打开一致

    // 命令行缓冲区
    std::wstring SSH_HOST = host;
    std::wstring SSH_PORT = port;
    std::wstring SSH_USER = user;
    std::wstring SSH_PASS = pass;
    std::wstring SSH_INITCD = director;

    std::wstring ExceComd = L"HISTFILE=/dev/null;cd " + SSH_INITCD + L";exec bash -il;";
    // 每次连接生成唯一临时文件名（面板句柄+随机区分多会话）
    WCHAR tmpNameBuf[128] = { 0 };
    static ULONG tmpSerial = 0;
    swprintf_s(tmpNameBuf, L"NppSSH_%p_%lu.tmp", _panelHwnd, tmpSerial++);
    std::wstring newTmpFile = tmpNameBuf;
    // 写入临时脚本
    if (!SSH_INITCD.empty())SSH_SettingsSaveConfigTmpFile(newTmpFile, ExceComd);
    NppSSH_LogInfoAuto("【数据：】"+WStringToLogStr(SSH_HOST) 
        + WStringToLogStr(SSH_PORT)
        + WStringToLogStr(SSH_USER)
        + WStringToLogStr(SSH_PASS));
    std::wstring ExceLogin = L"\"" + puttyExePath + L"\" -ssh \"" + SSH_HOST + L"\" -P " + SSH_PORT + L" -l " + SSH_USER ;
    if (!SSH_PASS.empty()) { ExceLogin += L" -pw " + SSH_PASS; }
    std::wstring ExcelFilePath = SSH_SettingsGetConfigFileExistPath(newTmpFile);
    if (!ExcelFilePath.empty())ExceLogin += L" -t -m \"" + ExcelFilePath + L"\"";
    NppSSH_LogInfoAuto("【当前执行的命令】" + WStringToLogStr(ExceLogin));
    // 3. 创建PuTTY独立进程
    BOOL bCreateOk = ::CreateProcessW(
        nullptr,                    // lpApplicationName：null 从命令行解析exe
        ExceLogin.data(),                 // lpCommandLine：带引号程序路径
        nullptr,                    // 进程安全属性默认
        nullptr,                    // 线程安全属性默认
        FALSE,                      // 不继承句柄
        CREATE_NEW_PROCESS_GROUP | NORMAL_PRIORITY_CLASS, // 创建独立进程组
        nullptr,                    // 使用当前环境变量
        nullptr,                    // 使用程序所在目录作为工作目录
        &si,                        // 启动信息
        &pi                         // 返回进程/线程句柄
    );

    if (!bCreateOk)
    {
        DWORD errCode = ::GetLastError();
        wchar_t errMsg[1024] = { 0 };
        swprintf_s(errMsg, L"启动 putty.exe 失败\n错误码：%d\n路径：%s", errCode, puttyExePath.c_str());
        ::MessageBoxW(_panelHwnd, errMsg, L"NppSSH 错误提示", MB_OK | MB_ICONERROR);
        char logErr[2048] = { 0 };
        sprintf_s(logErr, "CreateProcessW 启动PuTTY失败，Err=%d Path=%ws", errCode, puttyExePath.c_str());
        NppSSH_LogErrorAuto(std::string(logErr));
        if (!SSH_INITCD.empty())SSH_SettingsDeleteConfigFile(newTmpFile);
        return false;
    }

    CleanInvalidSession();
    // ===== 新建独立会话 =====
    PuTTYSession* pNewSess = new PuTTYSession(_panelSeqId);
    pNewSess->hProcess = pi.hProcess;
    pNewSess->tmpFile = newTmpFile;
    pNewSess->isWindowTop = _winTopState;
    ::CloseHandle(pi.hThread);

    // 启动该会话专属监控线程
    HANDLE hThread = CreateThread(NULL, 0, MonitorThreadProxy, pNewSess, 0, NULL);
    if (hThread != NULL) { pNewSess->hMonitorThread = hThread; }

    // 加锁存入会话列表
    {
        std::lock_guard<std::mutex> lock(_sessionListMtx);
        _sessionList.push_back(pNewSess);
    }

    // 打印调试日志
    NppSSH_LogInfoAuto("成功启动PuTTY进程，路径：" + WStringToLogStr(puttyExePath));
    
    return true;
}

void SSHAppPanel::CloseSoftWare() {
    std::lock_guard<std::mutex> lock(_sessionListMtx);
    if (_sessionList.empty())
    {
        ::MessageBoxW(_panelHwnd, L"当前无任何PuTTY连接会话", L"NppSSH 提示", MB_OK | MB_ICONINFORMATION);
        NppSSH_LogInfoAuto("关闭所有会话：无活跃会话");
        return;
    }
    std::vector<PuTTYSession*> validList;
    for (PuTTYSession* pSess : _sessionList)
    {
        if (!pSess) continue;
        //pSess->StopMonitor();
        DWORD exitCode = 0;
        BOOL bGetExit = ::GetExitCodeProcess(pSess->hProcess, &exitCode);
        if (!bGetExit || exitCode != STILL_ACTIVE) {
            ::MessageBoxW(_panelHwnd, L"PuTTY已自行关闭", L"NppSSH 提示", MB_OK | MB_ICONINFORMATION);
            NppSSH_LogInfoAuto("PuTTY进程已提前退出，清理句柄");
            continue;
        }
        // 存活会话保留
        validList.push_back(pSess);
        // 优雅关闭PuTTY主窗口（发送WM_CLOSE，等效手动点叉）
        if (pSess->hWnd != NULL)
        {
            // 强制PuTTY窗口前置，弹窗会显示在桌面顶层，用户可见
            ::SetForegroundWindow(pSess->hWnd);
            ::BringWindowToTop(pSess->hWnd);
            //NppSSH_LogInfoAuto("已将PuTTY窗口置顶");

            // 异步投递关闭消息，主线程立刻返回，不会卡死NPP
            ::PostMessageW(pSess->hWnd, WM_CLOSE, 0, 0);
            NppSSH_LogInfoAuto("异步投递WM_CLOSE消息至PuTTY窗口，不阻塞主线程");
        }
        else
        {
            // 找不到窗口，弹出确认框，用户确认后才强制终止
            int closeResult = ::MessageBoxW(_panelHwnd,
                L"未找到PuTTY可视窗口，进程仍在后台运行。\n是否确认强制终止Putty进程？",
                L"NppSSH 提示",
                MB_YESNO | MB_ICONWARNING);

            if (closeResult == IDYES)
            {
                ::TerminateProcess(pSess->hProcess, 0);
                NppSSH_LogInfoAuto("用户确认强制终止PuTTY进程");
            }
            else
            {
                NppSSH_LogInfoAuto("用户取消强制终止，放弃关闭PuTTY");
                continue; // 用户选NO，直接退出函数，不清理句柄
            }
        }
    }
    _sessionList.swap(validList);
    NppSSH_LogInfoAuto("全部PuTTY会话清理完毕");
}

void SSHAppPanel::UpdateWinTopBtnUI_FromMemState()
{
    if (!_hBtnWinTop || !::IsWindow(_hBtnWinTop) || !_hToolTip)
        return;

    // 根据内存变量决定图标与提示文本
    int iconId{};
    const WCHAR* tipText{};
    if (_winTopState)
    {
        // true：已经置顶 → 显示【取消置顶】图标，提示文字：取消置顶所有PuTTY窗口
        iconId = IDI_ICON_CLOSEWINTOP;
        tipText = L"点击取消置顶所有PuTTY窗口";
    }
    else
    {
        // false：未置顶 → 显示【置顶】图标，提示文字：置顶所有PuTTY窗口
        iconId = IDI_ICON_WINTOP;
        tipText = L"点击置顶所有PuTTY窗口";
    }

    SetButtonIconOnly(_hBtnWinTop, iconId);// 更新按钮图标
    UpdateToolTipMessage(_hBtnWinTop, tipText);// 更新按钮提示文字
}

bool PuTTYSession::FindPuTTYWindowByPid(DWORD pid, HWND& outHwnd) const
{
    outHwnd = NULL;
    struct EnumWinParam
    {
        HWND wnd;
        DWORD pid;
        EnumWinParam() : wnd(NULL), pid(0) {}
    } enumParam;
    enumParam.pid = pid;

    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL
        {
            EnumWinParam* p = reinterpret_cast<EnumWinParam*>(lp);
            DWORD winPid = 0;
            GetWindowThreadProcessId(hwnd, &winPid);
            if (winPid == p->pid && GetWindow(hwnd, GW_OWNER) == nullptr)
            {
                p->wnd = hwnd;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&enumParam));

    outHwnd = enumParam.wnd;
    SSHAppPanel* pPanel = SSH_PanelVecBySeqIdGetSSHBtnPanel(panelSeqId);
    if (pPanel == nullptr)
        return outHwnd != NULL;
    if (outHwnd != NULL) {
        pPanel->Set_isConnected(true);
    }
    else {
        pPanel->Set_isConnected(false);
    }
    return outHwnd != NULL;
}

// 头文件声明 static DWORD WINAPI PuTTYSessionMonitor(LPVOID lp);
DWORD WINAPI PuTTYSession::PuTTYSessionMonitor()
{
    //PuTTYSession* pSess = reinterpret_cast<PuTTYSession*>(lp);
    NppSSH_LogInfoAuto("新建PuTTY会话监控线程启动");

    const int MAX_SEACH_FIND_WAIT_MS = 1000;
    const int MAX_DELECT_WAIT_MS = 2000;
    int flag = 0;

    // 阶段1：循环查找窗口
    while (!this->stopFlag.load(std::memory_order_acquire))
    {
        {
            std::unique_lock<std::mutex> lock(this->mtx);
            if (this->cv.wait_for(lock, std::chrono::milliseconds(MAX_SEACH_FIND_WAIT_MS),
                [this]() { return this->stopFlag.load(std::memory_order_acquire); }))
            {
                NppSSH_LogInfoAuto("会话监控：收到停止信号退出阶段1");
                goto THREAD_CLEAN;
            }
        }
        flag++;
        DWORD pid = GetProcessId(this->hProcess);
        char pidLog[128] = { 0 };
        sprintf_s(pidLog, "会话PID=%lu 第%d次查找窗口", pid, flag);
        NppSSH_LogInfoAuto(pidLog);

        bool hasWnd = FindPuTTYWindowByPid(pid, this->hWnd);
        if (hasWnd){
            if(isWindowTop)windowTopBtnHandle(this->hWnd, isWindowTop);
            NppSSH_LogInfoAuto("成功捕获PuTTY窗口，进入阶段2等待删除临时文件");
            break;
        }
    }

    // 阶段2：等待后删除临时脚本
    {
        std::unique_lock<std::mutex> lock(this->mtx);
        if (this->cv.wait_for(lock, std::chrono::milliseconds(MAX_DELECT_WAIT_MS),
            [this]() { return this->stopFlag.load(std::memory_order_acquire); }))
        {
            NppSSH_LogInfoAuto("会话监控：阶段2收到停止信号");
            if (!SSH_SettingsGetConfigFileExistPath(this->tmpFile).empty())
                SSH_SettingsDeleteConfigFile(this->tmpFile);
            goto THREAD_CLEAN;
        }
    }
    if (!SSH_SettingsGetConfigFileExistPath(this->tmpFile).empty())
    {
        SSH_SettingsDeleteConfigFile(this->tmpFile);
        NppSSH_LogInfoAuto("会话临时脚本文件删除成功：" + WStringToLogStr(this->tmpFile));
    }

    // 阶段3：持续监听窗口是否消失（进程关闭）
    const int MAX_SEACH_WAIT_MS = 1;
    int retry = 0;
    while (!this->stopFlag.load(std::memory_order_acquire))
    {
        {
            std::unique_lock<std::mutex> lock(this->mtx);
            if (this->cv.wait_for(lock, std::chrono::seconds(MAX_SEACH_WAIT_MS),
                [this]() { return this->stopFlag.load(std::memory_order_acquire); }))
            {
                NppSSH_LogInfoAuto("会话监控：阶段3收到停止信号");
                goto THREAD_CLEAN;
            }
        }
        retry++;
        DWORD pid = GetProcessId(this->hProcess);
        FindPuTTYWindowByPid(pid, this->hWnd);
        if (this->hWnd == NULL)
        {
            NppSSH_LogInfoAuto("PuTTY窗口消失，会话监控线程退出");
            goto THREAD_CLEAN;
        }
    }
THREAD_CLEAN:
    //_isConnected = false;
    this->CleanHandle();
    this->stopFlag.store(true, std::memory_order_release);
    NppSSH_LogInfoAuto("单个PuTTY会话监控线程正常结束");
    return 0;
}



// 设置指定面板永久置顶
void windowTopBtnHandle(HWND hWnd, bool isWinTop) {
    if (isWinTop) {
        ::SetForegroundWindow(hWnd);
        ::SetWindowPos(
            hWnd,
            HWND_TOPMOST,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
        );
        // 强制PuTTY窗口前置，弹窗会显示在桌面顶层，用户可见
        //::BringWindowToTop(pSess->hWnd);

        //NppSSH_LogInfoAuto("已将PuTTY窗口永久置顶");
    }
    else {
        ::SetForegroundWindow(hWnd);
        ::SetWindowPos(
            hWnd,
            HWND_NOTOPMOST,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
        );

        //NppSSH_LogInfoAuto("已取消PuTTY窗口置顶");

    }
}


// NPP启动重建面板具体实现
void SSHAppPanel_InitRecreatePanel(SSHBasePanel* pNewPanel) {
    if (g_nppData._nppHandle == NULL || g_hInst == NULL) {
        ::MessageBoxW(g_nppData._nppHandle, L"NPP环境未初始化，无法重建面板！", L"NppSSH 错误提示", MB_OK | MB_ICONWARNING);
        return;
    }
    SSHAppPanel* pCon = dynamic_cast<SSHAppPanel*>(pNewPanel);
    if (pCon) {
        pCon->initPanel();
        // 获取插件DLL目录，拼接图片路径
        TCHAR szNppPath[MAX_PATH] = { 0 };
        GetModuleFileName(NULL, szNppPath, MAX_PATH);
        PathRemoveFileSpec(szNppPath);
        TCHAR szConfigDir[MAX_PATH] = { 0 };
        _stprintf_s(szConfigDir, MAX_PATH, _T("%s\\plugins\\config\\bg.png"), szNppPath);
        //宽字符转ANSI打印路径
        char logBuf[1024] = { 0 };
        WideCharToMultiByte(CP_UTF8, 0, szConfigDir, -1, logBuf, 1024, NULL, NULL);
        NppSSH_LogInfoAuto(std::string("当前拼接完整图片路径：") + logBuf);
        // 设置图片背景
        pCon->SetBackgroundImage(szConfigDir);

        // 设置面板背景色（黑色示例）
        //pCon->setBackgroundColor(RGB(240, 240, 240));
        pCon->setForegroundColor(RGB(0, 0, 0));
        pCon->display(true);
    }
}