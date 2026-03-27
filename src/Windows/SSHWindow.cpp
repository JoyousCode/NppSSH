//SSHWindow.cpp（仅分发调用，无具体逻辑）
#include "SSHWindow.h"
#include "SSHPanel.h"
#include "SSHConnection.h"

// 全局变量转发（实际定义在SSHPanel中）
std::vector<NppSSHDockPanel*>& g_sshPanels = SSHPanel_GetGlobalPanels();
std::atomic<int>& g_panelCounter = SSHPanel_GetGlobalPanelCounter();
NppData& g_nppData = SSHPanel_GetGlobalNppData();
HINSTANCE& g_hInst = SSHPanel_GetGlobalHInst();

// SSH连接全局状态转发（实际定义在SSHConnection中）
LIBSSH2_SESSION*& sshSession = SSHConnection_GetSession();
SOCKET& sock = SSHConnection_GetSocket();
bool& connected = SSHConnection_GetConnectedState();
const char*& host = SSHConnection_GetHost();
int& port = SSHConnection_GetPort();
const char*& user = SSHConnection_GetUser();
const char*& pass = SSHConnection_GetPass();

// 注册表操作转发
void SavePanelCountToReg(int count) {
    SSHPanel_SavePanelCountToReg(count);
}

int LoadPanelCountFromReg() {
    return SSHPanel_LoadPanelCountFromReg();
}

void DeletePanelCountFromReg() {
    SSHPanel_DeletePanelCountFromReg();
}

// NPP启动重建面板转发
void RecreatePanelsOnNppStart() {
    SSHPanel_RecreatePanelsOnNppStart();
}

// SSH连接操作转发
bool NppSSH_Connect(const char* host, int port, const char* user, const char* pass) {
    return SSHConnection_Connect(host, port, user, pass);
}

void NppSSH_Disconnect() {
    SSHConnection_Disconnect();
}

bool NppSSH_IsConnected() {
    return SSHConnection_IsConnected();
}

void NppSSH_ResetConnectionState() {
    SSHConnection_ResetState();
}


//NppSSHDockPanel* CreateNewSSHDockPanel(int panelId) {
//    return new NppSSHDockPanel(panelId);
//}
//在 SSHWindow 中提供全局清理接口（如 NppSSH_Cleanup），
// 内部统一调用 SSHPanel 的面板清理和 SSHConnection 的连接清理，
// 让 NppPluginSSH 的 DLL_PROCESS_DETACH、NPPN_SHUTDOWN 仅调用此一个接口，避免清理逻辑分散导致的泄漏：
//void NppSSH_Cleanup() {
//    // 清理SSH连接
//    NppSSH_Disconnect();
//    // 清理面板
//    for (auto* panel : g_sshPanels) {
//        if (panel) delete panel;
//    }
//    g_sshPanels.clear();
//    // 清理注册表
//    DeletePanelCountFromReg();
//}





