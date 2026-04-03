// SSHPanel.cpp（面板 + 注册表具体实现）
//#ifndef NPPM_GETTOOLBARICONSIZE
//#define NPPM_GETTOOLBARICONSIZE (NPPMSG + 1000) // 改用更高的ID，避免与NPP内部冲突
//#endif
//#ifndef NPPN_TOOLBARUPDATED
//#define NPPN_TOOLBARUPDATED NPPN_TOOLBARICONSETCHANGED // 直接复用官方通知码
//#endif
#include "SSHPanel.h"
#include "SSHSettings.h" // 引入INI工具

// 面板相关全局变量实际定义
static std::vector<NppSSHDockPanel*> s_sshPanels;
static std::atomic<int> s_panelCounter = 0;
static NppData s_nppData;
static HINSTANCE s_hInst;

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
    _isSSHConnected(false) {
    ZeroMemory(_titleBuf, sizeof(_titleBuf));
}
// TODO:登录成功后,添加: _setSSHConnected(true);
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
}

// 断开当前面板的SSH连接（无提示）
void NppSSHDockPanel::disconnectSSH() {
    if (_isSSHConnected) {
        // 调用SSHConnection的断开逻辑
        NppSSH_Disconnect();
        _isSSHConnected = false;
        // 恢复按钮状态
        if (_hBtnConnectSSH) ::EnableWindow(_hBtnConnectSSH, TRUE);
        if (_hBtnDisconnectSSH) ::EnableWindow(_hBtnDisconnectSSH, FALSE);
    }
}

