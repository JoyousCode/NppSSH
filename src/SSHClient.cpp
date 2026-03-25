#include <SSHClient.h>
#include <shlwapi.h>
#include <atomic>
#include <algorithm>
// 全局原子计数器：生成唯一面板ID，保证多标签不重复（线程安全）
static std::atomic<int> g_panelCounter = 0;
// 全局面板句柄数组：管理多面板，支持NPP标签切换
static std::vector<NppSSHDockPanel*> g_sshPanels;

// 全局 SSH 状态
LIBSSH2_SESSION* sshSession = nullptr;
SOCKET sock = INVALID_SOCKET;
bool connected = false;

// 初始的连接信息
const char* host = "36.33.27.234";
int port = 4922;
const char* user = "plmadm";
const char* pass = "plmadm1";

NppData g_nppData;
HINSTANCE g_hInst;

// 面板初始化：纯原生接口
void NppSSHDockPanel::initPanel() {
    // 1. 调用DockingDlgInterface原生init：绑定NPP实例和父窗口
    DockingDlgInterface::init(g_hInst, g_nppData._nppHandle);

    // 2. 初始化原生tTbData结构体（完全按Docking.h定义，无多余成员）
    ZeroMemory(&_dockData, sizeof(tTbData));
    // 面板标签名（多标签区分：NppSSH-1、NppSSH-2...，NPP底部标签栏显示）
    std::wstring panelTitle = L"NppSSH-" + std::to_wstring(_panelId);
    // 关键：将临时字符串拷贝到静态缓冲区（避免析构后pszName指向空）
    static wchar_t titleBuf[64];
    wcscpy_s(titleBuf, panelTitle.c_str());
    //_dockData.pszName = panelTitle.c_str();          // 原生成员：面板名称
    _dockData.pszName = titleBuf;
    _dockData.uMask = DWS_DF_CONT_BOTTOM;            // 原生常量：初始停靠在底部
    _dockData.dlgID = IDD_SSH_PANEL;                 // 原生成员：对话框ID
    _dockData.pszModuleName = this->getPluginFileName(); // 原生方法：获取插件模块名（NPP识别用）
    _dockData.hIconTab = nullptr;                    // 图标设为null，复用NPP默认图标（无报错）
    _dockData.pszAddInfo = nullptr;                  // 无额外信息，设为null
    // ======================
// 弹出 dockData 全部内容
// ======================
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
    DockingDlgInterface::create(&_dockData);

    // 4. 从资源中获取EDIT控件句柄（不再手动CreateWindow）
    _hOutputEdit = ::GetDlgItem(_hSelf, IDC_OUTPUT_EDIT);
    if (_hOutputEdit) {
        // 设置初始文本 + 复用NPP字体
        ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\n等待SSH连接...");
        HFONT hDefaultFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        ::SendMessage(_hOutputEdit, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    }
    // 5. 加入全局管理，支持标签切换和内存清理
    g_sshPanels.push_back(this);
}

// 重写原生run_dlgProc：创建面板内UI，处理窗口消息（纯原生）
INT_PTR CALLBACK NppSSHDockPanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        // 原生消息：面板窗口创建完成，初始化内部UI（输出文本框）
    case WM_INITDIALOG: {
        ::MessageBox(NULL, TEXT("WM_INITDIALOG!"), TEXT("NppSSH提示UI"), MB_OK);

        if (!_hSelf) {
            ::MessageBox(NULL, TEXT("!_hSelf!"), TEXT("NppSSH提示UI"), MB_OK);

            return FALSE;
        }
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
        return TRUE;
    }

     // 面板关闭：原生NPP消息，自动清理资源，无内存泄漏
    case WM_CLOSE: {
        // 从NPP原生停靠管理器移除面板
        //::SendMessage(g_nppData._nppHandle, NPPM_REMOVEDOCKINGDLG, 0, reinterpret_cast<LPARAM>(_hSelf));
        // 从全局管理数组中移除
        auto it = std::find(g_sshPanels.begin(), g_sshPanels.end(), this);
        if (it != g_sshPanels.end()) g_sshPanels.erase(it);
        // 调用原生destroy方法，销毁窗口
        this->destroy();
        // 释放面板实例
        delete this;
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
        ::MessageBoxW(NULL, L"Notepad++插件环境已经初始化！", L"NppSSH初始化提示", MB_OK | MB_ICONERROR);

        //    // 生成唯一面板ID，创建新面板实例（每次调用新建一个，多标签）
        int newPanelId = ++g_panelCounter;
        NppSSHDockPanel* pNewPanel = new NppSSHDockPanel(newPanelId);
        pNewPanel->initPanel();
        pNewPanel->display(true); // true=显示面板，原生接口无报错
        return;
    }
}
