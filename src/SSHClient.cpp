#include <SSHClient.h>
#include <shlwapi.h>
//#include <atomic>
#include <algorithm>
#include <windowsx.h>
#pragma comment(lib, "shlwapi.lib") // 注册表操作依赖库
// 全局面板句柄数组：管理多面板，支持NPP标签切换
//static std::vector<NppSSHDockPanel*> g_sshPanels;
std::vector<NppSSHDockPanel*> g_sshPanels;
// 全局原子计数器：生成唯一面板ID，保证多标签不重复（线程安全）
static std::atomic<int> g_panelCounter = 0;


// 全局 SSH 状态
LIBSSH2_SESSION* sshSession = nullptr;
SOCKET sock = INVALID_SOCKET;
bool connected = false;

// 初始的连接信息
const char* host = "36.33.27.234";
int port = 22;
const char* user = "";
const char* pass = "";

NppData g_nppData;
HINSTANCE g_hInst;

// 新增：保存面板数量到注册表
void SavePanelCountToReg(int count) {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, NPP_SSH_REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, NPP_SSH_PANEL_COUNT, 0, REG_DWORD, (const BYTE*)&count, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

// 新增：从注册表加载面板数量
int LoadPanelCountFromReg() {
    HKEY hKey;
    DWORD count = 0;
    DWORD dwSize = sizeof(DWORD);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, NPP_SSH_REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueEx(hKey, NPP_SSH_PANEL_COUNT, 0, NULL, (BYTE*)&count, &dwSize);
        RegCloseKey(hKey);
    }
    return (int)count;
}

// 新增：删除注册表面板数量（插件卸载时调用）
void DeletePanelCountFromReg() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, NPP_SSH_REG_PATH, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValue(hKey, NPP_SSH_PANEL_COUNT);
        RegCloseKey(hKey);
        RegDeleteKey(HKEY_CURRENT_USER, NPP_SSH_REG_PATH);
    }
}

// 新增：NPP启动时自动重建所有面板
void RecreatePanelsOnNppStart() {
    if (g_nppData._nppHandle == NULL || g_hInst == NULL) return;
    int panelCount = LoadPanelCountFromReg();
    if (panelCount <= 0) return;
    // 按注册表记录的数量重建面板，ID延续自注册表
    for (int i = 1; i <= panelCount; i++) {
        g_panelCounter = i; // 同步计数器，保证新创建面板ID不重复
        NppSSHDockPanel* pNewPanel = new NppSSHDockPanel(i);
        if (pNewPanel) {
            pNewPanel->initPanel();
            ::SendMessage(g_nppData._nppHandle, NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(pNewPanel->getHSelf()));
        }
    }
}
// TODO:登录成功后,添加
//_setSSHConnected(true);
// // 实现 isSSHConnected()
bool NppSSHDockPanel::isSSHConnected() const {
    return _isSSHConnected;
}

