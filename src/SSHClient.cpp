#include "SSHClient.h"


// 仅保留分发逻辑，核心实现全部委托给SSHWindow
void CreateNppSSHTerminal() {
    // 前置检查（复用SSHWindow的全局变量）
    if (g_nppData._nppHandle == NULL || g_hInst == NULL) {
        ::MessageBoxW(NULL, L"Notepad++插件环境未初始化！", L"NppSSH提示", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t szSuccessMsg[256] = { 0 };
    swprintf_s(szSuccessMsg, 256,
        L"调用Notepad++插件环境初始化数据！\n\n"
        L"g_nppData._nppHandle = %p\n"
        L"g_hInst = %p",
        g_nppData._nppHandle,
        g_hInst);
    ::MessageBoxW(NULL, szSuccessMsg, L"NppSSH初始化提示", MB_OK | MB_ICONINFORMATION);

    // 生成唯一面板ID，创建新面板（复用SSHWindow的面板类）
    int newPanelId = ++g_panelCounter;
    NppSSHDockPanel* pNewPanel = new NppSSHDockPanel(newPanelId);
    if (pNewPanel) {
        pNewPanel->initPanel();
        // 显示面板：使用Notepad_plus_msgs.h原生消息NPPM_DMMSHOW（NPPMSG+30）
        ::SendMessage(g_nppData._nppHandle, NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(pNewPanel->getHSelf()));
    }
    // 同步INI（替换原注册表）
    SavePanelCountToIni(g_sshPanels.size());
}