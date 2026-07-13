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
    _strPuttyFullPath(L""){
}
SSHConEmu::~SSHConEmu() {
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
}
void SSHConEmu::setBackgroundColor(COLORREF color) {

}
// 重写前景文字色
void SSHConEmu::setForegroundColor(COLORREF color) {

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
    int btnSize = _iconSize + 4;
    ::SendMessage(btn, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIcon);
    //::SetWindowPos(btn, NULL, 0, 0, btnSize, btnSize, SWP_NOMOVE | SWP_NOZORDER);

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
    _strPuttyFullPath = initPath;
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

    // 获取 ConEmu 路径
    //std::wstring conemuRootDir = SSH_SettingsGetPluginsDir()+ L"\\NppSSH\\ConEmuPack.230724";
    //std::wstring conemuExeFullPath = conemuRootDir + L"\\ConEmu64.exe";
    //std::wstring conemuCPath = conemuRootDir + L"\\ConEmu\\ConEmuC64.exe";

    //NppSSH_LogInfoAuto(std::string("【当前拼接完整ConEmu路径】：") + WStringToLogStr(conemuExeFullPath));

    //// 检查 ConEmu64.exe 是否存在
    //DWORD fileAttr = GetFileAttributesW(conemuExeFullPath.c_str());
    //if (fileAttr == INVALID_FILE_ATTRIBUTES || (fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
    //    ::MessageBoxW(_panelHwnd, L"错误：ConEmu64.exe 不存在", L"NppSSH", MB_ICONERROR);
    //    DestroyWindow(_panelHwnd);
    //    _panelHwnd = nullptr;
    //    return;
    //}

    //// 检查 ConEmuC64.exe 是否存在
    //fileAttr = GetFileAttributesW(conemuCPath.c_str());
    //if (fileAttr == INVALID_FILE_ATTRIBUTES || (fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
    //    ::MessageBoxW(_panelHwnd, L"错误：ConEmuC64.exe 不存在", L"NppSSH", MB_ICONERROR);
    //    DestroyWindow(_panelHwnd);
    //    _panelHwnd = nullptr;
    //    return;
    //}
    //// 容器句柄转为十六进制字符串
    //WCHAR hwndHexStr[32] = { 0 };
    //swprintf_s(hwndHexStr, L"0x%p", _panelHwnd);

    //// ========== 第一步：启动 ConEmu64 -insidewnd 创建嵌套窗口（仅创建窗口，不启动终端会话） ==========
    //// -insidewnd: 嵌入到指定父窗口
    //// -NoAutoClose: 最后一个标签关闭时不自动关闭ConEmu
    //// -Detached: 启动时不创建控制台（只有GUI窗口）
    //std::wstring puttyExePath = L"D:\\software\\developer\\putty-x64-0.84-cn1\\putty.exe";
    //std::wstring puttyLaunchParam = puttyExePath+  L" -load \"localhost\" -l root -pw 123456";
    //// ConEmu -run 内置GuiMacro
    //std::wstring runScript = puttyLaunchParam + L" -new_console";
    //std::wstring  cmdLine = L"\"" + conemuExeFullPath + L"\" "
    //    + L"-insidewnd " + std::wstring(hwndHexStr) + L" "
    //    + L"-NoAutoClose -Detached "
    //    + L"-run \"" + runScript + L"\"";

    //// 2. 创建双向匿名管道（注意：这里需要一对管道，而不是一个）
    //// 因为你需要向 mintty 写数据，也需要从 mintty 读数据
    //SECURITY_ATTRIBUTES saPipe = { 0 };
    //saPipe.nLength = sizeof(SECURITY_ATTRIBUTES);
    //saPipe.bInheritHandle = TRUE;
    //saPipe.lpSecurityDescriptor = nullptr;

    //HANDLE hPipeRead = nullptr;    // 子进程的 stdin（父进程写）
    //HANDLE hPipeWrite = nullptr;   // 父进程的 stdout（子进程写）
    //HANDLE hPipeReadChild = nullptr; // 子进程的 stdout（父进程读）
    //HANDLE hPipeWriteChild = nullptr; // 父进程的 stdin（子进程读）
    //// 创建第一对管道：父写 -> 子读
    //if (!CreatePipe(&hPipeRead, &hPipeWrite, &saPipe, 0))
    //{
    //    DWORD err = GetLastError();
    //    wchar_t errMsg[256] = { 0 };
    //    swprintf_s(errMsg, L"创建管道1失败，错误码: %d", err);
    //    ::MessageBoxW(_panelHwnd, errMsg, L"NppSSH", MB_OK);
    //    DestroyWindow(_panelHwnd);
    //    _panelHwnd = nullptr;
    //    return;
    //}
    //// 创建第二对管道：子写 -> 父读
    //if (!CreatePipe(&hPipeReadChild, &hPipeWriteChild, &saPipe, 0))
    //{
    //    DWORD err = GetLastError();
    //    wchar_t errMsg[256] = { 0 };
    //    swprintf_s(errMsg, L"创建管道2失败，错误码: %d", err);
    //    ::MessageBoxW(_panelHwnd, errMsg, L"NppSSH", MB_OK);
    //    CloseHandle(hPipeRead);
    //    CloseHandle(hPipeWrite);
    //    DestroyWindow(_panelHwnd);
    //    _panelHwnd = nullptr;
    //    return ;
    //}
    //// 5. 初始化进程/线程安全属性
    //SECURITY_ATTRIBUTES saProcess = { 0 };
    //SECURITY_ATTRIBUTES saThread = { 0 };
    //saProcess.nLength = sizeof(SECURITY_ATTRIBUTES);
    //saProcess.bInheritHandle = TRUE;
    //saProcess.lpSecurityDescriptor = nullptr;
    //saThread.nLength = sizeof(SECURITY_ATTRIBUTES);
    //saThread.bInheritHandle = TRUE;
    //saThread.lpSecurityDescriptor = nullptr;

    //STARTUPINFOW si = { 0 };
    //PROCESS_INFORMATION pi = { 0 };
    //ZeroMemory(&si, sizeof(STARTUPINFOW));
    //ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    //si.cb = sizeof(STARTUPINFOW);
    //si.dwFlags = STARTF_USESHOWWINDOW;
    //si.wShowWindow = SW_HIDE;

    //BOOL bRet = CreateProcessW(
    //    nullptr,
    //    const_cast<wchar_t*>(cmdLine.c_str()),
    //    nullptr,
    //    nullptr,
    //    TRUE,
    //    CREATE_NO_WINDOW,
    //    nullptr,
    //    nullptr,
    //    &si,
    //    &pi
    //);
    ////BOOL bRet = CreateProcessW(
    ////    nullptr,                     // lpApplicationName：不指定，由 lpCommandLine 解析
    ////    &cmdLine[0],                 // lpCommandLine：可写缓冲区，强制带引号
    ////    &saProcess,                         // lpProcessAttributes：安全属性，允许句柄继承
    ////    &saThread,                         // lpThreadAttributes：线程安全属性，同进程
    ////    TRUE,                        // bInheritHandles：允许子进程继承句柄
    ////    CREATE_NO_WINDOW,                           // dwCreationFlags：增加 CREATE_NO_WINDOW 标志，避免独立弹窗干扰
    ////    nullptr,                     // lpEnvironment：使用父进程环境变量
    ////    nullptr,                   // lpCurrentDirectory：工作目录，强制处理后的路径
    ////    &si,                         // lpStartupInfo：显式初始化的 STARTUPINFOW
    ////    &pi                          // lpProcessInformation：接收进程信息
    ////);

    //if (!bRet)
    //{
    //    DWORD err = GetLastError();
    //    WCHAR errMsg[256] = { 0 };
    //    swprintf_s(errMsg, L"启动ConEmu失败，错误码：%d", err);
    //    ::MessageBoxW(_panelHwnd, errMsg, L"NppSSH", MB_OK);
    //    DestroyWindow(_panelHwnd);
    //    _panelHwnd = nullptr;
    //    return;
    //}
    //// 保存进程句柄，关闭线程句柄
    //_hConEumProcess = pi.hProcess;
    //CloseHandle(pi.hThread);
    //WaitForInputIdle(pi.hProcess, 2000);
    //ShowWindow(_panelHwnd, SW_SHOW);

    //// 等待ConEmu窗口完全就绪
    //Sleep(1500);

    //// 获取ConEmu窗口句柄（通过枚举子窗口）
    //_hConEmuWnd = FindWindowExW(_panelHwnd, nullptr, nullptr, nullptr);
    //if (!_hConEmuWnd) {
    //    // 如果直接查找失败，尝试延迟后再次查找
    //    Sleep(500);
    //    _hConEmuWnd = FindWindowExW(_panelHwnd, nullptr, nullptr, nullptr);
    //}
    // 日志记录（调试/排查）
    NppSSH_LogInfoAuto("面板初始化完成 [序列ID: " + std::to_string(_panelSeqId) + "]");
    NppSSH_LogInfoAuto("面板初始化完成 [标题ID: " + std::to_string(_panelrealId) + "]");
    if (!initPanle) initPanle = true;//面板初始化完成
}