void NppSSHDockPanel::resetPanelToInit() {
    disconnectSSH();
    if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
        ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\n等待SSH连接...");
    }
    // 可选：重置面板时，启用连接SSH按钮（若之前置灰）
    if (_hBtnConnectSSH) ::EnableWindow(_hBtnConnectSSH, TRUE);
    if (_hBtnDisconnectSSH) ::EnableWindow(_hBtnDisconnectSSH, FALSE);
}
// 新增：创建面板顶部独立按钮栏
//void NppSSHDockPanel::createTopButtonBar() {
//    if (!_hSelf || !::IsWindow(_hSelf)) return;
//
//    // 获取面板客户区大小，确定按钮位置（顶部横向排列，左对齐，边距10）
//    RECT rcClient;
//    ::GetClientRect(_hSelf, &rcClient);
//    int btnWidth = 80;  // 按钮宽度
//    int btnHeight = 28; // 按钮高度
//    int btnMargin = 10; // 按钮左右边距
//    int btnTop = 10;    // 按钮顶部边距
//    int btnGap = 10; // 按钮之间的间距
//
//    // 创建「连接SSH」按钮（Unicode版，适配NPP中文环境）
//    _hBtnConnectSSH = ::CreateWindowW(
//        L"BUTTON",                // 控件类名
//        L"连接",               // 按钮文字
//        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER, // 控件样式
//        btnMargin,                // 左坐标
//        btnTop,                   // 上坐标
//        btnWidth,                 // 宽度
//        btnHeight,                // 高度
//        _hSelf,                   // 父窗口句柄
//        (HMENU)IDC_BTN_CONNECT_SSH, // 按钮ID
//        ::GetModuleHandleW(NULL), // 实例句柄
//        NULL
//    );
//    // 新增：创建「断开SSH」按钮（在连接按钮右侧，间隔10px）
//    _hBtnDisconnectSSH = ::CreateWindowW(
//        L"BUTTON",
//        L"断开",
//        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER,
//        btnMargin + btnWidth + btnGap, // 左坐标 = 连接按钮左 + 宽度 + 间距
//        btnTop,
//        btnWidth,
//        btnHeight,
//        _hSelf,
//        (HMENU)IDC_BTN_DISCONNECT_SSH,
//        ::GetModuleHandleW(NULL),
//        NULL
//    );
//
//    // 复用NPP默认字体，保证按钮样式与NPP统一
//    HFONT hNppFont = (HFONT)::SendMessage(s_nppData._nppHandle, NPPM_SETSMOOTHFONT, 0, 0);
//    if (hNppFont == NULL) hNppFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
//    ::SendMessage(_hBtnConnectSSH, WM_SETFONT, (WPARAM)hNppFont, TRUE);
//    ::SendMessage(_hBtnDisconnectSSH, WM_SETFONT, (WPARAM)hNppFont, TRUE);
//}
// 加载自定义图标（你可以替换为自己的图标 ID）
HICON NppSSHDockPanel::LoadCustomIcon(int iconId, int size)
{
    // 检查资源是否存在，避免加载失败返回NULL导致崩溃
    //HRSRC hRsrc = FindResource(s_hInst, MAKEINTRESOURCE(iconId), IMAGE_ICON);
    //if (!hRsrc)
    //{
    //    // 资源不存在时，返回系统默认图标（避免报错）
    //    return LoadIcon(NULL, IDI_APPLICATION);
    //}
         //::MessageBox(NULL, TEXT("NppSSH 连接!"), TEXT("NppSSH提示LoadCustomIcon"), MB_OK);
    //return (HICON)LoadImage(
    //    s_hInst,
    //    MAKEINTRESOURCE(iconId),
    //    IMAGE_ICON,
    //    size, size,
    //    LR_DEFAULTCOLOR | LR_LOADFROMFILE | LR_SHARED // 增加 LR_SHARED 减少资源占用
    //);
         // 直接从插件实例加载图标资源（RC 中已关联 IDC_BTN_CONNECT_SSH 和图标）
    // 调试弹框1：显示传入参数
    wchar_t paramMsg[256];
    swprintf_s(paramMsg, L"LoadCustomIcon参数：\n实例句柄=%p\n图标ID=%d\n图标尺寸=%d",
        s_hInst, iconId, size);
    ::MessageBoxW(NULL, paramMsg, L"NppSSH调试", MB_OK | MB_ICONINFORMATION);
    // 强制使用系统图标（跳过自定义图标，优先验证渲染逻辑）
    //if (iconId == IDI_ICON_CONNECT) {
    //    ::MessageBoxW(NULL, L"加载系统警告图标", L"NppSSH调试", MB_OK | MB_ICONWARNING);
    //    return LoadIcon(NULL, IDI_WARNING); // 黄色感叹号（高对比度）
    //}
    //else if (iconId == IDI_ICON_DISCONNECT) {
    //    ::MessageBoxW(NULL, L"加载系统错误图标", L"NppSSH调试", MB_OK | MB_ICONERROR);
    //    return LoadIcon(NULL, IDI_ERROR);   // 红色叉号（高对比度）
    //}
    
    
    // 校验基础参数
    if (s_hInst == NULL || iconId <= 0 || size <= 0) {
        ::MessageBoxW(NULL, L"图标加载参数无效", L"NppSSH错误", MB_OK | MB_ICONWARNING);
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
        ::MessageBoxW(NULL, errMsg, L"NppSSH错误", MB_OK | MB_ICONWARNING);
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    // 持久化到类成员，避免被系统回收
    if (iconId == IDI_ICON_CONNECT) {
        _hIconConnect = hIcon;
    }
    else if (iconId == IDI_ICON_DISCONNECT) {
        _hIconDisconnect = hIcon;
    }
    return hIcon;
    
}// 把按钮变成纯图标模式
void NppSSHDockPanel::SetButtonIconOnly(HWND btn, int iconId)
{
    if (btn == nullptr || !::IsWindow(btn))
    {
        ::MessageBoxW(NULL, L"按钮句柄无效", L"NppSSH错误", MB_OK | MB_ICONWARNING);
        return; // 窗口无效直接返回，避免崩溃
    }
    // 调试弹框3：显示按钮信息
    wchar_t btnMsg[256];
    swprintf_s(btnMsg, L"SetButtonIconOnly：\n按钮句柄=%p\n图标ID=%d", btn, iconId);
    ::MessageBoxW(NULL, btnMsg, L"NppSSH调试", MB_OK | MB_ICONINFORMATION);

    // 获取工具栏图标尺寸
    //int iconSize = 36;// 默认图标
    HICON hIcon = LoadCustomIcon(iconId, _iconSize);
    if (hIcon == NULL) {
        // 图标加载失败时用系统默认图标（避免报错）
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
        ::MessageBoxW(NULL, L"图标加载失败，使用默认图标", L"NppSSH提示", MB_OK | MB_ICONWARNING);
        return;
    }

    // 关键修复1：先移除所有原有样式，强制设置为纯图标
    ::SetWindowLongPtrW(btn, GWL_STYLE, WS_VISIBLE | WS_CHILD | BS_ICON | WS_BORDER);
    ::SetWindowPos(btn, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED); // 通知样式变更
    // 关键修复2：设置图标后，强制按钮持有句柄
    // 设置图标+按钮尺寸（与工具栏完全一致：图标尺寸+4px边距，匹配NPP工具栏按钮）
    int btnSize = _iconSize + 4;
    ::SendMessage(btn, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIcon);
    ::SetWindowPos(btn, NULL, 0, 0, btnSize, btnSize, SWP_NOMOVE | SWP_NOZORDER);

    // 关键修复3：双重刷新（确保样式和图标生效）
    ::InvalidateRect(btn, NULL, TRUE);
    ::UpdateWindow(btn);
    ::RedrawWindow(btn, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);

     // 调试：显示当前实际尺寸（含消息返回值）
    wchar_t sizeMsg[256];
    swprintf_s(sizeMsg, L"按钮最终尺寸：%dpx", _iconSize);
    ::MessageBoxW(NULL, sizeMsg, L"NppSSH尺寸同步", MB_OK | MB_ICONINFORMATION);
}
// 当用户修改 Npp 工具栏大小时自动更新
void NppSSHDockPanel::UpdateToolbarIconSize()
{
    ::MessageBoxW(NULL, L"触发工具栏尺寸更新", L"NppSSH调试", MB_OK | MB_ICONINFORMATION);
    if (_hBtnConnectSSH && IsWindow(_hBtnConnectSSH)) {
        SetButtonIconOnly(_hBtnConnectSSH, IDI_ICON_CONNECT);
    }
    if (_hBtnDisconnectSSH && IsWindow(_hBtnDisconnectSSH)) {
        SetButtonIconOnly(_hBtnDisconnectSSH, IDI_ICON_DISCONNECT);
    }
}
// 修改：创建顶部按钮栏（去掉文字，直接设为图标）
void NppSSHDockPanel::createTopButtonBar() {
    if (!_hSelf || !::IsWindow(_hSelf))
    {
        ::MessageBoxW(NULL, L"面板句柄无效，无法创建按钮", L"NppSSH错误", MB_OK | MB_ICONWARNING);
        return;
    }

    ::MessageBoxW(NULL, L"开始创建按钮栏", L"NppSSH调试", MB_OK | MB_ICONINFORMATION);


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


    // 关键：将按钮设为纯图标模式（对接自定义图标）
    if (_hBtnConnectSSH) {
        SetButtonIconOnly(_hBtnConnectSSH, IDI_ICON_CONNECT);
    }
    else {
        ::MessageBoxW(NULL, L"连接按钮创建失败", L"NppSSH错误", MB_OK | MB_ICONWARNING);
    }
    
    if (_hBtnDisconnectSSH) {
        SetButtonIconOnly(_hBtnDisconnectSSH, IDI_ICON_DISCONNECT);
        ::EnableWindow(_hBtnDisconnectSSH, FALSE);// 初始状态：断开按钮置灰
    }
    else {
        ::MessageBoxW(NULL, L"断开按钮创建失败", L"NppSSH错误", MB_OK | MB_ICONWARNING);
    }

    // 复用NPP字体（保持样式统一）
    //HFONT hNppFont = (HFONT)::SendMessage(s_nppData._nppHandle, NPPM_SETSMOOTHFONT, 0, 0);
    //if (hNppFont == NULL) hNppFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    //if (_hBtnConnectSSH) ::SendMessage(_hBtnConnectSSH, WM_SETFONT, (WPARAM)hNppFont, TRUE);
    //if (_hBtnDisconnectSSH) ::SendMessage(_hBtnDisconnectSSH, WM_SETFONT, (WPARAM)hNppFont, TRUE);
}

// 面板初始化：纯原生接口
void NppSSHDockPanel::initPanel() {
    // 调试弹框6：显示初始化开始
    ::MessageBoxW(NULL, L"面板初始化开始", L"NppSSH调试", MB_OK | MB_ICONINFORMATION);
    // 检查资源是否存在
    HRSRC hRes = ::FindResource(s_hInst, MAKEINTRESOURCE(IDD_SSH_PANEL), RT_DIALOG);
    if (hRes == NULL) {
        wchar_t errMsg[256] = { 0 };
        swprintf_s(errMsg, L"找不到IDD_SSH_PANEL资源！GetLastError: %d", ::GetLastError());
        ::MessageBoxW(NULL, errMsg, L"NppSSH资源错误", MB_OK | MB_ICONERROR);
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

    wchar_t msgBuf[512] = { 0 };        // 弹出 dockData 全部内容
    swprintf_s(msgBuf, 512,
        L"创建面板窗口准备 _dockData 的数据：\n\n"
        L"pszName: %s\n"
        L"uMask: %u\n"
        L"dlgID: %d\n"
        L"pszModuleName: %s\n"
        L"hIconTab: %p\n"
        L"pszAddInfo: %s\n"
        L"iPrevCont: %d",
        _dockData.pszName ? _dockData.pszName : L"NULL",
        _dockData.uMask,
        _dockData.dlgID,
        _dockData.pszModuleName ? _dockData.pszModuleName : L"NULL",
        _dockData.hIconTab,
        _dockData.pszAddInfo ? _dockData.pszAddInfo : L"NULL",
        _dockData.iPrevCont
    );
    ::MessageBoxW(NULL, msgBuf, L"NppSSH: _dockData", MB_OK);

    // 调用DockingDlgInterface原生create：绑定停靠数据，创建面板窗口
    //DockingDlgInterface::create(&_dockData);
    StaticDialog::create(_dlgID, false);
    _dockData.hClient = _hSelf;
    if (!_hSelf) {
        ::MessageBoxW(NULL, L"面板窗口创建失败！", L"NppSSH错误", MB_OK | MB_ICONERROR);
        return;
    }
    ::SendMessage(s_nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&_dockData));
    ::SendMessage(s_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD, reinterpret_cast<LPARAM>(_hSelf));

    // 调试弹框7：显示面板创建成功
    ::MessageBoxW(NULL, L"面板创建成功，开始创建按钮栏", L"NppSSH调试", MB_OK | MB_ICONINFORMATION);
    // ====== 新增：调用创建顶部按钮栏 ======
    createTopButtonBar();

    // 4. 从资源中获取EDIT控件句柄（不再手动CreateWindow）
    _hOutputEdit = ::GetDlgItem(_hSelf, IDC_OUTPUT_EDIT);
    if (_hOutputEdit) {
        ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\n等待SSH连接...");           // 设置初始文本 + 复用NPP字体
        HFONT hNppFont = (HFONT)::SendMessage(s_nppData._nppHandle, NPPM_SETSMOOTHFONT, 0, 0);
        if (hNppFont == NULL) hNppFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        ::SendMessage(_hOutputEdit, WM_SETFONT, (WPARAM)hNppFont, TRUE);
        ::SendMessage(_hOutputEdit, EM_GETLINECOUNT, 0, 0);     // 设置编辑框自适应换行/滚动
        ::SendMessage(_hOutputEdit, EM_SETTABSTOPS, 1, (LPARAM)8);
        // ====== 修改：调整输出编辑框位置，避开顶部按钮栏 ======
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
    // 5. 加入全局管理，支持标签切换和内存清理
    s_sshPanels.push_back(this);

    if (_hSelf && ::IsWindow(_hSelf)) {         // 强制设置面板窗口样式，解决遮挡/闪烁问题
        DWORD dwStyle = ::GetWindowLongPtrW(_hSelf, GWL_STYLE);
        ::SetWindowLongPtrW(_hSelf, GWL_STYLE, dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        ::SetWindowPos(_hSelf, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);     // 确保面板在停靠容器的顶层，不被覆盖
    }

    // 调试弹框8：面板初始化完成
    ::MessageBoxW(NULL, L"面板初始化完成", L"NppSSH调试", MB_OK | MB_ICONINFORMATION);
}

// 重写原生run_dlgProc：创建面板内UI，处理窗口消息（纯原生）
INT_PTR CALLBACK NppSSHDockPanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {      // 原生消息：面板窗口创建完成，初始化内部UI（输出文本框）
    case WM_INITDIALOG: {
        if (!_hSelf) {
            ::MessageBox(NULL, TEXT("面板窗口句柄无效！"), TEXT("NppSSH错误提示"), MB_OK | MB_ICONERROR);
            return FALSE;
        }
        // ================== 核心修改：纯图标按钮 ==================
        //SetButtonIconOnly(::GetDlgItem(_hSelf, IDC_BTN_CONNECT_SSH), IDC_BTN_CONNECT_SSH);
        //SetButtonIconOnly(::GetDlgItem(_hSelf, IDC_BTN_DISCONNECT_SSH), IDC_BTN_DISCONNECT_SSH);
        return TRUE;
    }
    // 面板大小变化时，自动适配输出文本框（防止遮挡/空白）（最小化关闭/打开notepad++会自动触发）
    case WM_SIZE: {
        if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
            RECT rc;
            ::GetClientRect(_hSelf, &rc);
            //::SetWindowPos(
            //    _hOutputEdit,
            //    NULL,
            //    10, 10, rc.right - 20, rc.bottom - 20,
            //    SWP_NOZORDER | SWP_NOACTIVATE
            //);
            // ====== 同步修改WM_SIZE中的编辑框位置，适配按钮栏 ======
            ::SetWindowPos(
                _hOutputEdit,
                NULL,
                5, _iconSize + 12, rc.right - 10, rc.bottom - 50,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
        }
        //// 新增：面板大小变化时，按钮栏宽度自适应（可选，保持按钮左对齐即可）
        //if (_hBtnConnectSSH && ::IsWindow(_hBtnConnectSSH)) {
        //    ::SetWindowPos(_hBtnConnectSSH, NULL, 10, 10, 32, 32, SWP_NOZORDER | SWP_NOACTIVATE);
        //}
        //// 新增：更新断开按钮位置
        //if (_hBtnDisconnectSSH && ::IsWindow(_hBtnDisconnectSSH)) {
        //    ::SetWindowPos(_hBtnDisconnectSSH, NULL, 52, 10, 32, 32, SWP_NOZORDER | SWP_NOACTIVATE);
        //}
        // 关键修复：移除硬编码的32px，改为动态适配（调用UpdateToolbarIconSize同步尺寸）
        //if (_hBtnConnectSSH || _hBtnDisconnectSSH) {
        //    UpdateToolbarIconSize();
        //}
        return TRUE;
    }
    // ====== 新增：处理按钮点击消息 WM_COMMAND ======
    case WM_COMMAND: {
        // 判断是否为连接SSH按钮点击
        if (LOWORD(wParam) == IDC_BTN_CONNECT_SSH) {
            // 模拟连接成功（实际需调用NppSSH_Connect）
            setSSHConnected(true);
            // 弹出提示框，验证按钮可用，后续替换为实际SSH连接逻辑
            ::MessageBoxW(_hSelf, L"连接SSH", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
            if (_hOutputEdit) {
                ::SetWindowTextW(_hOutputEdit, L"✅ SSH连接成功！");
            }
            //::onNppSSH();
        }
        // 新增：处理断开SSH按钮点击
        else if (LOWORD(wParam) == IDC_BTN_DISCONNECT_SSH) {
            disconnectSSH();
            if (_hOutputEdit) {
                ::SetWindowTextW(_hOutputEdit, L"✅ SSH已断开\n等待新的连接...");
            }
            ::MessageBoxW(_hSelf, L"SSH连接已断开", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
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
        // 检查当前面板是否有活跃SSH连接
        if (this->isSSHConnected()) {
            ::MessageBoxW(
                NULL,
                L"当前面板存在SSH活跃连接，关闭将断开连接并恢复初始状态！",
                L"NppSSH 连接提示",
                MB_OK | MB_ICONWARNING
            );
            this->resetPanelToInit();   // 重置为初始状态
        }
        // 手动关闭面板：自动断开当前SSH，无提示
        //disconnectSSH();
        // 从NPP原生停靠管理器移除面板
        ::SendMessage(s_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, (LPARAM)getHSelf());
        ::SendMessage(s_nppData._nppHandle, NPPM_DMMHIDE, 0, (LPARAM)getHSelf());

        auto it = std::find(s_sshPanels.begin(), s_sshPanels.end(), this);      // 从全局管理数组中移除
        if (it != s_sshPanels.end()) s_sshPanels.erase(it);

        // 原生停靠面板清理：先隐藏，再销毁
        this->display(false);
        ::DestroyWindow(_hSelf);
        this->destroy();
        delete this;                                // 释放面板实例
        //SSHPanel_SavePanelCountToReg(s_sshPanels.size());    // 同步更新注册表面板数量
        SSHPanel_SavePanelCountToIni(s_sshPanels.size());    // 同步更新INI面板数量
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
    if (s_nppData._nppHandle == NULL || s_hInst == NULL) return;
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