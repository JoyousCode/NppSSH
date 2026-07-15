#include "SSHConEmu.h"

static bool initPanle;//防止未初始化完成就调用面板
SSHConEmu::SSHConEmu(int panelSeqId, int panelrealId)
    :SSHBasePanel(panelSeqId, panelrealId),
    _editLabelFontSize(18),
    _hIconPutty(nullptr),
    _hIconSelectFile(nullptr),
    _hIconDestroy(nullptr),
    _hConEumProcess(nullptr),
    _hConEmuWnd(nullptr),
    _hBtnPutty(nullptr),
    _hBtnDestroy(nullptr),
    _hStaticPuttyTip(nullptr),
    _hEditPuttyPath(nullptr),
    _hBtnSelectFile(nullptr),
    _strPuttyFullPath(L""),
    _hPuttyProcess(NULL) ,
    _hPuTTYWnd(NULL),
    _TempExceFile(L"") {
}
SSHConEmu::~SSHConEmu() {
    StopSeachPutty();
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
    if (_hIconSelectFile)
    {
        ::DestroyIcon(_hIconSelectFile);
        _hIconSelectFile = nullptr;
    }
    // 插件销毁时关闭残留PuTTY并释放句柄
    if (_hPuttyProcess != NULL)
    {
        DWORD exitCode = 0;
        if (::GetExitCodeProcess(_hPuttyProcess, &exitCode) && exitCode == STILL_ACTIVE)
        {
            // 进程仍在运行，关闭PuTTY
            CloseSoftWare();
        }
        ::CloseHandle(_hPuttyProcess);
        _hPuttyProcess = NULL;
    }
}
// 重写背景色
void SSHConEmu::setBackgroundColor(COLORREF color) {
    _bgColor = color;
    // 刷新面板，触发WM_ERASEBKGND、WM_PAINT重绘
    ::InvalidateRect(GetHwndSelf(), nullptr, TRUE);
}
// 重写前景文字色
void SSHConEmu::setForegroundColor(COLORREF color) {
    _fgColor = color;
    ::InvalidateRect(GetHwndSelf(), nullptr, TRUE);
}
// 加载自定义图标（可以替换为自己的图标 ID）
HICON SSHConEmu::LoadCustomIcon(int iconId, int size)
{
    // 校验基础参数
    if (g_hInst == NULL || iconId <= 0 || size <= 0) {
        ::MessageBoxW(g_nppData._nppHandle, L"图标加载参数无效", L"NppSSH错误", MB_OK | MB_ICONWARNING);
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
    // 兜底：加载失败时返回系统默认图标
    if (hIcon == nullptr)
    {
        wchar_t errMsg[256] = { 0 };
        swprintf_s(errMsg, L"图标ID:%d 加载失败，错误码:%d", iconId, ::GetLastError());
        ::MessageBoxW(g_nppData._nppHandle, errMsg, L"NppSSH错误", MB_OK | MB_ICONWARNING);
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
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
    else if (iconId == IDI_ICON_SELECT) {
        if (_hIconSelectFile) ::DestroyIcon(_hIconSelectFile); // 释放旧图标
        _hIconSelectFile = hIcon;
    }
    return hIcon;

}
// 把按钮变成纯图标模式
void SSHConEmu::SetButtonIconOnly(HWND btn, int iconId)
{
    if (btn == nullptr || !::IsWindow(btn))
    {
        ::MessageBoxW(g_nppData._nppHandle, L"按钮句柄无效", L"NppSSH错误", MB_OK | MB_ICONWARNING);
        return; // 窗口无效直接返回，避免崩溃
    }

    // 获取工具栏图标尺寸
    HICON hIcon = LoadCustomIcon(iconId, _iconSize);//_iconSize=24
    if (hIcon == NULL) {
        // 图标加载失败时用系统默认图标（避免报错）
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
        ::MessageBoxW(g_nppData._nppHandle, L"图标加载失败，使用默认图标", L"NppSSH提示", MB_OK | MB_ICONWARNING);
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
void SSHConEmu::OpenPuttyFileDialog()
{
    OPENFILENAMEW ofn = { 0 };
    WCHAR szFile[MAX_PATH] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ZeroMemory(szFile, sizeof(szFile));

    // 预填充当前路径
    wcscpy_s(szFile, _strPuttyFullPath.c_str());

    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = _panelHwnd;
    ofn.hInstance = g_hInst;
    ofn.lpstrFilter = L"PuTTY程序 (putty.exe)\0putty.exe\0所有文件 (*.*)\0*.*\0\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"选择 putty.exe";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING | OFN_NOCHANGEDIR;

    BOOL bOk = ::GetOpenFileNameW(&ofn);
    if (bOk)
    {
        // 更新成员变量 + 编辑框内容
        _strPuttyFullPath = szFile;
        ::SetWindowTextW(_hEditPuttyPath, szFile);
        NppSSH_LogInfoAuto("已选择Putty路径：" + WStringToLogStr(_strPuttyFullPath));
    }
}
void SSHConEmu::SetPathControlFontSize(int fontSize)
{
    // 更新私有字号变量
    _editLabelFontSize = fontSize*0.8;

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
// 创建按钮栏
void SSHConEmu::createButtonBar() {
    if (!GetHwndSelf() || !::IsWindow(GetHwndSelf()))
    {
        ::MessageBoxW(g_nppData._nppHandle, L"面板句柄无效，无法创建按钮", L"NppSSH错误", MB_OK | MB_ICONWARNING);
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
    WCHAR initPath[] = L"D:\\software\\developer\\putty-x64-0.84-cn1\\putty.exe";
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
    //

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

    SetPathControlFontSize(btnInitSize);

    // 将按钮设为纯图标模式（对接自定义图标）
    if (_hBtnSelectFile) {
        SetButtonIconOnly(_hBtnSelectFile, IDI_ICON_SELECT);
    }
    else {
        ::MessageBoxW(g_nppData._nppHandle, L"选择文件按钮创建失败", L"NppSSH错误", MB_OK | MB_ICONWARNING);
    }

    if (_hBtnPutty) {
        SetButtonIconOnly(_hBtnPutty, IDI_ICON_PUTTY);
    }
    else {
        ::MessageBoxW(g_nppData._nppHandle, L"putty连接按钮创建失败", L"NppSSH错误", MB_OK | MB_ICONWARNING);
    }

    if (_hBtnDestroy) {
        SetButtonIconOnly(_hBtnDestroy, IDI_ICON_CLOSE);
        //::EnableWindow(_hBtnDestroy, FALSE);// 初始状态：断开按钮置灰
    }
    else {
        ::MessageBoxW(g_nppData._nppHandle, L"断开按钮创建失败", L"NppSSH错误", MB_OK | MB_ICONWARNING);
    }
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
    // 调用DockingDlgInterface原生create：绑定停靠数据，创建面板窗口
    StaticDialog::create(_dlgID, false);

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
    char bufSelf[64] = { 0 };
    sprintf(bufSelf, "_panelHwnd(_hSelf)=0x%p", _panelHwnd);
    NppSSH_LogInfoAuto(bufSelf);

    createButtonBar();
    // 日志记录（调试/排查）
    NppSSH_LogInfoAuto("面板初始化完成 [序列ID: " + std::to_string(_panelSeqId) + "]");
    NppSSH_LogInfoAuto("面板初始化完成 [标题ID: " + std::to_string(_panelrealId) + "]");
    if (!initPanle) initPanle = true;//面板初始化完成
}




bool SSHConEmu::Set_hPuTTYWnd() {
    DWORD pid = ::GetProcessId(_hPuttyProcess);
    char pidLog[128] = { 0 };
    sprintf_s(pidLog, "当前PuTTY进程PID=%lu", pid);
    NppSSH_LogInfoAuto(pidLog);
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
            // 仅匹配无所有者的主顶层窗口
            if (winPid == p->pid && GetWindow(hwnd, GW_OWNER) == nullptr)
            {
                p->wnd = hwnd;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&enumParam));

    _hPuTTYWnd = enumParam.wnd;
    if (_hPuTTYWnd != NULL)
    {
        return TRUE;
        // 设置全局永久置顶
        //SetWindowPos(_hPuTTYWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    return FALSE;
}
void SSHConEmu::CloseSoftWare() {
    // 1. 校验进程句柄有效性
    if (_hPuttyProcess == NULL)
    {
        ::MessageBoxW(_panelHwnd, L"当前面板未启动PuTTY程序", L"提示", MB_OK | MB_ICONINFORMATION);
        NppSSH_LogInfoAuto("关闭PuTTY失败：无有效进程句柄");
        return;
    }

    // 2. 判断进程是否已经退出
    DWORD exitCode = 0;
    BOOL bGetExit = ::GetExitCodeProcess(_hPuttyProcess, &exitCode);
    if (!bGetExit || exitCode != STILL_ACTIVE)
    {
        ::MessageBoxW(_panelHwnd, L"PuTTY已自行关闭", L"提示", MB_OK | MB_ICONINFORMATION);
        NppSSH_LogInfoAuto("PuTTY进程已提前退出，清理句柄");
        ::CloseHandle(_hPuttyProcess);
        _hPuttyProcess = NULL;
        return;
    }

    // 3. 优雅关闭PuTTY主窗口（发送WM_CLOSE，等效手动点叉）
    if (_hPuTTYWnd != NULL)
    {

        // 强制PuTTY窗口前置，弹窗会显示在桌面顶层，用户可见
        ::BringWindowToTop(_hPuTTYWnd);
        ::SetForegroundWindow(_hPuTTYWnd);
        NppSSH_LogInfoAuto("已将PuTTY窗口置顶");

        // 异步投递关闭消息，主线程立刻返回，不会卡死NPP
        ::PostMessageW(_hPuTTYWnd, WM_CLOSE, 0, 0);
        NppSSH_LogInfoAuto("异步投递WM_CLOSE消息至PuTTY窗口，不阻塞主线程");
    }
    else
    {
        // 找不到窗口，弹出确认框，用户确认后才强制终止
        int closeResult = ::MessageBoxW(_panelHwnd,
            L"未找到PuTTY可视窗口，进程仍在后台运行。\n是否确认强制终止Putty进程？",
            L"NppSSH 连接提示",
            MB_YESNO | MB_ICONWARNING);

        if (closeResult == IDYES)
        {
            ::TerminateProcess(_hPuttyProcess, 0);
            NppSSH_LogInfoAuto("用户确认强制终止PuTTY进程");
        }
        else
        {
            NppSSH_LogInfoAuto("用户取消强制终止，放弃关闭PuTTY");
            return; // 用户选NO，直接退出函数，不清理句柄
        }
    }

    // 4. 等待进程退出，最多等待3秒
    const DWORD waitMs = 10000;
    DWORD waitRet = ::WaitForSingleObject(_hPuttyProcess, waitMs);
    if (waitRet == WAIT_TIMEOUT)
    {
        // 等待超时，弹窗确认是否强制杀死
        int timeoutResult = ::MessageBoxW(_panelHwnd,
            L"PuTTY窗口关闭超时，进程未退出。\n是否确认强制终止Putty进程？",
            L"NppSSH 连接超时",
            MB_YESNO | MB_ICONWARNING);

        if (timeoutResult == IDYES)
        {
            ::TerminateProcess(_hPuttyProcess, 0);
            NppSSH_LogInfoAuto("PuTTY等待关闭超时，用户确认强制结束进程");
        }
        else
        {
            NppSSH_LogInfoAuto("用户取消超时强制终止，放弃关闭PuTTY");
            return;
        }
    }

    // 5. 释放句柄、置空标识
    ::CloseHandle(_hPuttyProcess);
    _hPuttyProcess = NULL;

    ::MessageBoxW(_panelHwnd, L"PuTTY已关闭", L"NppSSH", MB_OK);
    NppSSH_LogInfoAuto("当前面板PuTTY进程关闭完成");
}
void SSHConEmu::ShowPuttyLoginWindow_Modal()
{
    // 1. 取出Putty完整路径
    const std::wstring& puttyExePath = _strPuttyFullPath;

    // 校验路径非空
    if (puttyExePath.empty())
    {
        ::MessageBoxW(_panelHwnd, L"未选择 putty.exe 程序路径！\n请点击浏览按钮指定程序", L"启动失败", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：putty路径为空");
        return;
    }

    // 校验文件是否真实存在
    if (!::PathFileExistsW(puttyExePath.c_str()))
    {
        wchar_t errTip[1024] = { 0 };
        swprintf_s(errTip, L"指定路径不存在 putty.exe：\n\n%s", puttyExePath.c_str());
        ::MessageBoxW(_panelHwnd, errTip, L"启动失败", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：文件不存在 -> " + WStringToLogStr(puttyExePath));
        return;
    }

    // 2. 初始化进程启动参数
    STARTUPINFOW si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL; // 正常窗口显示，和双击打开一致

    // 命令行缓冲区
    std::wstring SSH_HOST = L"192.168.137.201";
    std::wstring SSH_PORT = L"22";
    std::wstring SSH_USER = L"root";
    std::wstring SSH_PASS = L"123456";
    std::wstring SSH_INITCD = L"/";
    std::wstring ExceComd = L"HISTFILE=/dev/null;cd " + SSH_INITCD + L";bash;";
    //std::wstring ExceFile = L"putty_auto_cd.tmp";
    _TempExceFile = L"NppSSH_" + HwndToWString(_panelHwnd)+L".tmp";
    if(!SSH_INITCD.empty())SSH_SettingsSaveConfigTmpFile(_TempExceFile, ExceComd);
    std::wstring ExceLogin = L"\""+puttyExePath+L"\" -ssh \""+SSH_HOST+L"\" -P "+ SSH_PORT+L" -l " + SSH_USER + L" -pw " + SSH_PASS;
    std::wstring ExcelFilePath = SSH_SettingsGetConfigFileExistPath(_TempExceFile);
    if(!ExcelFilePath.empty())ExceLogin += L" -t -m \"" + ExcelFilePath + L"\"";
    NppSSH_LogInfoAuto("【当前执行的命令】"+WStringToLogStr(ExceLogin));
    // 3. 创建PuTTY独立进程
    BOOL bCreateOk = ::CreateProcessW(
        nullptr,                    // lpApplicationName：null 从命令行解析exe
        const_cast<wchar_t*>(ExceLogin.c_str()),                 // lpCommandLine：带引号程序路径
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
        ::MessageBoxW(_panelHwnd, errMsg, L"进程创建失败", MB_OK | MB_ICONERROR);
        char logErr[2048] = { 0 };
        sprintf_s(logErr, "CreateProcessW 启动PuTTY失败，Err=%d Path=%ws", errCode, puttyExePath.c_str());
        NppSSH_LogErrorAuto(std::string(logErr));
        return;
    }

    // 4. 进程创建成功
    _hPuttyProcess = pi.hProcess; // 直接存入类成员，作为唯一标识
    ::CloseHandle(pi.hThread);    // 线程句柄无需保留，直接释放
    
    //Set_hPuTTYWnd();

    // 1. 停止旧线程（防止残留）
    StopSeachPutty();//停止会将stop置为true，启动时候如果为true会直接启动失败，所以需要补充赋值false
    _stopSeachPutty.store(false, std::memory_order_release);

    // 2. 启动本次命令的ShellReader线程
    StartSeachPutty();

    //Sleep(500);
    //SSH_SettingsDeleteConfigFile(ExceFile);
    // 打印调试日志
    NppSSH_LogInfoAuto("成功启动PuTTY进程，路径：" + WStringToLogStr(puttyExePath));
}
void SSHConEmu::StartSeachPutty() {
    std::lock_guard<std::mutex> lock(_SeachPuttyMutex);
    if ( _seachPuttyThread.joinable()) {
        NppSSH_LogInfoAuto("【INFO】等待旧 SeachPuttyThread 线程自然结束");
        //_seachPuttyThread.detach();
        _seachPuttyThread.join();
        return;
    }

    // 重置线程控制状态
    _stopSeachPutty.store(false, std::memory_order_release);

    // 启动线程
    _seachPuttyThread = std::thread(&SSHConEmu::SeachPuttyThread, this);
    if (_seachPuttyThread.joinable())NppSSH_LogInfoAuto("【OK】SeachPuttyThread 线程启动成功");
}
// 后台无运行
void SSHConEmu::SeachPuttyThread() {
    NppSSH_LogInfoAuto("==============================================");
    NppSSH_LogInfoAuto("=        SeachPuttyThread 线程运行中          =");
    NppSSH_LogInfoAuto("==============================================");
    //std::wstring ExceFile = L"putty_auto_cd.tmp";
    //第一阶段：查找Putty窗口
    const int MAX_SEACH_FIND_WAIT_MS = 1000;//查找窗口间隔时间，临界40ms进入第二阶段，41ms就是刚启动的一瞬间
    const int MAX_DELECT_WAIT_MS = 2000;    //等待2s删除文件
    int flag = 0;// 次数,仅仅用于日志打印
    // 循环运行，直到收到停止信号
    while (!_stopSeachPutty.load(std::memory_order_acquire)) {

        {
            std::unique_lock<std::mutex> lock(_SeachPuttyMutex);
            // 等待1秒，或被唤醒（停止信号触发时唤醒）
            if (_seachPuttyCv.wait_for(lock, std::chrono::milliseconds(MAX_SEACH_FIND_WAIT_MS),
                [this]() { return _stopSeachPutty.load(std::memory_order_acquire); })) {
                // 被唤醒且检测到停止信号，直接退出
                NppSSH_LogInfoAuto("等待期间收到停止信号，退出心跳线程");
                goto THREAD_EXIT;
            }
        }

        flag++;
        std::ostringstream oss;
        oss << _seachPuttyThread.get_id();
        NppSSH_LogInfoAuto("线程[" + oss.str() + "]已启动循环.【第" + IntToStr(flag) + "次】");

        if (_stopSeachPutty.load(std::memory_order_acquire)) {
            NppSSH_LogInfoAuto("收到停止信号，立即退出");
            goto THREAD_EXIT;
        }

        bool has_hPuTTYWnd = Set_hPuTTYWnd();
        if (has_hPuTTYWnd) {
            NppSSH_LogInfoAuto("有Putty窗口，线程第一阶段结束");
            break;
        }
    }
    //第二阶段删除临时文件
    {
        std::unique_lock<std::mutex> lock(_SeachPuttyMutex);
        if (_seachPuttyCv.wait_for(lock, std::chrono::milliseconds(MAX_DELECT_WAIT_MS),
            [this]() { return _stopSeachPutty.load(std::memory_order_acquire); })) {
            NppSSH_LogInfoAuto("等待期间收到停止信号，退出心跳线程");
            if (!SSH_SettingsGetConfigFileExistPath(_TempExceFile).empty())SSH_SettingsDeleteConfigFile(_TempExceFile);
            goto THREAD_EXIT;
        }
        if (!SSH_SettingsGetConfigFileExistPath(_TempExceFile).empty())SSH_SettingsDeleteConfigFile(_TempExceFile);//第一阶段结束后删除临时文件
        NppSSH_LogInfoAuto("【删除】"+ WStringToLogStr(_TempExceFile) +"成功，线程第二阶段结束");
    }
    //第三阶段：监听Putty窗口
    const int MAX_SEACH_WAIT_MS = 1;//1s
    int retry = 0;// 次数,仅仅用于日志打印
    // 循环运行，直到收到停止信号
    while (!_stopSeachPutty.load(std::memory_order_acquire)) {
        {
            std::unique_lock<std::mutex> lock(_SeachPuttyMutex);
            // 等待1秒，或被唤醒（停止信号触发时唤醒）
            if (_seachPuttyCv.wait_for(lock, std::chrono::seconds(MAX_SEACH_WAIT_MS),
                [this]() { return _stopSeachPutty.load(std::memory_order_acquire); })) {
                // 被唤醒且检测到停止信号，直接退出
                NppSSH_LogInfoAuto("等待期间收到停止信号，退出心跳线程");
                goto THREAD_EXIT;
            }
        }

        retry++;
        std::ostringstream oss;
        oss << _seachPuttyThread.get_id();
        //NppSSH_LogInfoAuto("线程[" + oss.str() + "]已启动循环.【第" + IntToStr(retry) + "次】");

        if (_stopSeachPutty.load(std::memory_order_acquire)) {
            NppSSH_LogInfoAuto("收到停止信号，立即退出");
            goto THREAD_EXIT;
        }

        bool has_hPuTTYWnd = Set_hPuTTYWnd();
        if (!has_hPuTTYWnd) {
            NppSSH_LogInfoAuto("没有Putty窗口，线程结束");
            goto THREAD_EXIT;
        }
    }
THREAD_EXIT:
    NppSSH_LogInfoAuto("线程正常退出");
    if (_seachPuttyThread.joinable()) {
        _seachPuttyThread.detach();
    }
}
void SSHConEmu::StopSeachPutty() {
    std::lock_guard<std::mutex> lock(_SeachPuttyMutex);
    _stopSeachPutty.store(true, std::memory_order_release);
    if (_hPuTTYWnd == nullptr) {//为空直接放弃，不操作
        NppSSH_LogInfoAuto("为空直接放弃，不操作");
        _seachPuttyThread = std::thread();
        return;
    }
    if (_seachPuttyThread.joinable()) {
        // 唤醒搜索线程（如果在wait_for中阻塞，立即唤醒）
        _seachPuttyCv.notify_one();
        if (_seachPuttyThread.joinable()) {
            NppSSH_LogInfoAuto("停止线程.............直接分离心搜索线程（不等待）");
            _seachPuttyThread.detach();//直接不等待，让线程脱离主线程，自生自灭，根据废掉所有资源会自动销毁
            //m_heartbeatThread.join();//等线程执行完才会执行
        }
        else {
            NppSSH_LogInfoAuto("停止线程.............搜索线程不存在");
        }
    }
}
HBITMAP SSHConEmu::LoadImageByGdiPlus(const WCHAR* filePath)
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
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        if (m_hBgImage) {

            RECT rcClient;
            GetClientRect(GetHwndSelf(), &rcClient);
            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, m_hBgImage);

            // 获取图片原始尺寸
            BITMAP bmpInfo = { 0 };
            GetObject(m_hBgImage, sizeof(BITMAP), &bmpInfo);

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
            break;
        }
        else if (cmd == IDC_BTN_CONNECT_PUTTY) {
            NppSSH_LogInfoAuto("用户点击面板连接按钮，显示登录对话框");
            ShowPuttyLoginWindow_Modal();
        }
        else if (cmd == IDC_BTN_CLOSE_SSH) {
            NppSSH_LogInfoAuto("用户点击面板关闭Putty按钮" + std::to_string(this->_panelSeqId));
            CloseSoftWare();
            //if (_isConnected) {
            //    disconnectSSH(); 
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

        SSH_SettingsByRealIdRemove(this->_panelrealId);
        SSH_PanelVecBySeqIdRemove(_panelSeqId);
        SSH_SettingsSavePanelCount(SSH_PanelVecSize());

        ::DestroyWindow(this->_panelHwnd);
        this->destroy();
        delete this;
        return TRUE;
    }
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
    if (pNewPanel) {
        pNewPanel->initPanel();
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
        pNewPanel->SetBackgroundImage(szConfigDir);

        // 设置面板背景色（黑色示例）
        //pNewPanel->setBackgroundColor(RGB(240, 240, 240));
        pNewPanel->setForegroundColor(RGB(0, 0, 0));
        pNewPanel->display(true);
    }
}