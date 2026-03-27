// SSHPanel.cpp（面板 + 注册表具体实现）
#include "SSHPanel.h"

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

// 注册表操作具体实现
void SSHPanel_SavePanelCountToReg(int count) {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, NPP_SSH_REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, NPP_SSH_PANEL_COUNT, 0, REG_DWORD, (const BYTE*)&count, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

int SSHPanel_LoadPanelCountFromReg() {
    HKEY hKey;
    DWORD count = 0;
    DWORD dwSize = sizeof(DWORD);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, NPP_SSH_REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueEx(hKey, NPP_SSH_PANEL_COUNT, 0, NULL, (BYTE*)&count, &dwSize);
        RegCloseKey(hKey);
    }
    return (int)count;
}

void SSHPanel_DeletePanelCountFromReg() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, NPP_SSH_REG_PATH, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValue(hKey, NPP_SSH_PANEL_COUNT);
        RegCloseKey(hKey);
        RegDeleteKey(HKEY_CURRENT_USER, NPP_SSH_REG_PATH);
    }
}

// 面板类构造函数
NppSSHDockPanel::NppSSHDockPanel(int panelId)
    : DockingDlgInterface(IDD_SSH_PANEL),
    _dockData(),
    _panelId(panelId),
    _hOutputEdit(NULL),
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
}

// 断开当前面板的SSH连接（无提示）
void NppSSHDockPanel::disconnectSSH() {
    if (_isSSHConnected) {
        // 调用SSHConnection的断开逻辑
        NppSSH_Disconnect();
        _isSSHConnected = false;
    }
}

void NppSSHDockPanel::resetPanelToInit() {
    disconnectSSH();
    if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
        ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\n等待SSH连接...");
    }
    // 可选：重置面板时，启用连接SSH按钮（若之前置灰）
    if (_hBtnConnectSSH && ::IsWindow(_hBtnConnectSSH)) {
        ::EnableWindow(_hBtnConnectSSH, TRUE);
    }
}
// 新增：创建面板顶部独立按钮栏
void NppSSHDockPanel::createTopButtonBar() {
    if (!_hSelf || !::IsWindow(_hSelf)) return;

    // 获取面板客户区大小，确定按钮位置（顶部横向排列，左对齐，边距10）
    RECT rcClient;
    ::GetClientRect(_hSelf, &rcClient);
    int btnWidth = 80;  // 按钮宽度
    int btnHeight = 28; // 按钮高度
    int btnMargin = 10; // 按钮左右边距
    int btnTop = 10;    // 按钮顶部边距
    int btnGap = 10; // 按钮之间的间距

    // 创建「连接SSH」按钮（Unicode版，适配NPP中文环境）
    _hBtnConnectSSH = ::CreateWindowW(
        L"BUTTON",                // 控件类名
        L"连接SSH",               // 按钮文字
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER, // 控件样式
        btnMargin,                // 左坐标
        btnTop,                   // 上坐标
        btnWidth,                 // 宽度
        btnHeight,                // 高度
        _hSelf,                   // 父窗口句柄
        (HMENU)IDC_BTN_CONNECT_SSH, // 按钮ID
        ::GetModuleHandleW(NULL), // 实例句柄
        NULL
    );
    // 新增：创建「断开SSH」按钮（在连接按钮右侧，间隔10px）
    _hBtnDisconnectSSH = ::CreateWindowW(
        L"BUTTON",
        L"断开SSH",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_BORDER,
        btnMargin + btnWidth + btnGap, // 左坐标 = 连接按钮左 + 宽度 + 间距
        btnTop,
        btnWidth,
        btnHeight,
        _hSelf,
        (HMENU)IDC_BTN_DISCONNECT_SSH,
        ::GetModuleHandleW(NULL),
        NULL
    );

    // 复用NPP默认字体，保证按钮样式与NPP统一
    HFONT hNppFont = (HFONT)::SendMessage(s_nppData._nppHandle, NPPM_SETSMOOTHFONT, 0, 0);
    if (hNppFont == NULL) hNppFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    ::SendMessage(_hBtnConnectSSH, WM_SETFONT, (WPARAM)hNppFont, TRUE);
    ::SendMessage(_hBtnDisconnectSSH, WM_SETFONT, (WPARAM)hNppFont, TRUE);
}
// 面板初始化：纯原生接口
void NppSSHDockPanel::initPanel() {
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
            10, 50, rcClient.right - 20, rcClient.bottom - 60,
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
    // 面板大小变化时，自动适配输出文本框（防止遮挡/空白）
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
                10, 50, rc.right - 20, rc.bottom - 60,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
        }
        // 新增：面板大小变化时，按钮栏宽度自适应（可选，保持按钮左对齐即可）
        if (_hBtnConnectSSH && ::IsWindow(_hBtnConnectSSH)) {
            ::SetWindowPos(_hBtnConnectSSH, NULL, 10, 10, 80, 28, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        // 新增：更新断开按钮位置
        if (_hBtnDisconnectSSH && ::IsWindow(_hBtnDisconnectSSH)) {
            ::SetWindowPos(_hBtnDisconnectSSH, NULL, 100, 10, 80, 28, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return TRUE;
    }
    // ====== 新增：处理按钮点击消息 WM_COMMAND ======
    case WM_COMMAND: {
        // 判断是否为连接SSH按钮点击
        if (LOWORD(wParam) == IDC_BTN_CONNECT_SSH) {
            
            // 弹出提示框，验证按钮可用，后续替换为实际SSH连接逻辑
            ::MessageBoxW(_hSelf, L"连接SSH", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
            ::onNppSSH();
        }
        // 新增：处理断开SSH按钮点击
        else if (LOWORD(wParam) == IDC_BTN_DISCONNECT_SSH) {
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
        SSHPanel_SavePanelCountToReg(s_sshPanels.size());    // 同步更新注册表面板数量
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
    int panelCount = SSHPanel_LoadPanelCountFromReg();
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