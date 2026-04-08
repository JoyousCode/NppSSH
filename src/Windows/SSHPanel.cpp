// SSHPanel.cpp（面板 + 注册表具体实现）
#include "SSHPanel.h"
#include "SSHSettings.h" // 引入INI工具

// 面板相关全局变量实际定义
static std::vector<NppSSHDockPanel*> s_sshPanels;
static std::atomic<int> s_panelCounter = 0;
static NppData s_nppData;
static HINSTANCE s_hInst;

// 窗口句柄
HWND hHost, hPort, hUser, hPass;
// 全局变量获取接口
std::vector<NppSSHDockPanel*>& SSHPanel_GetGlobalPanels() {
    return s_sshPanels;
}

std::atomic<int>& SSHPanel_GetGlobalPanelCounter() {
    //在所有使用全局句柄（g_hInst、g_nppData）、全局变量（sshSession、sock）的地方，强制添加空值检查，
    //NppData& nppData = SSHPanel_GetGlobalNppData();
    //HINSTANCE& hInst = SSHPanel_GetGlobalHInst();
    //if (nppData._nppHandle == NULL || hInst == NULL) {
    //    ::MessageBoxW(NULL, L"NPP环境未初始化，面板创建失败！", L"NppSSH错误", MB_OK | MB_ICONERROR);
    //    return;
    //}
    return s_panelCounter;
}

NppData& SSHPanel_GetGlobalNppData() {
    return s_nppData;
}

HINSTANCE& SSHPanel_GetGlobalHInst() {
    return s_hInst;
}

// INI操作具体实现（替换原注册表函数）
void SSHPanel_SavePanelCountToIni(int count) {
    SSHSettings_SavePanelCountToIni(count);
}

int SSHPanel_LoadPanelCountFromIni() {
    return SSHSettings_LoadPanelCountFromIni();
}

void SSHPanel_DeletePanelCountFromIni() {
    SSHSettings_DeleteIniConfig();
}

// 面板类构造函数
NppSSHDockPanel::NppSSHDockPanel(int panelId)
    : DockingDlgInterface(IDD_SSH_PANEL),
    _dockData(),
    _panelId(panelId),
    _hOutputEdit(NULL),
    _hBtnConnectSSH(NULL),
    _hBtnDisconnectSSH(NULL),
    _isSSHConnected(false),
    _iconSize(24), // 默认工具栏图标尺寸
    _hIconConnect(NULL) ,
    _hIconDisconnect(NULL) {
    ZeroMemory(_titleBuf, sizeof(_titleBuf));
}
// 析构函数：释放图标资源，防止内存泄漏
NppSSHDockPanel::~NppSSHDockPanel() {
    if (_hIconConnect) ::DestroyIcon(_hIconConnect);
    if (_hIconDisconnect) ::DestroyIcon(_hIconDisconnect);
}
// 判断SSH是否连接
bool NppSSHDockPanel::isSSHConnected() const {
    return _isSSHConnected;
}

// 设置SSH是否连接
void NppSSHDockPanel::setSSHConnected(bool state) {
    _isSSHConnected = state;
    // 连接状态变化时更新按钮图标状态
    if (_hBtnConnectSSH) ::EnableWindow(_hBtnConnectSSH, !state);
    if (_hBtnDisconnectSSH) ::EnableWindow(_hBtnDisconnectSSH, state);

    // 同步更新输出框状态提示
    if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
        if (state) {
            ::SetWindowTextW(_hOutputEdit, L"✅ SSH连接成功！\n可执行SSH命令...");
        }
        else {
            ::SetWindowTextW(_hOutputEdit, L"🔌 SSH已断开\n等待新的连接...");
        }
    }
}

// 断开当前面板的SSH连接（无提示）
void NppSSHDockPanel::disconnectSSH() {
    if (_isSSHConnected) {      // 调用SSHConnection的断开逻辑
        NppSSH_Disconnect();    // 恢复按钮状态
        setSSHConnected(false); // 统一通过set方法更新状态
    }
}

