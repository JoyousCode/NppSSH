// SSHPanel.cpp（面板 + 注册表具体实现）
#include "SSHPanel.h"
#include "SSHSettings.h" // 引入INI工具

// 面板相关全局变量实际定义
static int s_panelId;
static std::vector<NppSSHDockPanel*> s_sshPanels;
static std::atomic<int> s_panelCounter = 0;
static NppData s_nppData;
static HINSTANCE s_hInst;
static int s_iconSize;
static bool initPanle;//防止未初始化完成就调用面板

static NppSSHDockPanel* pPanel = nullptr;
// 标记是否正在连接，避免重复操作
static std::atomic<bool> s_isConnecting = false;
//static std::atomic<bool> s_isPanelChangingConnection = false;

HWND SSHPanel_getLoginPanel() {
    return pPanel->getLoginPanel();
}

// 全局变量获取接口
std::vector<NppSSHDockPanel*>& SSHPanel_GetGlobalPanels() {
    return s_sshPanels;
}

std::atomic<int>& SSHPanel_GetGlobalPanelCounter() {
    return s_panelCounter;
}

NppData& SSHPanel_GetGlobalNppData() {
    return s_nppData;
}

HINSTANCE& SSHPanel_GetGlobalHInst() {
    return s_hInst;
}
int& SSH_GetPanelId() { return s_panelId; }//获取点击连接图标面板索引
int& SSHPanel_iconSize() { return s_iconSize; }//获取点击连接图标面板索引

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
// 编码转换工具（自动识别 UTF8 / GBK，彻底解决Windows弹框乱码）
inline std::wstring GBKToWstring(const std::string& str) {
    if (str.empty())
        return L"";

    wchar_t buf[1024] = { 0 };

    // 1. 优先按 UTF-8 转换（libssh2 错误信息都是 UTF-8）
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len > 0 && len < 1024) {
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf, len);
        return buf;
    }

    // 2. 失败则使用 GBK（系统本地编码）
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buf, _countof(buf));
    return buf;
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
    _hIconDisconnect(NULL),
    _hTabIcon(NULL) {
    ZeroMemory(_titleBuf, sizeof(_titleBuf));
}
// 析构函数：释放图标资源，防止内存泄漏
NppSSHDockPanel::~NppSSHDockPanel() {
    if (_hIconConnect) ::DestroyIcon(_hIconConnect);
    if (_hIconDisconnect) ::DestroyIcon(_hIconDisconnect);
    if (_hTabIcon)  ::DestroyIcon(_hTabIcon);
}
// 判断SSH是否连接
bool NppSSHDockPanel::isSSHConnected() const {
    return _isSSHConnected;
}

// 设置SSH是否连接
void NppSSHDockPanel::setSSHConnected(bool state) {
    // 加锁：防止快速断开/重连造成流程混乱、文本被覆盖
    //if (s_isPanelChangingConnection)
    //    return;
    //s_isPanelChangingConnection = true;
    _isSSHConnected = state;
    // 连接状态变化时更新按钮图标状态
    if (_hBtnConnectSSH) ::EnableWindow(_hBtnConnectSSH, !state);
    if (_hBtnDisconnectSSH) ::EnableWindow(_hBtnDisconnectSSH, state);

    // 同步更新输出框状态提示
    if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
        if (state) {
            OnSSHConnected(this->_panelId);         //调用转发SSH连接设置当前面板连接资源
            //TODO 待优化连接成功后的命令执行
            //::SetWindowTextW(_hOutputEdit, L"输出框状态提示SSH连接成功！/r/n可执行SSH命令...");
            SYSTEMTIME st;
            GetLocalTime(&st);
            char currentTime[128];
            sprintf_s(currentTime, "当前登录: %04d-%02d-%02d %02d:%02d:%02d from %s\r\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                host);
            g_loginBanner += currentTime;

            g_loginBanner += "[" + std::string(user) + "@" + std::string(host) + " ~]# ";
            //::SetWindowTextW(_hOutputEdit, GBKToWstring(g_loginBanner).c_str());
            SSH_AppendOutputText(this->_panelId, g_loginBanner);
            //if (_hOutputEdit) {
            //    // 保持整体只读，但允许在末尾输入
            //    ::SendMessage(_hOutputEdit, EM_SETREADONLY, FALSE, 0);
            //    // 强制光标跳到可输入区域
            //    ForceCursorToEditableEnd();
            //    // 自动聚焦到编辑框，用户可直接打字
            //    ::SetFocus(_hOutputEdit);
            //}
            // 清空 banner，防止下一次复用脏数据
            g_loginBanner.clear();
        }
        else {

            DisconnectPanel(this->_panelId);        //调用转发断开连接释放当前面板连接资源
            ::SetWindowTextW(_hOutputEdit, L"🔌 SSH已断开\n等待新的连接...");
        }
        NppSSH_LogInfoAuto("setSSHConnected==========PanelID======" + std::to_string(this->_panelId));
        //MessageBoxW(s_nppData._nppHandle, (L"当前面板ID==" + std::to_wstring(this->_panelId)).c_str(), L"NppSSH", MB_OK | MB_TASKMODAL);
        //自动滚动到底部
        DWORD len = ::GetWindowTextLengthW(_hOutputEdit);
        ::SendMessageW(_hOutputEdit, EM_SETSEL, len, len);
        ::SendMessageW(_hOutputEdit, EM_SCROLLCARET, 0, 0);
    }
    // 解锁
    //s_isPanelChangingConnection = false;
}