//// 全局变量定义
//std::vector<NppSSHDockPanel*> g_sshPanels;
//std::atomic<int> g_panelCounter = 0;
//NppData g_nppData;
//HINSTANCE g_hInst;
//
//// SSH连接全局状态
//LIBSSH2_SESSION* sshSession = nullptr;
//SOCKET sock = INVALID_SOCKET;
//bool connected = false;
//const char* host = "36.33.27.234";
//int port = 22;
//const char* user = "";
//const char* pass = "";
//
//// 注册表操作实现
//void SavePanelCountToReg(int count) {
//    HKEY hKey;
//    if (RegCreateKeyEx(HKEY_CURRENT_USER, NPP_SSH_REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
//        RegSetValueEx(hKey, NPP_SSH_PANEL_COUNT, 0, REG_DWORD, (const BYTE*)&count, sizeof(DWORD));
//        RegCloseKey(hKey);
//    }
//}
//
//int LoadPanelCountFromReg() {
//    HKEY hKey;
//    DWORD count = 0;
//    DWORD dwSize = sizeof(DWORD);
//    if (RegOpenKeyEx(HKEY_CURRENT_USER, NPP_SSH_REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
//        RegQueryValueEx(hKey, NPP_SSH_PANEL_COUNT, 0, NULL, (BYTE*)&count, &dwSize);
//        RegCloseKey(hKey);
//    }
//    return (int)count;
//}
//
//void DeletePanelCountFromReg() {
//    HKEY hKey;
//    if (RegOpenKeyEx(HKEY_CURRENT_USER, NPP_SSH_REG_PATH, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
//        RegDeleteValue(hKey, NPP_SSH_PANEL_COUNT);
//        RegCloseKey(hKey);
//        RegDeleteKey(HKEY_CURRENT_USER, NPP_SSH_REG_PATH);
//    }
//}
//
//// 面板类构造函数
//NppSSHDockPanel::NppSSHDockPanel(int panelId)
//    : DockingDlgInterface(IDD_SSH_PANEL),
//    _dockData(),
//    _panelId(panelId),
//    _hOutputEdit(NULL),
//    _isSSHConnected(false) {
//    ZeroMemory(_titleBuf, sizeof(_titleBuf));
//}
//
//// TODO:登录成功后,添加: _setSSHConnected(true);
//// 判断SSH是否连接
//bool NppSSHDockPanel::isSSHConnected() const {
//    return _isSSHConnected;
//}
//// 设置SSH是否连接
//void NppSSHDockPanel::setSSHConnected(bool state) {
//    _isSSHConnected = state;
//}
//// 断开当前面板的SSH连接（无提示）
//void NppSSHDockPanel::disconnectSSH() {
//    if (_isSSHConnected) {
//        // 断开SSH会话
//        if (sshSession != nullptr) {
//            libssh2_session_disconnect(sshSession, "Panel closed manually");
//            libssh2_session_free(sshSession);
//            sshSession = nullptr;
//        }
//        // 关闭Socket
//        if (sock != INVALID_SOCKET) {
//            closesocket(sock);
//            sock = INVALID_SOCKET;
//        }
//        // 更新状态
//        connected = false;
//        _isSSHConnected = false;
//    }
//}
//
//void NppSSHDockPanel::resetPanelToInit() {
//    disconnectSSH();
//    if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
//        ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\n等待SSH连接...");
//    }
//}
//// 面板初始化：纯原生接口
//void NppSSHDockPanel::initPanel() {
//    // 检查资源是否存在
//    HRSRC hRes = ::FindResource(g_hInst, MAKEINTRESOURCE(IDD_SSH_PANEL), RT_DIALOG);
//    if (hRes == NULL) {
//        wchar_t errMsg[256] = { 0 };
//        swprintf_s(errMsg, L"找不到IDD_SSH_PANEL资源！GetLastError: %d", ::GetLastError());
//        ::MessageBoxW(NULL, errMsg, L"NppSSH资源错误", MB_OK | MB_ICONERROR);
//        return;
//    }
//
//    DockingDlgInterface::init(g_hInst, g_nppData._nppHandle);   // 1. 调用DockingDlgInterface原生init：绑定NPP实例和父窗口
//    ZeroMemory(&_dockData, sizeof(tTbData));                    // 2. 初始化原生tTbData结构体（完全按Docking.h定义，无多余成员）
//
//    // 面板标签名（多标签区分：NppSSH-1、NppSSH-2...，NPP底部标签栏显示）
//    std::wstring panelTitle = L"NppSSH-" + std::to_wstring(_panelId);
//    // 关键：将临时字符串拷贝到静态缓冲区（避免析构后pszName指向空）
//    //static wchar_t titleBuf[64];
//    wcscpy_s(_titleBuf, _countof(_titleBuf), panelTitle.c_str());
//
//    _dockData.pszName = _titleBuf;                           // 原生成员：面板名称
//    _dockData.uMask = DWS_DF_CONT_BOTTOM | DWS_DF_FLOATING;  // 面板默认停靠在底部和允许面板浮动为独立窗口
//    _dockData.iPrevCont = CONT_BOTTOM;                       // 原生要求：记录上一次停靠位置为底部
//    _dockData.dlgID = IDD_SSH_PANEL;                        // 原生成员：对话框ID
//    _dockData.pszModuleName = this->getPluginFileName();    // 原生方法：获取插件模块名（NPP识别用）
//    _dockData.hIconTab = nullptr;                           // 图标设为null，复用NPP默认图标（无报错）
//    _dockData.pszAddInfo = nullptr;                         // 无额外信息，设为null
//
//    wchar_t msgBuf[512] = { 0 };        // 弹出 dockData 全部内容
//    swprintf_s(msgBuf, 512,
//        L"创建面板窗口准备 _dockData 的数据：\n\n"
//        L"pszName: %s\n"
//        L"uMask: %u\n"
//        L"dlgID: %d\n"
//        L"pszModuleName: %s\n"
//        L"hIconTab: %p\n"
//        L"pszAddInfo: %s\n"
//        L"iPrevCont: %d",
//        _dockData.pszName ? _dockData.pszName : L"NULL",
//        _dockData.uMask,
//        _dockData.dlgID,
//        _dockData.pszModuleName ? _dockData.pszModuleName : L"NULL",
//        _dockData.hIconTab,
//        _dockData.pszAddInfo ? _dockData.pszAddInfo : L"NULL",
//        _dockData.iPrevCont
//    );
//    ::MessageBoxW(NULL, msgBuf, L"NppSSH: _dockData", MB_OK);
//
//    // 调用DockingDlgInterface原生create：绑定停靠数据，创建面板窗口
//    //DockingDlgInterface::create(&_dockData);
//    StaticDialog::create(_dlgID, false);
//    _dockData.hClient = _hSelf;
//    if (!_hSelf) {
//        ::MessageBoxW(NULL, L"面板窗口创建失败！", L"NppSSH错误", MB_OK | MB_ICONERROR);
//        return;
//    }
//    ::SendMessage(g_nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&_dockData));
//    ::SendMessage(g_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD, reinterpret_cast<LPARAM>(_hSelf));
//
//    // 4. 从资源中获取EDIT控件句柄（不再手动CreateWindow）
//    _hOutputEdit = ::GetDlgItem(_hSelf, IDC_OUTPUT_EDIT);
//    if (_hOutputEdit) {
//        ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\n等待SSH连接...");           // 设置初始文本 + 复用NPP字体
//        HFONT hNppFont = (HFONT)::SendMessage(g_nppData._nppHandle, NPPM_SETSMOOTHFONT, 0, 0);
//        if (hNppFont == NULL) hNppFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
//        ::SendMessage(_hOutputEdit, WM_SETFONT, (WPARAM)hNppFont, TRUE);
//        ::SendMessage(_hOutputEdit, EM_GETLINECOUNT, 0, 0);     // 设置编辑框自适应换行/滚动
//        ::SendMessage(_hOutputEdit, EM_SETTABSTOPS, 1, (LPARAM)8);
//    }
//    // 5. 加入全局管理，支持标签切换和内存清理
//    g_sshPanels.push_back(this);
//
//    if (_hSelf && ::IsWindow(_hSelf)) {         // 强制设置面板窗口样式，解决遮挡/闪烁问题
//        DWORD dwStyle = ::GetWindowLongPtrW(_hSelf, GWL_STYLE);
//        ::SetWindowLongPtrW(_hSelf, GWL_STYLE, dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
//        ::SetWindowPos(_hSelf, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);     // 确保面板在停靠容器的顶层，不被覆盖
//    }
//}
//
//// 重写原生run_dlgProc：创建面板内UI，处理窗口消息（纯原生）
//INT_PTR CALLBACK NppSSHDockPanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
//    switch (message) {      // 原生消息：面板窗口创建完成，初始化内部UI（输出文本框）
//    case WM_INITDIALOG: {
//        if (!_hSelf) {
//            ::MessageBox(NULL, TEXT("面板窗口句柄无效！"), TEXT("NppSSH错误提示"), MB_OK | MB_ICONERROR);
//            return FALSE;
//        }
//        return TRUE;
//    }
//    // 面板大小变化时，自动适配输出文本框（防止遮挡/空白）
//    case WM_SIZE: {
//        if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
//            RECT rc;
//            ::GetClientRect(_hSelf, &rc);
//            ::SetWindowPos(
//                _hOutputEdit,
//                NULL,
//                10, 10, rc.right - 20, rc.bottom - 20,
//                SWP_NOZORDER | SWP_NOACTIVATE
//            );
//        }
//        return TRUE;
//    }
//
//    // 响应NPP停靠管理器的浮动/停靠消息，更新面板状态
//    case WM_NOTIFY: {
//        LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
//        if (pnmh->hwndFrom == g_nppData._nppHandle) {
//            switch (LOWORD(pnmh->code)) {
//            case DMN_FLOAT: _isFloating = true; break;
//            case DMN_DOCK: _isFloating = false; _iDockedPos = HIWORD(pnmh->code); break;
//            case DMN_CLOSE: ::PostMessage(_hSelf, WM_CLOSE, 0, 0); break;
//            }
//        }
//        return TRUE;
//    }
//
//    // 面板关闭：原生NPP消息，自动清理资源，无内存泄漏
//    case WM_CLOSE: {
//        // 检查当前面板是否有活跃SSH连接
//        if (this->isSSHConnected()) {
//            ::MessageBoxW(
//                NULL,
//                L"当前面板存在SSH活跃连接，关闭将断开连接并恢复初始状态！",
//                L"NppSSH 连接提示",
//                MB_OK | MB_ICONWARNING
//            );
//            this->resetPanelToInit();   // 重置为初始状态
//        }
//        // 手动关闭面板：自动断开当前SSH，无提示
//        //disconnectSSH();
//        // 从NPP原生停靠管理器移除面板
//        //::SendMessage(g_nppData._nppHandle, NPPM_REMOVESHORTCUTBYCMDID, 0, reinterpret_cast<LPARAM>(_hSelf));
//        ::SendMessage(g_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, (LPARAM)getHSelf());
//        ::SendMessage(g_nppData._nppHandle, NPPM_DMMHIDE, 0, (LPARAM)getHSelf());
//
//        auto it = std::find(g_sshPanels.begin(), g_sshPanels.end(), this);      // 从全局管理数组中移除
//        if (it != g_sshPanels.end()) g_sshPanels.erase(it);
//
//        // 原生停靠面板清理：先隐藏，再销毁
//        this->display(false);
//        ::DestroyWindow(_hSelf);
//        this->destroy();
//        delete this;                                // 释放面板实例
//        SavePanelCountToReg(g_sshPanels.size());    // 同步更新注册表面板数量
//        return TRUE;
//    }
//    // 其他所有消息，交给DockingDlgInterface原生处理（避免NPP异常）
//    default:
//        return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
//    }
//}
//
//// NPP启动重建面板
//void RecreatePanelsOnNppStart() {
//    if (g_nppData._nppHandle == NULL || g_hInst == NULL) return;
//    int panelCount = LoadPanelCountFromReg();
//    if (panelCount <= 0) return;
//    // 按注册表记录的数量重建面板，ID延续自注册表
//    for (int i = 1; i <= panelCount; i++) {
//        g_panelCounter = i;// 同步计数器，保证新创建面板ID不重复
//        NppSSHDockPanel* pNewPanel = new NppSSHDockPanel(i);
//        if (pNewPanel) {
//            pNewPanel->initPanel();
//            ::SendMessage(g_nppData._nppHandle, NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(pNewPanel->getHSelf()));
//        }
//    }
//}