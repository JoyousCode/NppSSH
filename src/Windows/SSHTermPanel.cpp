// SSHPanel.cpp（面板 + 注册表具体实现）
#include "SSHPanel.h"
#include "SSHSettings.h" // 引入INI工具
#include <CommCtrl.h>
static NppData s_nppData;
static HINSTANCE s_hInst;
static int s_iconSize;
static bool initPanle;//防止未初始化完成就调用面板

//static SSHPanel* pPanel = nullptr;
// 标记是否正在连接，避免重复操作
static std::atomic<bool> s_isConnecting = false;

NppData& SSHPanel_GetGlobalNppData() {
    return s_nppData;
}

HINSTANCE& SSHPanel_GetGlobalHInst() {
    return s_hInst;
}
int& SSHPanel_iconSize() { return s_iconSize; }//获取点击连接图标面板索引

// 面板类构造函数
SSHPanel::SSHPanel(int panelSeqId, int panelrealId)
    :
    SSHBasePanel(panelSeqId, panelrealId),
    _hOutputEdit(NULL),
    _hBtnConnectSSH(NULL),
    _hBtnDisconnectSSH(NULL),
    _hIconConnect(NULL) ,
    _hIconDisconnect(NULL) {
    //ZeroMemory(_titleBuf, sizeof(_titleBuf));
}

// 析构函数：释放图标资源，防止内存泄漏
SSHPanel::~SSHPanel() {
    if (_hIconConnect) ::DestroyIcon(_hIconConnect);
    if (_hIconDisconnect) ::DestroyIcon(_hIconDisconnect);
    if (_hTabIcon)  ::DestroyIcon(_hTabIcon);
}
// 判断SSH是否连接
//bool SSHPanel::isSSHConnected() const {
//    return _isSSHConnected;
//}