// 断开当前面板的SSH连接（无提示）
void NppSSHDockPanel::disconnectSSH() {
    if (_isSSHConnected) {      // 调用SSHConnection的断开逻辑
        //NppSSH_Disconnect();    // 调用转发断开连接释放资源
        //DisconnectPanel(this->_panelId);//通过面板ID断开连接
        setSSHConnected(false); // 统一通过set方法更新状态
    }
}

void NppSSHDockPanel::resetPanelToInit() {//关闭面板进行销毁时调用
    disconnectSSH();
    if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
        ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\r\n等待SSH连接...resetPanelToInit");
        ::SetWindowTextW(_hOutputEdit, L"🔌 SSH已断开\r\n等待新的连接...resetPanelToInit");
    }
    // 重置面板时，启用连接SSH按钮（若之前置灰）
    if (_hBtnConnectSSH) ::EnableWindow(_hBtnConnectSSH, TRUE);
    if (_hBtnDisconnectSSH) ::EnableWindow(_hBtnDisconnectSSH, FALSE);

    
    if (_hOutputEdit) {
        ::SendMessage(_hOutputEdit, EM_SETREADONLY, TRUE, 0);
    }
    NppSSH_LogInfoAuto("面板已重置，提示符状态清空");
}

// 加载自定义图标（可以替换为自己的图标 ID）
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
        size, size,               // 图标大小
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

    // 获取工具栏图标尺寸
    HICON hIcon = LoadCustomIcon(iconId, _iconSize);//_iconSize=24
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
        // 读取当前是否启用（TRUE=正常，FALSE=灰色）
        BOOL isEnabled = IsWindowEnabled(_hBtnConnectSSH);
        SetButtonIconOnly(_hBtnConnectSSH, IDI_ICON_CONNECT);
        // 恢复原来的状态
        EnableWindow(_hBtnConnectSSH, isEnabled);
    }
    if (_hBtnDisconnectSSH && IsWindow(_hBtnDisconnectSSH)) {
        // 读取当前是否启用（TRUE=正常，FALSE=灰色）
        BOOL isEnabled = IsWindowEnabled(_hBtnDisconnectSSH);
        SetButtonIconOnly(_hBtnDisconnectSSH, IDI_ICON_DISCONNECT);
        // 恢复原来的状态
        EnableWindow(_hBtnDisconnectSSH, isEnabled);
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

    // 创建「连接」按钮（无文字）
    _hBtnConnectSSH = ::CreateWindowW(
        L"BUTTON",
        L"", // 文字设为空
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER,
        btnMargin,
        btnTop,
        btnInitSize, btnInitSize, // 初始大小
        _hSelf,
        (HMENU)IDC_BTN_CONNECT_SSH,
        s_hInst, // 用全局插件实例句柄
        NULL
    );

    // 创建「断开」按钮（无文字）
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
    if(initPanle) initPanle = false;//标记正在初始化
    if (s_hInst == NULL || s_nppData._nppHandle == NULL) {
        ::MessageBoxW(s_nppData._nppHandle, L"NPP插件环境未初始化！", L"NppSSH资源错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 检查资源是否存在
    HRSRC hRes = ::FindResource(s_hInst, MAKEINTRESOURCE(IDD_SSH_PANEL), RT_DIALOG);
    if (hRes == NULL) {
        wchar_t errMsg[256] = { 0 };
        swprintf_s(errMsg, L"找不到IDD_SSH_PANEL资源！GetLastError: %d", ::GetLastError());
        ::MessageBoxW(s_nppData._nppHandle, errMsg, L"NppSSH资源错误", MB_OK | MB_ICONERROR);
        return;
    }

    DockingDlgInterface::init(s_hInst, s_nppData._nppHandle);   // 调用DockingDlgInterface原生init：绑定NPP实例和父窗口
    ZeroMemory(&_dockData, sizeof(tTbData));                    // 初始化原生tTbData结构体（完全按Docking.h定义，无多余成员）

    // 面板标签名（多标签区分：NppSSH-1、NppSSH-2...，NPP底部标签栏显示）
    std::wstring panelTitle = L"NppSSH-" + std::to_wstring(_panelId);
    wcscpy_s(_titleBuf, _countof(_titleBuf), panelTitle.c_str());

    _hTabIcon = (HICON)::LoadImage(
        s_hInst,
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
    //DockingDlgInterface::create(&_dockData);
    //StaticDialog::create(IDD_SSH_PANEL);


    _dockData.hClient = _hSelf;
    if (!_hSelf) {
        ::MessageBoxW(s_nppData._nppHandle, L"面板窗口创建失败！", L"NppSSH错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 注册面板到NPP停靠管理器
    ::SendMessage(s_nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&_dockData));
    ::SendMessage(s_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD, reinterpret_cast<LPARAM>(_hSelf));

    createTopButtonBar();               // 调用创建顶部按钮栏

    //  从资源中获取EDIT控件句柄（不再手动CreateWindow）
    //_hOutputEdit = ::GetDlgItem(_hSelf, IDC_OUTPUT_EDIT);
    s_iconSize = _iconSize;
    _hOutputEdit = SSH_InitTerminalEditBox(_hSelf);
    if (!_hOutputEdit) {
        ::MessageBoxW(s_nppData._nppHandle, L"NPP插件环境_hOutputEdit初始化失败！", L"NppSSH调试提示", MB_OK);
    }
    
    
    if (_hSelf && ::IsWindow(_hSelf)) {         // 强制设置面板窗口样式，解决遮挡/闪烁问题
        DWORD dwStyle = ::GetWindowLongPtrW(_hSelf, GWL_STYLE);
        SetWindowLongPtrW(_hSelf, GWL_STYLE, dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        //::SetWindowPos(_hSelf, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);     // 确保面板在停靠容器的顶层，不被覆盖
    }
    // 加入全局管理，支持标签切换和内存清理
    s_sshPanels.push_back(this);
    // 13. 日志记录（调试/排查）
    NppSSH_LogInfoAuto("面板初始化完成 [ID: " + std::to_string(_panelId) + "]");
    if (!initPanle) initPanle = true;//面板初始化完成
}
/////////////////////////////////////////开始处理登录对话框//////////////////////////
// 窗口居中工具函数
// hWndChild: 要居中的窗口
// hWndParent: 父窗口（NPP主窗口）
void CenterWindow(HWND hWndChild, HWND hWndParent)
{
    if (!hWndChild || !hWndParent) return;

    RECT rcChild, rcParent;
    GetWindowRect(hWndChild, &rcChild);
    GetWindowRect(hWndParent, &rcParent);

    int cx = (rcParent.right - rcParent.left) - (rcChild.right - rcChild.left);
    int cy = (rcParent.bottom - rcParent.top) - (rcChild.bottom - rcChild.top);

    SetWindowPos(
        hWndChild,
        NULL,
        rcParent.left + cx / 2,
        rcParent.top + cy / 2,
        0, 0,
        SWP_NOSIZE | SWP_NOZORDER
    );
}

// 官方标准模态登录窗口（修复关闭后NPP被置底）
void NppSSHDockPanel::ShowSSHLoginWindow_Modal()
{
    if (s_nppData._nppHandle == NULL || s_hInst == NULL) {
        ::MessageBoxW(s_nppData._nppHandle, L"NPP环境未初始化", L"NppSSH", MB_ICONERROR);
        return;
    }

    // 官方SDK标准：DialogBoxParam 模态对话框
    // 父窗口固定为 NPP 主窗口，自动管理Z序、激活状态、禁用/恢复
    DialogBoxParamW(
        s_hInst,
        MAKEINTRESOURCE(IDD_SSH_LOGIN),
        s_nppData._nppHandle,  // 关键：父窗口是NPP主窗口
        SSH_LoginDlgProc,
        (LPARAM)this
    );
}

// 官方标准对话框过程
INT_PTR CALLBACK NppSSHDockPanel::SSH_LoginDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        pPanel = (NppSSHDockPanel*)lParam;
        // 居中在 Notepad++ 主窗口
        CenterWindow(hWnd, s_nppData._nppHandle);
        SetForegroundWindow(hWnd);

        // 初始化默认值
        SetDlgItemTextA(hWnd, IDC_HOST, "192.168.137.201");
        SetDlgItemTextA(hWnd, IDC_PORT, "22");
        SetDlgItemTextA(hWnd, IDC_USER, "root");
        SetDlgItemTextA(hWnd, IDC_PASS, "123456");

        // 密码框样式
        SendDlgItemMessage(hWnd, IDC_PASS, EM_SETPASSWORDCHAR, L'•', 0);
        s_isConnecting = false; // 初始化连接状态
        pPanel->setLoginPanel(hWnd);
        return TRUE;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDCANCEL)////取消连接，无论面板什么状态直接断开
        {
            // 取消连接时重置状态
            if (s_isConnecting) {
                NppSSH_Disconnect();
                s_isConnecting = false;
                NppSSH_LogInfoAuto("用户取消连接，已断开SSH");
            }
            EndDialog(hWnd, IDCANCEL); // 右上角关闭
            pPanel->setSSHConnected(false);//断开连接
        }
        else {
            if (LOWORD(wParam) == IDC_BTN_CONNECT || LOWORD(wParam) == IDC_BTN_TEST)//连接按钮
            {
                char host[256] = { 0 };
                char port[32] = { 0 };
                char user[256] = { 0 };
                char pass[256] = { 0 };

                GetDlgItemTextA(hWnd, IDC_HOST, host, 256);
                GetDlgItemTextA(hWnd, IDC_PORT, port, 32);
                GetDlgItemTextA(hWnd, IDC_USER, user, 256);
                GetDlgItemTextA(hWnd, IDC_PASS, pass, 256);
                if (s_isConnecting) {
                    MessageBoxW(hWnd, L"正在连接中，请等待...", L"NppSSH 提示", MB_OK | MB_ICONINFORMATION);
                    NppSSH_LogInfoAuto("用户重复点击连接按钮，忽略");
                    return TRUE;
                }
                s_isConnecting = true;
                NppSSH_LogInfoAuto("用户点击连接按钮，开始调用SSHConnection_Connect");

                bool ok = NppSSH_Connect(host, atoi(port), user, pass);
                s_isConnecting = false;// 异步连接立即重置，避免卡死
                if (ok) {
                    NppSSH_LogInfoAuto("SSH连接请求已发送，等待异步结果");
                    MessageBoxW(hWnd, L"SSH 连接成功 ✅", L"NppSSH", MB_OK | MB_TASKMODAL);
                    if (pPanel && LOWORD(wParam) == IDC_BTN_CONNECT) {
                        pPanel->setSSHConnected(true);//更新面板显示效果，绑定面板ID和session
                    }
                    
                    
                    // 已经由setSSHConnected(true);处理
                    //NppSSH_LogInfoAuto("连接成功PanelID======" + std::to_string(pPanel->_panelId));
                    //MessageBoxW(s_nppData._nppHandle, std::to_wstring(pPanel->_panelId).c_str(), L"NppSSH", MB_OK | MB_TASKMODAL);
                    //OnSSHConnected(pPanel->_panelId); // 面板连接成功后，通知窗口绑定索引
                    //DisconnectPanel(pPanel->_panelId);// 面板断开SSH函数
                    if (LOWORD(wParam) == IDC_BTN_TEST) {
                        //无论成功还是失败都断开连接，防止占用远程资源
                        NppSSH_Disconnect();
                        //DisconnectPanel(pPanel->_panelId);        //调用转发断开连接释放当前面板连接资源
                        
                    }
                    else {
                        EndDialog(hWnd, IDOK); // 官方标准关闭
                    }
                }else {
                    s_isConnecting = false;
                    MessageBoxW(hWnd, L"SSH 连接失败 ❌", L"NppSSH", MB_ICONERROR);
                    NppSSH_LogErrorAuto("SSH连接请求发送失败");
                }
                
            }
        }
        return TRUE;
    }

    case WM_DESTROY:
        pPanel = nullptr;
        NppSSH_LogInfoAuto("登录对话框销毁");
        return TRUE;
    }

    return FALSE;
}

/*
* 面板处理开始
*/
// 重写原生run_dlgProc：创建面板内UI，处理窗口消息（纯原生）
INT_PTR CALLBACK NppSSHDockPanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {      // 原生消息：面板窗口创建完成，初始化内部UI（输出文本框）
    case WM_INITDIALOG: 
    {
        if (!_hSelf) {
            NppSSH_LogErrorAuto("面板窗口句柄无效！");
            ::MessageBox(NULL, TEXT("面板窗口句柄无效！"), TEXT("NppSSH错误提示"), MB_OK | MB_ICONERROR);
            return FALSE;
        }
        return TRUE;
    }
    // 面板大小变化时，自动适配输出文本框（防止遮挡/空白）（最小化关闭/打开notepad++会自动触发）
    case WM_SIZE: 
    {
        if (initPanle) {
            //::MessageBoxW(s_nppData._nppHandle, L"SSH面板变化", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
            SSH_SizeSSHTerminal(_hSelf, this->_panelId);
        }
        return TRUE;
    }
    // 处理按钮点击消息
    case WM_COMMAND: 
    {
        UINT cmd = LOWORD(wParam);
        HWND hCtrl = (HWND)lParam;
        if (cmd == IDC_BTN_CONNECT_SSH) {
            NppSSH_LogInfoAuto("用户点击面板连接按钮，显示登录对话框");
            s_panelId = this->_panelId;     //点击连接存储当前面板ID，方便读取
            ShowSSHLoginWindow_Modal();
        }
        else if (cmd == IDC_BTN_DISCONNECT_SSH) {
            NppSSH_LogInfoAuto("用户点击面板断开按钮"+ std::to_string(this->_panelId));
            disconnectSSH(); // 断开连接
            if (_hOutputEdit) {

                MessageBoxW(s_nppData._nppHandle, std::to_wstring(this -> _panelId).c_str(), L"NppSSH", MB_OK | MB_TASKMODAL);
                //DisconnectPanel(this -> _panelId);// 面板断开SSH函数
                ::SetWindowTextW(_hOutputEdit, L"✅ SSH已断开\n等待新的连接...");
            }
            ::MessageBoxW(s_nppData._nppHandle, L"SSH连接已断开", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
        }
        // 新增：拦截输出编辑框的所有操作
        if (hCtrl == _hOutputEdit) {
            UINT notifyCode = HIWORD(wParam);
            // 拦截光标移动、文本修改、粘贴等操作

            switch (notifyCode) {
            case EN_SETFOCUS: // 编辑框获焦时，强制光标到可编辑区域
                ::SendMessage(_hOutputEdit, EM_SETREADONLY, FALSE, 0);
                break;
            case EN_CHANGE: // 文本变化时，检查是否在合法区域
                DWORD totalLen = ::GetWindowTextLengthW(_hOutputEdit);
                break;
            }
            return TRUE;
        }
    }
    // 新增：拦截键盘消息（仅允许在合法区域输入）
    case WM_KEYDOWN : {
        // 仅处理输出编辑框的按键
        if (GetFocus() == _hOutputEdit) {
            DWORD selStart, selEnd;
            ::SendMessageW(_hOutputEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

            // 处理回车（执行命令）
            if (wParam == VK_RETURN) {
                NppSSH_LogInfoAuto("endendend========");
                // 1. 获取合法区域的文本（命令部分）
               
                return TRUE; // 拦截回车，不传递
            }
            // 处理删除键（仅允许删除合法区域的字符）
            if (wParam == VK_BACK || wParam == VK_DELETE) {
                DWORD selStart, selEnd;
                ::SendMessageW(_hOutputEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            }
        }
        break;
    }
    // 响应NPP停靠管理器的浮动/停靠消息，更新面板状态
    case WM_NOTIFY: 
    {
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
    case WM_CLOSE: 
    {
        NppSSH_LogInfoAuto("面板开始关闭，当前连接状态：" + std::to_string(_isSSHConnected));
        ::MessageBoxW(s_nppData._nppHandle, L"关闭面板", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);

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
    case NPPN_TOOLBARICONSETCHANGED: 
    {
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
    int panelCount = SSHPanel_LoadPanelCountFromIni(); // 从INI加载
    if (panelCount <= 0) return;
    // 按注册表记录的数量重建面板，ID延续自注册表
    for (int i = 1; i <= panelCount; i++) {
        s_panelCounter = i;// 同步计数器，保证新创建面板ID不重复
        NppSSHDockPanel* pNewPanel = new NppSSHDockPanel(i);
        if (pNewPanel) {
            pNewPanel->initPanel();
            ::SendMessage(s_nppData._nppHandle, NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(pNewPanel->getHSelf()));
            // 额外触发标签栏重绘（兜底）
            //::RedrawWindow(s_nppData._nppHandle, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
        }
    }
}