void SSHConEmu::ShowPuttyLoginWindow_Modal()
{
    ::MessageBoxW(_panelHwnd, L"登录Putty", L"NppSSH", MB_OK);
    // 官方SDK标准：DialogBoxParam 模态对话框
    // 父窗口固定为 NPP 主窗口，自动管理Z序、激活状态、禁用/恢复
    //DialogBoxParamW(
    //    g_hInst,
    //    MAKEINTRESOURCE(IDD_SSH_LOGIN),
    //    g_nppData._nppHandle,  // 关键：父窗口是NPP主窗口
    //    SSH_LoginDlgProc,
    //    (LPARAM)this
    //);
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
        return (LRESULT)::GetStockObject(WHITE_BRUSH);
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
            NppSSH_LogInfoAuto("用户点击面板断开按钮" + std::to_string(this->_panelSeqId));
            //if (_isConnected) {
            //    disconnectSSH(); // 断开连接
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
        //NppSSH_LogInfoAuto("【NMHDR】" + std::string(bufNMHDR));
        // 提前预存关闭确认结果，避免在case内部模态阻塞
        //int closeResult = IDNO;
        // 1.消息来源是面板：全部放行，交给编辑框子类处理
        if (pnmh->hwndFrom == g_nppData._nppHandle && pnmh->code == DMN_CLOSE)
        {
            //NppSSH_LogInfoAuto("面板【准备】关闭，当前连接状态：" + std::to_string(_isConnected) + "【触发关闭，执行断开】" + std::string(bufNMHDR));
            //const bool bHasActiveConn = this->Get_isConnected();

            // 检查当前面板是否有活跃SSH连接
            //if (bHasActiveConn)
            //{
            //    this->disconnectSSH();   // 断开连接
            //    this->display(false);//准备销毁，先隐藏防止不完整的面板出现影响效果
            //}
            SendMessageW(GetHwndSelf(), WM_CLOSE, wParam, lParam);
        }

         //2.消息来源是Terminal富文本：全部放行，交给编辑框子类处理
        //if (pnmh->hwndFrom == this->_hOutputEdit)
        //{
        //    //NppSSH_LogInfoAuto("父转发Terminal通知 code:" + std::to_string(pnmh->code));
        //    SendMessageW(_hOutputEdit, WM_NOTIFY, wParam, lParam);
        //}
        return TRUE;
    }
    // 面板关闭：原生NPP消息，自动清理资源，无内存泄漏
    case WM_CLOSE:
    {
        NppSSH_LogInfoAuto("面板【开始】关闭，当前连接状态：" + std::to_string(_isConnected));
        //SSH_TerminalBySeqIdRemove(_panelSeqId);
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
        pNewPanel->setForegroundColor(RGB(255, 0, 0));
        pNewPanel->display(true);
    }
}