// 实现 setSSHConnected()
void NppSSHDockPanel::setSSHConnected(bool state) {
    _isSSHConnected = state;
}
// 断开当前面板的SSH连接（无提示）
void NppSSHDockPanel::disconnectSSH()
{
    if (_isSSHConnected)
    {
        // 断开SSH会话
        if (sshSession != nullptr) {
            libssh2_session_disconnect(sshSession, "Panel closed manually");
            libssh2_session_free(sshSession);
            sshSession = nullptr;
        }
        // 关闭Socket
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        // 更新状态
        connected = false;
        _isSSHConnected = false;
    }
}
// 面板初始化：纯原生接口
void NppSSHDockPanel::initPanel() {
    // 新增：检查资源是否存在
    HRSRC hRes = ::FindResource(g_hInst, MAKEINTRESOURCE(IDD_SSH_PANEL), RT_DIALOG);
    if (hRes == NULL) {
        wchar_t errMsg[256] = { 0 };
        swprintf_s(errMsg, L"找不到IDD_SSH_PANEL资源！GetLastError: %d", ::GetLastError());
        ::MessageBoxW(NULL, errMsg, L"NppSSH资源错误", MB_OK | MB_ICONERROR);
        return;
    }
    
    DockingDlgInterface::init(g_hInst, g_nppData._nppHandle);   // 1. 调用DockingDlgInterface原生init：绑定NPP实例和父窗口
    ZeroMemory(&_dockData, sizeof(tTbData));    // 2. 初始化原生tTbData结构体（完全按Docking.h定义，无多余成员）
    // 面板标签名（多标签区分：NppSSH-1、NppSSH-2...，NPP底部标签栏显示）
    std::wstring panelTitle = L"NppSSH-" + std::to_wstring(_panelId);
    // 关键：将临时字符串拷贝到静态缓冲区（避免析构后pszName指向空）
    //static wchar_t titleBuf[64];
    wcscpy_s(_titleBuf, _countof(_titleBuf), panelTitle.c_str());
    //wcscpy_s(titleBuf, panelTitle.c_str());
    //_dockData.pszName = panelTitle.c_str();         
    _dockData.pszName = _titleBuf;           // 原生成员：面板名称
    _dockData.uMask = DWS_DF_CONT_BOTTOM | DWS_DF_FLOATING; //面板默认停靠在底部和允许面板浮动为独立窗口
    _dockData.iPrevCont = CONT_BOTTOM;  // 原生要求：记录上一次停靠位置为底部

    //_dockData.uMask = DWS_DF_CONT_BOTTOM;            // 原生常量：初始停靠在底部
    _dockData.dlgID = IDD_SSH_PANEL;                 // 原生成员：对话框ID
    _dockData.pszModuleName = this->getPluginFileName(); // 原生方法：获取插件模块名（NPP识别用）
    _dockData.hIconTab = nullptr;                    // 图标设为null，复用NPP默认图标（无报错）
    _dockData.pszAddInfo = nullptr;                  // 无额外信息，设为null
   
    // 弹出 dockData 全部内容
    wchar_t msgBuf[512] = { 0 };
    swprintf_s(msgBuf, 512,
        L"_dockData 所有内容：\n\n"
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

    ::MessageBoxW(NULL, msgBuf, L"NppSSH: _dockData 完整内容", MB_OK);


    // 3. 调用DockingDlgInterface原生create：绑定停靠数据，创建面板窗口
    //DockingDlgInterface::create(&_dockData);
    StaticDialog::create(_dlgID, false);
    _dockData.hClient = _hSelf;
    if (!_hSelf) {
        ::MessageBoxW(NULL, L"面板窗口创建失败！", L"NppSSH错误", MB_OK | MB_ICONERROR);
        return;
    }
    ::SendMessage(g_nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&_dockData));
    ::SendMessage(g_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD, reinterpret_cast<LPARAM>(_hSelf));


    

    ::MessageBoxW(NULL, msgBuf, L"create NppSSH: _dockData 完整内容", MB_OK);

    // 4. 从资源中获取EDIT控件句柄（不再手动CreateWindow）
    _hOutputEdit = ::GetDlgItem(_hSelf, IDC_OUTPUT_EDIT);
    if (_hOutputEdit) {
        // 设置初始文本 + 复用NPP字体
        ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\n等待SSH连接...");
        HFONT hNppFont = (HFONT)::SendMessage(g_nppData._nppHandle, NPPM_SETSMOOTHFONT, 0, 0);
        if (hNppFont == NULL) hNppFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        ::SendMessage(_hOutputEdit, WM_SETFONT, (WPARAM)hNppFont, TRUE);
        // 设置编辑框自适应换行/滚动
        //::SendMessage(_hOutputEdit, EM_SETREADONLY, TRUE, 0);
        ::SendMessage(_hOutputEdit, EM_GETLINECOUNT, 0, 0);
        // 设置编辑框可自动换行/滚动（优化体验）
        //::SendMessage(_hOutputEdit, EM_SETLINECOUNT, 0, 0);
        ::SendMessage(_hOutputEdit, EM_SETTABSTOPS, 1, (LPARAM)8);
    }
    // 5. 加入全局管理，支持标签切换和内存清理
    g_sshPanels.push_back(this);
    // 强制设置面板窗口样式，解决遮挡/闪烁问题
    if (_hSelf && ::IsWindow(_hSelf)) {
        DWORD dwStyle = ::GetWindowLongPtrW(_hSelf, GWL_STYLE);
        ::SetWindowLongPtrW(_hSelf, GWL_STYLE, dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        // 确保面板在停靠容器的顶层，不被覆盖
        ::SetWindowPos(_hSelf, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

// 重写原生run_dlgProc：创建面板内UI，处理窗口消息（纯原生）
INT_PTR CALLBACK NppSSHDockPanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        // 原生消息：面板窗口创建完成，初始化内部UI（输出文本框）
    case WM_INITDIALOG: {

        if (!_hSelf) {
            ::MessageBox(NULL, TEXT("面板窗口句柄无效！"), TEXT("NppSSH错误提示"), MB_OK | MB_ICONERROR);

            return FALSE;
        }
        // 给面板窗口添加 WS_CLIPCHILDREN 样式（Windows 原生窗口样式，非 DWS_*）
        //DWORD dwStyle = GetWindowLongPtrW(_hSelf, GWL_STYLE);
        //SetWindowLongPtrW(_hSelf, GWL_STYLE, dwStyle | WS_CLIPCHILDREN);
        // 无需手动创建EDIT控件，资源模板已创建，仅需后续适配大小
        return TRUE;
    }
// 面板大小变化时，自动适配输出文本框（防止遮挡/空白）
    case WM_SIZE: {
        if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
            RECT rc;
            ::GetClientRect(_hSelf, &rc);
            ::SetWindowPos(
                _hOutputEdit,
                NULL,
                10, 10, rc.right - 20, rc.bottom - 20,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
        }
        // 确保面板在最顶层，避免被编辑区覆盖
        //::SetWindowPos(_hSelf, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        return TRUE;
    }
    // 响应NPP停靠管理器的浮动/停靠消息，更新面板状态
    case WM_NOTIFY: {
        LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
        if (pnmh->hwndFrom == g_nppData._nppHandle) {
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
            this->resetPanelToInit(); // 重置为初始状态
        }
        // 手动关闭面板：自动断开当前SSH，无提示
        //disconnectSSH();
        // 从NPP原生停靠管理器移除面板
        //::SendMessage(g_nppData._nppHandle, NPPM_REMOVESHORTCUTBYCMDID, 0, reinterpret_cast<LPARAM>(_hSelf));
        ::SendMessage(g_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, (LPARAM)getHSelf());
        ::SendMessage(g_nppData._nppHandle, NPPM_DMMHIDE, 0, (LPARAM)getHSelf());
        // 从全局管理数组中移除
        auto it = std::find(g_sshPanels.begin(), g_sshPanels.end(), this);
        if (it != g_sshPanels.end()) g_sshPanels.erase(it);
        // 原生停靠面板清理：先隐藏，再销毁
        this->display(false);
        ::DestroyWindow(_hSelf);
        this->destroy();
        // 释放面板实例
        delete this;
        // 同步更新注册表面板数量
        SavePanelCountToReg(g_sshPanels.size());
        return TRUE;
    }

     // 其他所有消息，交给DockingDlgInterface原生处理（避免NPP异常）
    default:
        return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
    }
}

void CreateNppSSHTerminal() {
        // 前置检查：NPP插件环境是否初始化（避免空指针）
    if (g_nppData._nppHandle == NULL || g_hInst == NULL) {
        ::MessageBoxW(NULL, L"Notepad++插件环境未初始化！", L"NppSSH提示", MB_OK | MB_ICONERROR);
        return;
    }
    else {
        // 同样显示成功时的句柄值，方便对比
        wchar_t szSuccessMsg[256] = { 0 };
        swprintf_s(szSuccessMsg, 256,
            L"Notepad++插件环境已经初始化！\n\n"
            L"g_nppData._nppHandle = %p\n"
            L"g_hInst = %p",
            g_nppData._nppHandle,
            g_hInst);

        ::MessageBoxW(NULL, szSuccessMsg, L"NppSSH初始化提示", MB_OK | MB_ICONINFORMATION);

        //    // 生成唯一面板ID，创建新面板实例（每次调用新建一个，多标签）
        int newPanelId = ++g_panelCounter;
        NppSSHDockPanel* pNewPanel = new NppSSHDockPanel(newPanelId);
        if (pNewPanel) {
            pNewPanel->initPanel();
            // 显示面板：使用Notepad_plus_msgs.h原生消息NPPM_DMMSHOW（NPPMSG+30）
            ::SendMessage(g_nppData._nppHandle, NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(pNewPanel->getHSelf()));
        }
        //pNewPanel->initPanel();
        //pNewPanel->display(true); // true=显示面板，原生接口无报错
        // 新增：创建面板后立即保存数量到注册表
        SavePanelCountToReg(g_sshPanels.size());
        return;
    }
}