// 设置SSH是否连接
void SSHPanel::setSSHConnected(bool state) {
    // 加锁：防止快速断开/重连造成流程混乱、文本被覆盖
    //if (s_isPanelChangingConnection)
    //    return;
    //s_isPanelChangingConnection = true;
    _isConnected = state;
    // 连接状态变化时更新按钮图标状态
    if (_hBtnConnectSSH) ::EnableWindow(_hBtnConnectSSH, !state);
    if (_hBtnDisconnectSSH) ::EnableWindow(_hBtnDisconnectSSH, state);

    // 同步更新输出框状态提示
    if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
        if (state) {
            //OnSSHConnected(this->_panelId);         //调用转发SSH连接设置当前面板连接资源
            //TODO 待优化连接成功后的命令执行
            //::SetWindowTextW(_hOutputEdit, L"输出框状态提示SSH连接成功！/r/n可执行SSH命令...");
            //SYSTEMTIME st;
            //GetLocalTime(&st);
            //char currentTime[128];
            //sprintf_s(currentTime, "当前登录: %04d-%02d-%02d %02d:%02d:%02d from %s\r\n",
            //    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
            //    host);
            //g_loginBanner += currentTime;

            //std::string appendPrompt = NppSSH_PanelPrompt(pPanel->_panelId);
            //SSH_PanelPrompt(pPanel->_panelId, appendPrompt);
            //SSH_AppendOutputText(this->_panelId, g_loginBanner, true);
            //// 清空 banner，防止下一次复用脏数据
            //g_loginBanner.clear();
        }
        else {
            NppSSH_LogInfoAuto("NppSSH_Disconnect===面板唯一索引=" + std::to_string(this->_panelSeqId));
            SSH_ConnectionOnDisconn(this->_panelSeqId);        //调用转发断开连接释放当前面板连接资源
            
        }
        NppSSH_LogInfoAuto("setSSHConnected==========面板唯一索引======" + std::to_string(this->_panelSeqId));
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
void SSHPanel::disconnectSSH() {//_isSSHConnected= true表示登录成功
    if (_isConnected) {      // 调用SSHConnection的断开逻辑
        //NppSSH_Disconnect();    // 调用转发断开连接释放资源
        //DisconnectPanel(this->_panelId);//通过面板ID断开连接
        setSSHConnected(false); // 统一通过set方法更新状态
    }
}

void SSHPanel::resetPanelToInit() {//关闭面板进行销毁时调用
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
HICON SSHPanel::LoadCustomIcon(int iconId, int size)
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
void SSHPanel::SetButtonIconOnly(HWND btn, int iconId)
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
void SSHPanel::UpdateToolbarIconSize()
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
void SSHPanel::createTopButtonBar() {
    if (!GetHwndSelf() || !::IsWindow(GetHwndSelf()))
    {
        ::MessageBoxW(s_nppData._nppHandle, L"面板句柄无效，无法创建按钮", L"NppSSH错误", MB_OK | MB_ICONWARNING);
        return;
    }

    RECT rcClient;
    ::GetClientRect(GetHwndSelf(), &rcClient);
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
        GetHwndSelf(),
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
        GetHwndSelf(),
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
}

// 面板初始化：纯原生接口
void SSHPanel::initPanel() {
    if(initPanle) initPanle = false;//标记正在初始化
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
    std::wstring panelTitle = L"NppSSH-" + std::to_wstring(_panelrealId);
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
    //wchar_t* pFlag = new wchar_t[32];
    //wcscpy_s(pFlag, 32, L"NO_CLOSE_BUTTON");
    //_dockData.pszAddInfo = pFlag;
    
    // 调用DockingDlgInterface原生create：绑定停靠数据，创建面板窗口
    StaticDialog::create(_dlgID, false);

    //DockingDlgInterface::create(&_dockData);
    //StaticDialog::create(IDD_SSH_PANEL);//固定面板，适合单一 SSH 面板

    DWORD dwStyle = ::GetWindowLongPtrW(GetHwndSelf(), GWL_STYLE);
    SetWindowLongPtrW(GetHwndSelf(), GWL_STYLE, dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_SYSMENU);
    


    _dockData.hClient = GetHwndSelf();
    if (!GetHwndSelf()) {
        ::MessageBoxW(s_nppData._nppHandle, L"面板窗口创建失败！", L"NppSSH错误", MB_OK | MB_ICONERROR);
        return;
    }
    _panelHwnd = GetHwndSelf();
    // 注册面板到NPP停靠管理器
    ::SendMessage(s_nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&_dockData));
    ::SendMessage(s_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD, reinterpret_cast<LPARAM>(GetHwndSelf()));

    createTopButtonBar();               // 调用创建顶部按钮栏

    s_iconSize = _iconSize;
    // TODO：出现BUG，序列不是按照顺序的，需要将伪终端面板封装到面板类中
    //_hOutputEdit = SSH_TerminalInitControlPanel(GetHwndSelf(), _panelSeqId);
    //if (!_hOutputEdit) {
    //    ::MessageBoxW(s_nppData._nppHandle, L"NPP插件环境_hOutputEdit初始化失败！", L"NppSSH调试提示", MB_OK);
    //}
    //SSH_TerminalAppendTextHandle(_panelSeqId, "✅NppSSH面板已创建\r\n等待SSH连接...");
    
    //if (GetHwndSelf() && ::IsWindow(GetHwndSelf())) {         // 强制设置面板窗口样式，解决遮挡/闪烁问题
    //    DWORD dwStyle = ::GetWindowLongPtrW(GetHwndSelf(), GWL_STYLE);
    //    SetWindowLongPtrW(GetHwndSelf(), GWL_STYLE, dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    //    //::SetWindowPos(GetHwndSelf(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);     // 确保面板在停靠容器的顶层，不被覆盖
    //}
    char bufSelf[64] = { 0 };
    sprintf(bufSelf, "_panelHwnd(GetHwndSelf())=0x%p", _panelHwnd);
    NppSSH_LogInfoAuto(bufSelf);

    // ==== 挂载子类化 ====
    if (::IsWindow(_panelHwnd)) {
        _hTopParent = _panelHwnd;
        while (true)
        {
            HWND hTmpParent = ::GetParent(_hTopParent);
            if (hTmpParent == nullptr)
                break;
            _hTopParent = hTmpParent;
        }
        // 1. 获取原窗口过程
        // 防重复子类化：判断当前WndProc是否已经是自定义过程
        WNDPROC curProc = (WNDPROC)GetWindowLongPtrW(_hTopParent, GWLP_WNDPROC);
        if (curProc == SSHPanel::PanelSubclassWndProc)
        {
            NppSSH_LogInfoAuto("面板已完成子类化，跳过");
        }else {
            // 获取原生窗口过程，不再存入GWLP_USERDATA
            _oldPanelWndProc = (WNDPROC)::GetWindowLongPtrW(_hTopParent, GWLP_WNDPROC);
            //if (!_oldPanelWndProc)_oldPanelWndProc = DefWindowProcW;

            // 绑定当前面板实例到窗口属性
            swprintf_s(_titleParentBuf, _countof(_titleParentBuf), L"SSHPanel-%p", _hTopParent);
            ::SetPropW(_hTopParent, _titleParentBuf, (HANDLE)this);

            // 设置新窗口过程
            ::SetWindowLongPtrW(_hTopParent, GWLP_WNDPROC, (LONG_PTR)PanelSubclassWndProc);
        }
        NppSSH_LogInfoAuto("Notepad++软件子类化完成！hWnd=" + PtrToHexStr(_hTopParent)
            + " 原过程：" + PtrToHexStr(_oldPanelWndProc)
            + " 新过程：" + PtrToHexStr(PanelSubclassWndProc));
    }
    // 13. 日志记录（调试/排查）
    NppSSH_LogInfoAuto("面板初始化完成 [序列ID: " + std::to_string(_panelSeqId) + "]");
    NppSSH_LogInfoAuto("面板初始化完成 [标题ID: " + std::to_string(_panelrealId) + "]");
    if (!initPanle) initPanle = true;//面板初始化完成
}
/////////////////////////////////////////开始处理登录对话框/////////////////////////
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
void SSHPanel::ShowSSHLoginWindow_Modal()
{
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
//static thread_local bool s_LoginProcessingMsg = false;
// 官方标准对话框过程
INT_PTR CALLBACK SSHPanel::SSH_LoginDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SSHPanel* pPanel = nullptr;
    // 统一从窗口属性读取面板指针，替代每次遍历全局vector
    wchar_t buf[128]{};
    swprintf_s(buf, _countof(buf), L"SSHLoginDlg-%p", hWnd);
    pPanel = (SSHPanel*)GetPropW(hWnd, buf);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // lParam是DialogBoxParam传入的this，存入窗口属性
        pPanel = (SSHPanel*)lParam;
        wchar_t buf[128]{};
        swprintf_s(buf, _countof(buf), L"SSHLoginDlg-%p", hWnd);
        SetPropW(hWnd, buf, (HANDLE)pPanel);
        if (pPanel != nullptr) { pPanel->setLoginPanel(hWnd); }
        
        // 居中在 Notepad++ 主窗口
        CenterWindow(hWnd, s_nppData._nppHandle);
        SetForegroundWindow(hWnd);

        // 初始化默认值
        SetDlgItemTextA(hWnd, IDC_HOST, "192.168.137.201");
        SetDlgItemTextA(hWnd, IDC_PORT, "22");
        SetDlgItemTextA(hWnd, IDC_USER, "root");
        SetDlgItemTextA(hWnd, IDC_PASS, "123456");

        // 密码框样式：默认隐藏密码
        HWND hPassEdit = GetDlgItem(hWnd, IDC_PASS);
        SendDlgItemMessage(hWnd, IDC_PASS, EM_SETPASSWORDCHAR, L'•', 0);
        SendDlgItemMessageW(hWnd, IDC_HOST, EM_SETCUEBANNER, 0, (LPARAM)L"请输入SSH主机IP/域名");
        SendDlgItemMessageW(hWnd, IDC_PORT, EM_SETCUEBANNER, 0, (LPARAM)L"请输入SSH端口");
        SendDlgItemMessageW(hWnd, IDC_USER, EM_SETCUEBANNER, 0, (LPARAM)L"请输入登录用户");
        SendDlgItemMessageW(hWnd, IDC_PASS, EM_SETCUEBANNER, 0, (LPARAM)L"请输入登录密码");
        
        // 加载默认闭眼图标
        HWND hEyeBtn = GetDlgItem(hWnd, IDC_BTN_EYE);
        HICON hEyeHide = (HICON)LoadImageW(s_hInst, MAKEINTRESOURCE(IDI_EYE_HIDE), IMAGE_ICON, 28, 28, LR_DEFAULTCOLOR);
        HICON hEyeShow = (HICON)LoadImageW(s_hInst, MAKEINTRESOURCE(IDI_EYE_SHOW), IMAGE_ICON, 28, 28, LR_DEFAULTCOLOR);
        SendMessageW(hEyeBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hEyeHide);
        // 保存图标句柄到窗口属性，后续切换使用
        SetPropW(hWnd, L"hEyeHide", (HANDLE)hEyeHide);
        SetPropW(hWnd, L"hEyeShow", (HANDLE)hEyeShow);
        // 标记当前密码是否隐藏（默认true隐藏）
        SetPropW(hWnd, L"isPasswordHide", (HANDLE)1);

        s_isConnecting = false; // 初始化连接状态
        return TRUE;
    }

    case WM_COMMAND:
    {
        //SSHPanel* pPanel = SSH_PanelVecBySeqIdGetSSHPanel((int)lParam);
        //pPanel->setLoginPanel(hWnd);
        if (LOWORD(wParam) == IDCANCEL)////取消连接，无论面板什么状态直接断开
        {
            // 取消连接时重置状态
            if (s_isConnecting) {
                SSH_ConnectionOnDisconn(pPanel->Get_panelSeqId());
                s_isConnecting = false;
                NppSSH_LogInfoAuto("用户取消连接，已断开SSH");
            }
            EndDialog(hWnd, IDCANCEL); // 右上角关闭
            pPanel->setSSHConnected(false);//断开连接
        }// ========== 眼睛按钮点击处理 ==========
        else if (LOWORD(wParam) == IDC_BTN_EYE)
        {
            HWND hPassEdit = GetDlgItem(hWnd, IDC_PASS);
            HWND hEyeBtn = GetDlgItem(hWnd, IDC_BTN_EYE);
            // 读取当前状态
            BOOL bHide = (BOOL)GetPropW(hWnd, L"isPasswordHide");
            HICON hHide = (HICON)GetPropW(hWnd, L"hEyeHide");
            HICON hShow = (HICON)GetPropW(hWnd, L"hEyeShow");

            if (bHide)
            {
                // 当前隐藏 → 切换明文，取消掩码
                SendDlgItemMessage(hWnd, IDC_PASS, EM_SETPASSWORDCHAR, 0, 0);
                SendMessageW(hEyeBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hShow);
                SetPropW(hWnd, L"isPasswordHide", (HANDLE)0);
            }
            else
            {
                // 当前明文 → 切换掩码隐藏
                SendDlgItemMessage(hWnd, IDC_PASS, EM_SETPASSWORDCHAR, L'•', 0);
                SendMessageW(hEyeBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hHide);
                SetPropW(hWnd, L"isPasswordHide", (HANDLE)1);
            }
            // 强制重绘密码输入框
            InvalidateRect(hPassEdit, nullptr, TRUE);
            UpdateWindow(hPassEdit);

            InvalidateRect(hEyeBtn, nullptr, TRUE);
            return TRUE;
        }
        else {
            if (LOWORD(wParam) == IDC_BTN_CONNECT || LOWORD(wParam) == IDC_BTN_TEST)//连接按钮
            {
                char SSHhost[256] = { 0 };
                char port[32] = { 0 };
                char user[256] = { 0 };
                char pass[256] = { 0 };

                GetDlgItemTextA(hWnd, IDC_HOST, SSHhost, 256);
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
                bool ok = SSH_ConnectionHandle(pPanel->Get_panelSeqId(), SSHhost, atoi(port), user, pass);
                s_isConnecting = false;// 异步连接立即重置，避免卡死
                if (ok) {
                    NppSSH_LogInfoAuto("SSH连接请求已发送，等待异步结果");
                    MessageBoxW(hWnd, L"SSH 连接成功 ✅", L"NppSSH", MB_OK | MB_TASKMODAL);
                    if (LOWORD(wParam) == IDC_BTN_TEST) {
                        //无论成功还是失败都断开连接，防止占用远程资源
                        SSH_ConnectionOnDisconn(pPanel->Get_panelSeqId());
                    }
                    else {
                        EndDialog(hWnd, IDOK); // 官方标准关闭

                    }
                    if (pPanel && LOWORD(wParam) == IDC_BTN_CONNECT) {
                        pPanel->setSSHConnected(true);//更新面板显示效果，绑定面板ID和session
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
    //对话框销毁后的所有操作
    case WM_DESTROY:
        wchar_t buf[128]{};
        swprintf_s(buf, _countof(buf), L"SSHLoginDlg-%p", hWnd);
        RemovePropW(hWnd, buf);
        //SSHPanel* pPanel = SSH_PanelVecBySeqIdGetSSHPanel((int)lParam);
        // 模态对话框销毁 → POST 消息给伪终端 → 自动修复光标
        if (pPanel)
        {
            // 释放眼睛图标资源
            HICON hHide = (HICON)GetPropW(hWnd, L"hEyeHide");
            HICON hShow = (HICON)GetPropW(hWnd, L"hEyeShow");
            if (hHide) DestroyIcon(hHide);
            if (hShow) DestroyIcon(hHide);
            RemovePropW(hWnd, L"hEyeHide");
            RemovePropW(hWnd, L"hEyeShow");
            RemovePropW(hWnd, L"isPasswordHide");


            HWND hEdit = pPanel->GetOutputEditHandle();
            if (hEdit && IsWindow(hEdit))
            {
                // 关键：必须用 PostMessage，不能用 SendMessage
                //PostMessageW(hEdit, WM_USER + 1001, 0, 0);
                SSH_TerminalSetEnglishType(pPanel->Get_panelSeqId());//强制将微软拼音的输入模式改为英文模式


                HWND panelHwnd = hWnd;
                RECT rc;
                GetClientRect(panelHwnd, &rc);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;
                NppSSH_LogInfoAuto("WM_SIZE 面板新尺寸 -> 宽度:" + std::to_string(w)
                    + "  高度:" + std::to_string(h));
                if (w > 0 && h > 0)
                {
                    // 伪造一次 WM_SIZE（SIZE_RESTORED 表示“正常尺寸变化”）
                    SendMessageW(
                        panelHwnd,
                        WM_SIZE,
                        SIZE_RESTORED,
                        MAKELPARAM(w, h)
                    );
                }
            }
        }

        //::SetFocus(pPanel->GetOutputEditHandle());//销毁时候要将焦点放到终端模拟器

        //pPanel = nullptr;
        NppSSH_LogInfoAuto("登录对话框销毁");
        return TRUE;
    }

    return FALSE;
}
std::mutex g_panelVecMtx;
static thread_local bool s_bProcessingMsg = false;
LRESULT CALLBACK SSHPanel::PanelSubclassWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    //bool isOk = WM_SETFONT || WM_INITDIALOG || WM_GETDLGCODE || WM_KILLFOCUS || WM_IME_SETCONTEXT || WM_SETFOCUS;
    char mbuf[64] = { 0 };
    sprintf(mbuf, "0x%04X", msg);
    std::string msgStr(mbuf);
    //NppSSH_LogInfoAuto("【拦截PanelSubclassWndProc】消息message===" + msgStr);

    LRESULT res = 0;
    wchar_t buf[128]{};
    swprintf_s(buf, _countof(buf), L"SSHPanel-%p", hWnd);
    SSHPanel* dockPanel = (SSHPanel*)GetProp(hWnd, buf);

    //if (!dockPanel) {
    //    std::lock_guard<std::mutex> lock(g_panelVecMtx);
    //    for (auto* pPanel : g_SSHPanelVec) {
    //        if (pPanel && pPanel->Get_hTopParent() == hWnd) {
    //            dockPanel = pPanel;
    //            SetProp(hWnd, buf, (HANDLE)dockPanel);
    //            break;
    //        }
    //    }
    //}

    if (!dockPanel) {
        NppSSH_LogInfoAuto("PanelSubclassWndProc未找到终端！hWnd=" + PtrToHexStr(hWnd) + " msg=" + IntToStr(msg));
        WNDPROC oldProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        res = oldProc ? CallWindowProc(oldProc, hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam);
        return res;
    }

    WNDPROC oldWndProc = dockPanel->Get_oldPanelWndProc();
    if (!oldWndProc) {
        NppSSH_LogInfoAuto("原始的窗口过程未找到！hWnd=" + PtrToHexStr(hWnd) + " msg=" + IntToStr(msg));
        return TRUE;
        //oldWndProc = DefWindowProc;//防止oldWndProc为空，用系统默认窗口执行过程兜底
    }
    // 区分需要拦截的非客户区消息，其余直接放行（和TerminalEditProc筛选逻辑对齐）
    bool isNCInterceptMsg = (WM_CLOSE);
    if (!isNCInterceptMsg) {
        return CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
    }
    switch (msg)
    {
        case WM_CLOSE:
        {
            //NppSSH_LogInfoAuto("【拦截WM_CLOSE】消息message===" + msgStr);
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
                    s_bProcessingMsg = false;
                    return 0;
                }
            }
            // 用户确认放行，执行原生关闭逻辑
            res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
            s_bProcessingMsg = false;
            return res;
        }
        default:{// 其余消息交给原始窗口过程处理
            res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
            s_bProcessingMsg = false;//放重入
            return res;
        }
    }
    // 其余消息交给原始窗口过程处理
    res = CallWindowProc(oldWndProc, hWnd, msg, wParam, lParam);
    s_bProcessingMsg = false;
    return res;
}

inline std::string CheckHwndParentChildRelation(HWND hRoot, HWND hTarget)
{
    if (hRoot == nullptr || hTarget == nullptr)
        return "无效句柄";
    if (hRoot == hTarget)
    {
        char buf[256]{};
        sprintf(buf, "0x%p 与 0x%p 是同一个窗口", hRoot, hTarget);
        return std::string(buf);
    }

    // 向上遍历父窗口，看 hTarget 是否是 hRoot 的祖先
    int upCount = 0;
    HWND hCur = GetParent(hRoot);
    while (hCur != nullptr)
    {
        upCount++;
        if (hCur == hTarget)
        {
            char buf[512]{};
            sprintf(buf, "0x%p 查找%d次父级找到句柄 0x%p", hRoot, upCount, hTarget);
            return std::string(buf);
        }
        hCur = GetParent(hCur);
    }

    // 向下递归遍历所有子窗口，统计层级
    auto FindChildRecursive = [&](auto&& self, HWND parent, int level) -> int
        {
            HWND child = GetWindow(parent, GW_CHILD);
            while (child != nullptr)
            {
                if (child == hTarget)
                    return level;
                int subLevel = self(self, child, level + 1);
                if (subLevel != -1)
                    return subLevel;
                child = GetWindow(child, GW_HWNDNEXT);
            }
            return -1;
        };
    int childLevel = FindChildRecursive(FindChildRecursive, hRoot, 1);
    if (childLevel != -1)
    {
        char buf[512]{};
        sprintf(buf, "0x%p 查找%d次子级找到句柄 0x%p", hRoot, childLevel, hTarget);
        return std::string(buf);
    }

    // 既不是父祖先，也不是子后代
    char buf[512]{};
    sprintf(buf, "0x%p 和 0x%p 无父子层级关系", hRoot, hTarget);
    return std::string(buf);
}

HBITMAP SSHPanel::LoadImageByGdiPlus(const WCHAR* filePath)
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

/*
* 面板处理开始
*/
// 重写原生run_dlgProc：创建面板内UI，处理窗口消息（纯原生）
INT_PTR CALLBACK SSHPanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    
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
    case WM_SIZE: 
    {
        UINT sizeType = (UINT)wParam;
        int nClientW = LOWORD(lParam);
        int nClientH = HIWORD(lParam);

        if (sizeType == SIZE_MINIMIZED || nClientW <= 0 || nClientH <= 0)
            break;
        SetProp(_hOutputEdit, L"NppSSH_PanelW", (HANDLE)(LONG_PTR)nClientW);
        SetProp(_hOutputEdit, L"NppSSH_PanelH", (HANDLE)(LONG_PTR)nClientH);

        // 只负责重置定时器
        SetTimer(GetHwndSelf(), TIMER_ID_RESIZE_PTY, 200, nullptr);
        //NppSSH_LogInfoAuto("WM_SIZE 面板新尺寸 -> 宽度:" + std::to_string(nClientW)
            //+ "  高度:" + std::to_string(nClientH));

        if (initPanle && GetHwndSelf() && ::IsWindow(GetHwndSelf()) && _hOutputEdit && ::IsWindow(_hOutputEdit))
        {
            //::MessageBoxW(s_nppData._nppHandle, L"SSH面板变化", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
            SSH_TerminalResize(GetHwndSelf(), this->_panelSeqId);

            //重绘【整个 SSH 面板】 + 面板里面所有的子控件（包括按钮、编辑框、滚动条等全部子窗口）RDW_ALLCHILDREN = 把面板里所有子控件全部刷新一遍
            ::RedrawWindow(GetHwndSelf(), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);// 刷新整个面板 + 所有子控件（解决最大化/还原/遮挡BUG）
        }
        return TRUE;
    }
    case WM_TIMER:
    {
        if (wParam == TIMER_ID_RESIZE_PTY)
        {
            KillTimer(GetHwndSelf(), TIMER_ID_RESIZE_PTY);
            SendMessageW(_hOutputEdit,WM_USER_RESIZE_PTY,0,0);
        }
        break;
    }
    // 处理按钮点击消息
    case WM_COMMAND: 
    {
        UINT cmd = LOWORD(wParam);
        HWND hCtrl = (HWND)lParam;
        if (cmd == IDC_BTN_CONNECT_SSH) {
            NppSSH_LogInfoAuto("用户点击面板连接按钮，显示登录对话框");
            ShowSSHLoginWindow_Modal();
        }
        else if (cmd == IDC_BTN_DISCONNECT_SSH) {
            NppSSH_LogInfoAuto("用户点击面板断开按钮"+ std::to_string(this->_panelSeqId));
            if (_isConnected) {
                disconnectSSH(); // 断开连接
            }
        }
        break;
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
        int closeResult = IDNO;
        bool isCloseNotify = false;
        // 1.消息来源是面板：全部放行，交给编辑框子类处理
        if (pnmh->hwndFrom == g_nppData._nppHandle && pnmh->code == DMN_CLOSE)
        {
            NppSSH_LogInfoAuto("面板【准备】关闭，当前连接状态：" + std::to_string(_isConnected)+ "【触发关闭，执行断开】" + std::string(bufNMHDR));
            const bool bHasActiveConn = this->Get_isConnected();

            // 检查当前面板是否有活跃SSH连接
            if (bHasActiveConn)
            {
                this->disconnectSSH();   // 断开连接
                this->display(false);//准备销毁，先隐藏防止不完整的面板出现影响效果
            }
            SendMessageW(GetHwndSelf(), WM_CLOSE, wParam, lParam);
        }

        // 2.消息来源是Terminal富文本：全部放行，交给编辑框子类处理
        if (pnmh->hwndFrom == this->_hOutputEdit)
        {
            //NppSSH_LogInfoAuto("父转发Terminal通知 code:" + std::to_string(pnmh->code));
            SendMessageW(_hOutputEdit, WM_NOTIFY, wParam, lParam);
        }
        return TRUE;
    }
    // 面板关闭：原生NPP消息，自动清理资源，无内存泄漏
    case WM_CLOSE: 
    {
        NppSSH_LogInfoAuto("SSH_Panel面板【开始】关闭，当前连接状态：" + std::to_string(_isConnected));
        //SSH_TerminalBySeqIdRemove(_panelSeqId);
        // 从NPP原生停靠管理器移除面板
        ::SendMessage(s_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, (LPARAM)getHSelf());
        ::SendMessage(s_nppData._nppHandle, NPPM_DMMHIDE, 0, (LPARAM)getHSelf());
        SSH_PanelVecBySeqIdRemove(_panelSeqId, _panelrealId);
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
void SSHPanel_InitRecreatePanel(SSHBasePanel* pNewPanel) {
    if (s_nppData._nppHandle == NULL || s_hInst == NULL) {
        ::MessageBoxW(s_nppData._nppHandle, L"NPP环境未初始化，无法重建面板！", L"NppSSH错误", MB_OK | MB_ICONWARNING);
        return;
    }
    //s_panelCounter++;// 同步计数器，保证新创建面板ID不重复
    //SSHPanel* pNewPanel = new SSHPanel(panelId);
    SSHPanel* pCon = dynamic_cast<SSHPanel*>(pNewPanel);
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
        pCon->setForegroundColor(RGB(255, 0, 0));
        pCon->display(true);
        //::SendMessage(s_nppData._nppHandle, NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(pCon->getHSelf()));
        // 额外触发标签栏重绘（兜底）
        //::RedrawWindow(s_nppData._nppHandle, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
    }
}