void NppSSHDockPanel::resetPanelToInit() {
    disconnectSSH();
    if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
        ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\n等待SSH连接...");
    }
    // 重置面板时，启用连接SSH按钮（若之前置灰）
    if (_hBtnConnectSSH) ::EnableWindow(_hBtnConnectSSH, TRUE);
    if (_hBtnDisconnectSSH) ::EnableWindow(_hBtnDisconnectSSH, FALSE);
}

// 加载自定义图标（你可以替换为自己的图标 ID）
HICON NppSSHDockPanel::LoadCustomIcon(int iconId, int size)
{
    // 校验基础参数
    if (s_hInst == NULL || iconId <= 0 || size <= 0) {
        ::MessageBoxW(s_nppData._nppHandle, L"图标加载参数无效", L"NppSSH错误", MB_OK | MB_ICONWARNING);
        return LoadIcon(NULL, IDI_APPLICATION);
    }

    // 核心：加载图标（移除LR_LOADFROMFILE，使用资源加载）
    HICON hIcon = (HICON)::LoadImage(
        s_hInst,                  // 全局插件实例句柄（已初始化）
        MAKEINTRESOURCE(iconId),  // 图标 ID（IDC_BTN_CONNECT_SSH/IDC_BTN_DISCONNECT_SSH）
        IMAGE_ICON,               // 资源类型为图标
        size, size,               // 图标大小（跟随工具栏）
        LR_DEFAULTCOLOR  // 默认颜色 + 共享资源（避免内存泄漏）
    );
    // 兜底：加载失败时返回系统默认图标
    if (hIcon == nullptr)
    {
        wchar_t errMsg[256] = { 0 };
        swprintf_s(errMsg, L"图标ID:%d 加载失败，错误码:%d", iconId, ::GetLastError());
        ::MessageBoxW(s_nppData._nppHandle, errMsg, L"NppSSH错误", MB_OK | MB_ICONWARNING);
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    // 持久化到类成员，避免被系统回收
    if (iconId == IDI_ICON_CONNECT) {
        if (_hIconConnect) ::DestroyIcon(_hIconConnect); // 释放旧图标
        _hIconConnect = hIcon;
    }
    else if (iconId == IDI_ICON_DISCONNECT) {
        if (_hIconDisconnect) ::DestroyIcon(_hIconDisconnect); // 释放旧图标
        _hIconDisconnect = hIcon;
    }
    return hIcon;
    
}
// 把按钮变成纯图标模式
void NppSSHDockPanel::SetButtonIconOnly(HWND btn, int iconId)
{
    if (btn == nullptr || !::IsWindow(btn))
    {
        ::MessageBoxW(s_nppData._nppHandle, L"按钮句柄无效", L"NppSSH错误", MB_OK | MB_ICONWARNING);
        return; // 窗口无效直接返回，避免崩溃
    }
    // 调试弹框3：显示按钮信息
    wchar_t btnMsg[256];
    swprintf_s(btnMsg, L"SetButtonIconOnly：\n按钮句柄=%p\n图标ID=%d", btn, iconId);
    //::MessageBoxW(NULL, btnMsg, L"NppSSH调试", MB_OK | MB_ICONINFORMATION);

    // 获取工具栏图标尺寸
    //int iconSize = 36;// 默认图标
    HICON hIcon = LoadCustomIcon(iconId, _iconSize);
    if (hIcon == NULL) {
        // 图标加载失败时用系统默认图标（避免报错）
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
        ::MessageBoxW(s_nppData._nppHandle, L"图标加载失败，使用默认图标", L"NppSSH提示", MB_OK | MB_ICONWARNING);
        return;
    }

    // 先移除所有原有样式，强制设置为纯图标
    ::SetWindowLongPtrW(btn, GWL_STYLE, WS_VISIBLE | WS_CHILD | BS_ICON | WS_BORDER);
    ::SetWindowPos(btn, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED); // 通知样式变更
    // 设置图标后，强制按钮持有句柄
    // 设置图标+按钮尺寸（与工具栏完全一致：图标尺寸+4px边距，匹配NPP工具栏按钮）
    int btnSize = _iconSize + 4;
    ::SendMessage(btn, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIcon);
    ::SetWindowPos(btn, NULL, 0, 0, btnSize, btnSize, SWP_NOMOVE | SWP_NOZORDER);

    // 双重刷新（确保样式和图标生效）
    ::InvalidateRect(btn, NULL, TRUE);
    ::UpdateWindow(btn);
    ::RedrawWindow(btn, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
}

// 当用户修改 Npp 工具栏大小时自动更新
void NppSSHDockPanel::UpdateToolbarIconSize()
{
    //int newIconSize = ::SendMessage(s_nppData._nppHandle, NPPM_GETTOOLBARICONSIZE, 0, 0);
    ::MessageBoxW(s_nppData._nppHandle, L"触发工具栏尺寸更新", L"NppSSH调试", MB_OK | MB_ICONINFORMATION);
    if (_hBtnConnectSSH && IsWindow(_hBtnConnectSSH)) {
        SetButtonIconOnly(_hBtnConnectSSH, IDI_ICON_CONNECT);
    }
    if (_hBtnDisconnectSSH && IsWindow(_hBtnDisconnectSSH)) {
        SetButtonIconOnly(_hBtnDisconnectSSH, IDI_ICON_DISCONNECT);
    }
}
// 创建顶部按钮栏（去掉文字，直接设为图标）
void NppSSHDockPanel::createTopButtonBar() {
    if (!_hSelf || !::IsWindow(_hSelf))
    {
        ::MessageBoxW(s_nppData._nppHandle, L"面板句柄无效，无法创建按钮", L"NppSSH错误", MB_OK | MB_ICONWARNING);
        return;
    }

    RECT rcClient;
    ::GetClientRect(_hSelf, &rcClient);
    const int btnMargin = 5;    // 左边距
    const int btnTop = 2;       // 上边距
    const int btnGap = 10;       // 按钮间距
    const int btnInitSize = _iconSize;  // 按钮初始尺寸

    // 创建「连接SSH」按钮（无文字，后续通过 SetButtonIconOnly 设图标）
    _hBtnConnectSSH = ::CreateWindowW(
        L"BUTTON",
        L"", // 文字设为空
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER,
        btnMargin,
        btnTop,
        btnInitSize, btnInitSize, // 初始大小（后续会自动调整）
        _hSelf,
        (HMENU)IDC_BTN_CONNECT_SSH,
        s_hInst, // 用全局插件实例句柄
        NULL
    );

    // 创建「断开SSH」按钮（无文字）
    _hBtnDisconnectSSH = ::CreateWindowW(
        L"BUTTON",
        L"", // 文字设为空
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER,
        btnMargin + btnInitSize + btnGap, // 左坐标 = 连接按钮 + 间距
        btnTop,
        btnInitSize, btnInitSize, // 初始大小
        _hSelf,
        (HMENU)IDC_BTN_DISCONNECT_SSH,
        s_hInst,
        NULL
    );


    // 将按钮设为纯图标模式（对接自定义图标）
    if (_hBtnConnectSSH) {
        SetButtonIconOnly(_hBtnConnectSSH, IDI_ICON_CONNECT);
    }
    else {
        ::MessageBoxW(s_nppData._nppHandle, L"连接按钮创建失败", L"NppSSH错误", MB_OK | MB_ICONWARNING);
    }
    
    if (_hBtnDisconnectSSH) {
        SetButtonIconOnly(_hBtnDisconnectSSH, IDI_ICON_DISCONNECT);
        ::EnableWindow(_hBtnDisconnectSSH, FALSE);// 初始状态：断开按钮置灰
    }
    else {
        ::MessageBoxW(s_nppData._nppHandle, L"断开按钮创建失败", L"NppSSH错误", MB_OK | MB_ICONWARNING);
    }

    // 复用NPP字体（保持样式统一）
    //HFONT hNppFont = (HFONT)::SendMessage(s_nppData._nppHandle, NPPM_SETSMOOTHFONT, 0, 0);
    //if (hNppFont == NULL) hNppFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    //if (_hBtnConnectSSH) ::SendMessage(_hBtnConnectSSH, WM_SETFONT, (WPARAM)hNppFont, TRUE);
    //if (_hBtnDisconnectSSH) ::SendMessage(_hBtnDisconnectSSH, WM_SETFONT, (WPARAM)hNppFont, TRUE);
}

// 面板初始化：纯原生接口
void NppSSHDockPanel::initPanel() {
    // 检查资源是否存在
    HRSRC hRes = ::FindResource(s_hInst, MAKEINTRESOURCE(IDD_SSH_PANEL), RT_DIALOG);
    if (hRes == NULL) {
        wchar_t errMsg[256] = { 0 };
        swprintf_s(errMsg, L"找不到IDD_SSH_PANEL资源！GetLastError: %d", ::GetLastError());
        ::MessageBoxW(s_nppData._nppHandle, errMsg, L"NppSSH资源错误", MB_OK | MB_ICONERROR);
        return;
    }

    DockingDlgInterface::init(s_hInst, s_nppData._nppHandle);   // 1. 调用DockingDlgInterface原生init：绑定NPP实例和父窗口
    ZeroMemory(&_dockData, sizeof(tTbData));                    // 2. 初始化原生tTbData结构体（完全按Docking.h定义，无多余成员）

    // 面板标签名（多标签区分：NppSSH-1、NppSSH-2...，NPP底部标签栏显示）
    std::wstring panelTitle = L"NppSSH-" + std::to_wstring(_panelId);
    wcscpy_s(_titleBuf, _countof(_titleBuf), panelTitle.c_str());

    _dockData.pszName = _titleBuf;                           // 原生成员：面板名称
    _dockData.uMask = DWS_DF_CONT_BOTTOM | DWS_DF_FLOATING;  // 面板默认停靠在底部和允许面板浮动为独立窗口
    _dockData.iPrevCont = CONT_BOTTOM;                       // 原生要求：记录上一次停靠位置为底部
    _dockData.dlgID = IDD_SSH_PANEL;                        // 原生成员：对话框ID
    _dockData.pszModuleName = this->getPluginFileName();    // 原生方法：获取插件模块名（NPP识别用）
    _dockData.hIconTab = nullptr;                           // 图标设为null，复用NPP默认图标（无报错）
    _dockData.pszAddInfo = nullptr;                         // 无额外信息，设为null

    // 调用DockingDlgInterface原生create：绑定停靠数据，创建面板窗口
    //DockingDlgInterface::create(&_dockData);
    StaticDialog::create(_dlgID, false);
    _dockData.hClient = _hSelf;
    if (!_hSelf) {
        ::MessageBoxW(s_nppData._nppHandle, L"面板窗口创建失败！", L"NppSSH错误", MB_OK | MB_ICONERROR);
        return;
    }
    ::SendMessage(s_nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&_dockData));
    ::SendMessage(s_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD, reinterpret_cast<LPARAM>(_hSelf));

    // 调用创建顶部按钮栏
    createTopButtonBar();

    //  从资源中获取EDIT控件句柄（不再手动CreateWindow）
    _hOutputEdit = ::GetDlgItem(_hSelf, IDC_OUTPUT_EDIT);
    if (_hOutputEdit) {
        ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\n等待SSH连接...");           // 设置初始文本 + 复用NPP字体
        HFONT hNppFont = (HFONT)::SendMessage(s_nppData._nppHandle, NPPM_SETSMOOTHFONT, 0, 0);
        if (hNppFont == NULL) hNppFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        ::SendMessage(_hOutputEdit, WM_SETFONT, (WPARAM)hNppFont, TRUE);
        ::SendMessage(_hOutputEdit, EM_GETLINECOUNT, 0, 0);     // 设置编辑框自适应换行/滚动
        ::SendMessage(_hOutputEdit, EM_SETTABSTOPS, 1, (LPARAM)8);
        // 调整输出编辑框位置，避开顶部按钮栏
        RECT rcClient;
        ::GetClientRect(_hSelf, &rcClient);
        // 编辑框：左10、上50（避开按钮栏）、右-20、下-20，与按钮栏保留间距
        ::SetWindowPos(
            _hOutputEdit,
            NULL,
            5, _iconSize + 12, rcClient.right - 10, rcClient.bottom - 50,
            SWP_NOZORDER | SWP_NOACTIVATE
        );
    }
    

    if (_hSelf && ::IsWindow(_hSelf)) {         // 强制设置面板窗口样式，解决遮挡/闪烁问题
        DWORD dwStyle = ::GetWindowLongPtrW(_hSelf, GWL_STYLE);
        ::SetWindowLongPtrW(_hSelf, GWL_STYLE, dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        ::SetWindowPos(_hSelf, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);     // 确保面板在停靠容器的顶层，不被覆盖
    }
    // 加入全局管理，支持标签切换和内存清理
    s_sshPanels.push_back(this);
}
// SSH登录窗口-连接回调
void NppSSHDockPanel::OnConnect(HWND hWnd, NppSSHDockPanel* pPanel) {
    char host[256] = { 0 };
    char port[32] = { 0 };
    char user[256] = { 0 };
    char pass[256] = { 0 };

    GetWindowTextA(hHost, host, 256);
    GetWindowTextA(hPort, port, 32);
    GetWindowTextA(hUser, user, 256);
    GetWindowTextA(hPass, pass, 256);
    int nPort = atoi(port);
    bool ok = NppSSH_Connect(host, nPort, user, pass);
    
    // 结果提示（用Unicode避免乱码）
    if (ok) {
        DestroyWindow(hWnd);//连接成功关闭窗口
        // 同步面板连接状态
        if (pPanel) {
            
            pPanel->setSSHConnected(ok);
            ::MessageBoxW(s_nppData._nppHandle, L"SSH 连接成功 ✅", L"NppSSH提示", MB_OK);
        }
        //::MessageBoxW(s_nppData._nppHandle, L"SSH 连接失败 ❌", L"NppSSH提示", MB_OK);
        
    }
}

// SSH登录窗口-窗口过程（修复消息处理，关联面板实例）
LRESULT CALLBACK NppSSHDockPanel::SSH_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static NppSSHDockPanel* pPanel = nullptr;
    static BOOL s_isModalLock = FALSE; // 标记是否锁定父窗口
    // 传递面板实例（WM_CREATE时绑定）
    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pPanel = (NppSSHDockPanel*)pCreate->lpCreateParams;
    }
    switch (msg)
    {   
    case WM_INITDIALOG:
    case WM_SHOWWINDOW:
    {
        //官方标准模态：禁用父窗口
        if (!s_isModalLock && s_nppData._nppHandle != NULL)
        {
            EnableWindow(s_nppData._nppHandle, FALSE); // 🔴 禁用 NPP 主窗口
            s_isModalLock = TRUE;
        }

        // 强制置顶 + 取焦点
        //SetForegroundWindow(hWnd);
        //SetFocus(hWnd);
        return 0;
    }
    case WM_CREATE:
    {
        // 标签（Unicode版，中文正常显示）
        CreateWindowW(L"STATIC", L"主机:", WS_VISIBLE | WS_CHILD, 20, 20, 50, 20, hWnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"端口:", WS_VISIBLE | WS_CHILD, 20, 50, 50, 20, hWnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"用户:", WS_VISIBLE | WS_CHILD, 20, 80, 50, 20, hWnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"密码:", WS_VISIBLE | WS_CHILD, 20, 110, 50, 20, hWnd, NULL, NULL, NULL);

        // 输入框
        hHost = CreateWindowA("EDIT", "192.168.137.200", WS_VISIBLE | WS_CHILD | WS_BORDER, 70, 20, 200, 25, hWnd, (HMENU)IDC_HOST, NULL, NULL);
        hPort = CreateWindowA("EDIT", "22", WS_VISIBLE | WS_CHILD | WS_BORDER, 70, 50, 200, 25, hWnd, (HMENU)IDC_PORT, NULL, NULL);
        hUser = CreateWindowA("EDIT", "root", WS_VISIBLE | WS_CHILD | WS_BORDER, 70, 80, 200, 25, hWnd, (HMENU)IDC_USER, NULL, NULL);
        hPass = CreateWindowA("EDIT", "123456", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD, 70, 110, 200, 25, hWnd, (HMENU)IDC_PASS, NULL, NULL);

        // 连接按钮（Unicode中文）
        CreateWindowW(L"BUTTON", L"确认连接", WS_VISIBLE | WS_CHILD, 70, 150, 80, 30, hWnd, (HMENU)IDC_BTN_CONNECT, NULL, NULL);
        // 弹出后自动获取焦点 
        //SetForegroundWindow(hWnd);
        //SetFocus(hWnd);
        return 0;
    }
    // 失去焦点自动抢回（无定时器，纯消息拦截）
    case WM_KILLFOCUS:
    {
        HWND hNew = (HWND)wParam;
        if (!IsChild(hWnd, hNew))
        {
            SetFocus(hWnd); // 强制焦点留在窗口
        }
        return 0;
    }

    case WM_COMMAND:
    {
        if (pPanel && (LOWORD(wParam) == IDC_BTN_CONNECT)) { // 加空指针保护，避免崩溃
            pPanel->OnConnect(hWnd, pPanel);
        }
        return 0;
    }
    // 关闭时弹框恢复 NPP 主窗口（必须！）
    case WM_DESTROY:
    {
        //PostQuitMessage(0);
        if (s_isModalLock && s_nppData._nppHandle != NULL)
        {
            EnableWindow(s_nppData._nppHandle, TRUE); // ✅ 恢复 NPP
            s_isModalLock = FALSE;
        }
        return 0;
    }
        
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}


// 显示SSH登录窗口（绑定当前面板实例）
void NppSSHDockPanel::ShowSSHLoginWindow() {
    const wchar_t* CLASS_NAME = L"NppSSHLoginWindow";
    
    //// 注册窗口类
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = SSH_WndProc;
    wc.hInstance = s_hInst;
    //wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW; // 重绘优化

    // 避免重复注册
    if (!GetClassInfoExW(s_hInst, CLASS_NAME, &wc)) {
        RegisterClassExW(&wc);
    }

    // 窗口尺寸
    int winW = 300;
    int winH = 240;
    // 计算屏幕居中坐标
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;
    // 创建窗口（Unicode标题 + 居中位置）
    HWND hWnd = CreateWindowExW(
        0, CLASS_NAME, L"NppSSH 连接",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, winW, winH,
        s_nppData._nppHandle, NULL, s_hInst, this // 传递面板实例
    );

    // 模态消息循环（NPP官方插件标准用法）
    // // 模态消息循环
    if (hWnd) {
        MSG msg;
        while (IsWindowVisible(hWnd) && GetMessageW(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    // 优化消息循环，确保窗口销毁后立即退出
    //if (hWnd) {
    //    MSG msg;
    //    // 仅处理当前窗口的消息，窗口销毁后直接退出
    //    while (IsWindow(hWnd) && GetMessage(&msg, hWnd, 0, 0)) {
    //        if (!IsDialogMessage(hWnd, &msg)) {
    //            TranslateMessage(&msg);
    //            DispatchMessageW(&msg);
    //        }
    //    }
    //    // 强制销毁窗口（双重保险）
    //    if (IsWindow(hWnd)) {
    //        DestroyWindow(hWnd);
    //    }
    //}
}

// 重写原生run_dlgProc：创建面板内UI，处理窗口消息（纯原生）
INT_PTR CALLBACK NppSSHDockPanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {      // 原生消息：面板窗口创建完成，初始化内部UI（输出文本框）
    case WM_INITDIALOG: {
        if (!_hSelf) {
            ::MessageBox(NULL, TEXT("面板窗口句柄无效！"), TEXT("NppSSH错误提示"), MB_OK | MB_ICONERROR);
            return FALSE;
        }
        return TRUE;
    }
    // 面板大小变化时，自动适配输出文本框（防止遮挡/空白）（最小化关闭/打开notepad++会自动触发）
    case WM_SIZE: {
        //::MessageBoxW(s_nppData._nppHandle, L"SSH变化3", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
        if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
            RECT rc;
            ::GetClientRect(_hSelf, &rc);
            // ====== 同步修改WM_SIZE中的编辑框位置，适配按钮栏 ======
            ::SetWindowPos(
                _hOutputEdit,
                NULL,
                5, _iconSize + 12, rc.right - 10, rc.bottom - 50,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
        }
        return TRUE;
    }
    // 处理按钮点击消息
    case WM_COMMAND: {
        UINT cmd = LOWORD(wParam);
        if (cmd == IDC_BTN_CONNECT_SSH) {
            ShowSSHLoginWindow(); // 显示登录窗口（关联当前面板）
        }
        else if (cmd == IDC_BTN_DISCONNECT_SSH) {
            disconnectSSH(); // 断开连接
            if (_hOutputEdit) {
                ::SetWindowTextW(_hOutputEdit, L"✅ SSH已断开\n等待新的连接...");
            }
            ::MessageBoxW(s_nppData._nppHandle, L"SSH连接已断开", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
        }
        return TRUE;
    }
    // 响应NPP停靠管理器的浮动/停靠消息，更新面板状态
    case WM_NOTIFY: {
        LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
        if (pnmh->hwndFrom == s_nppData._nppHandle) {
            switch (LOWORD(pnmh->code)) {
            case DMN_FLOAT: _isFloating = true; break;
            case DMN_DOCK: _isFloating = false; _iDockedPos = HIWORD(pnmh->code); break;
            case DMN_CLOSE: ::PostMessage(_hSelf, WM_CLOSE, 0, 0); break;
            }
        }
        return TRUE;
    }

    // 面板关闭：原生NPP消息，自动清理资源，无内存泄漏
    case WM_CLOSE: {
        ::MessageBoxW(s_nppData._nppHandle, L"SSH变化3关闭面板", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);

        // 检查当前面板是否有活跃SSH连接
        if (this->isSSHConnected()) {
            ::MessageBoxW(
                s_nppData._nppHandle,
                L"当前面板存在SSH活跃连接，关闭将断开连接并恢复初始状态！",
                L"NppSSH 连接提示",
                MB_YESNO | MB_ICONWARNING
            );
            this->resetPanelToInit();   // 重置为初始状态
        }
        // 从NPP原生停靠管理器移除面板
        ::SendMessage(s_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, (LPARAM)getHSelf());
        ::SendMessage(s_nppData._nppHandle, NPPM_DMMHIDE, 0, (LPARAM)getHSelf());

        auto it = std::find(s_sshPanels.begin(), s_sshPanels.end(), this);      // 从全局管理数组中移除
        if (it != s_sshPanels.end()) s_sshPanels.erase(it);

        // 原生停靠面板清理：先隐藏，再销毁
        this->display(false);
        ::DestroyWindow(_hSelf);
        this->destroy();
        SSHPanel_SavePanelCountToIni(s_sshPanels.size());    // 同步更新INI面板数量
        delete this;                                // 释放面板实例
        return TRUE;
    }
                 // 工具栏图标大小变化时更新按钮图标
    case NPPN_TOOLBARICONSETCHANGED: {
        UpdateToolbarIconSize();
        return TRUE;
    }
                 // 其他所有消息，交给DockingDlgInterface原生处理（避免NPP异常）
    default:
        return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
    }
}

// NPP启动重建面板具体实现
void SSHPanel_RecreatePanelsOnNppStart() {
    if (s_nppData._nppHandle == NULL || s_hInst == NULL) {
        ::MessageBoxW(s_nppData._nppHandle, L"NPP环境未初始化，无法重建面板！", L"NppSSH错误", MB_OK | MB_ICONWARNING);
        return;
    }
    //int panelCount = SSHPanel_LoadPanelCountFromReg();
    int panelCount = SSHPanel_LoadPanelCountFromIni(); // 从INI加载
    if (panelCount <= 0) return;
    // 按注册表记录的数量重建面板，ID延续自注册表
    for (int i = 1; i <= panelCount; i++) {
        s_panelCounter = i;// 同步计数器，保证新创建面板ID不重复
        NppSSHDockPanel* pNewPanel = new NppSSHDockPanel(i);
        if (pNewPanel) {
            pNewPanel->initPanel();
            ::SendMessage(s_nppData._nppHandle, NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(pNewPanel->getHSelf()));
        }
    }
}