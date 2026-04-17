//this file is part of notepad++
//Copyright (C)2022 Don HO <don.h@free.fr>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "SSHClient.h" 
#include "Windows/SSHPanel.h"
#include "SSHSettings.h" // 引入INI工具

extern FuncItem funcItem[nbFunc];
extern NppData nppData;
extern NppData& g_nppData;
extern HINSTANCE& g_hInst;


BOOL APIENTRY DllMain(HANDLE hModule, DWORD  reasonForCall, LPVOID /*lpReserved*/)
{
	try {

		switch (reasonForCall)
		{
			case DLL_PROCESS_ATTACH:
				g_hInst = (HINSTANCE)hModule;
				pluginInit(hModule);
				break;

			case DLL_PROCESS_DETACH:
				// 仅插件卸载时执行清理（NPP关闭时不执行，避免销毁面板）
				// 插件卸载由NPP主动触发，PROCESS_DETACH区分：卸载时g_sshPanels已空，关闭时非空
				if (g_sshPanels.empty()) {
					DeletePanelCountFromIni(); // 卸载时删除INI配置
					pluginCleanUp();
				}
				//pluginCleanUp();
				break;

			case DLL_THREAD_ATTACH:
				break;

			case DLL_THREAD_DETACH:
				break;
		}
	}
	catch (...) { 
		// 捕获异常，避免插件崩溃导致Notepad++退出
		::MessageBox(NULL, _T("插件初始化/清理异常！"), NPP_PLUGIN_NAME, MB_ICONERROR);
		return FALSE;
	}

    return TRUE;
}


extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
	
	g_nppData = notpadPlusData;
	nppData = notpadPlusData;
	commandMenuInit();
	
	RecreatePanelsOnNppStart();// NPP插件环境初始化完成后，自动重建配置中记录的面板
}

extern "C" __declspec(dllexport) const TCHAR * getName()
{
	return NPP_PLUGIN_NAME;
	//return TEXT("NppSSH");
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF)
{
	*nbF = nbFunc;
	return funcItem;
}


extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
	if (!notifyCode) return; // 空指针防护
	switch (notifyCode->nmhdr.code) 
	{
		case NPPN_SHUTDOWN:
		{
			// 检查活跃连接并提示
			bool hasActiveConnection = false;
			for (auto* panel : g_sshPanels) {
				if (panel && panel->isSSHConnected()) {
					hasActiveConnection = true;
					break;
				}
			}
			if (hasActiveConnection) {
				::MessageBoxW(
					NULL,
					L"您有已登录的SSH连接，关闭notepad++则直接退出关于SSH的所有登录",
					L"NppSSH 连接提示",
					MB_OK | MB_ICONWARNING
				);
			}
			// 统一断开所有SSH连接
			for (auto* panel : g_sshPanels) {
				if (panel) {
					panel->resetPanelToInit(); // 断开连接+恢复初始文本
				}
			}
			break;
		}

		// 新增：监听工具栏图标大小变化（自动适配）
		//case NPPN_TOOLBARICONSETCHANGED:
		//{
		//	// 遍历所有面板，更新按钮图标大小
		//	std::vector<NppSSHDockPanel*>& panels = SSHPanel_GetGlobalPanels();
		//	for (auto* panel : panels) {
		//		if (panel && panel->getHSelf() && IsWindow(panel->getHSelf())) {
		//			panel->UpdateToolbarIconSize();
		//		}
		//	}
		//	break;
		//}
		// 
		//case NPPN_TOOLBARICONSETCHANGED:
		//{
		//	// 声明全局面板容器（如果已存在可省略）
		//	extern std::vector<NppSSHDockPanel*>& SSHPanel_GetGlobalPanels();
		//	auto& panels = SSHPanel_GetGlobalPanels();
		//	for (auto* pPanel : panels)
		//	{
		//		if (pPanel != nullptr && pPanel->getHSelf() != nullptr && ::IsWindow(pPanel->getHSelf()))
		//		{
		//			pPanel->UpdateToolbarIconSize();
		//		}
		//	}
		//	break;
		//}

		// 关键：监听工具栏图标大小变化通知
		case NPPN_TOOLBARICONSETCHANGED:
		{
			// 遍历所有面板，更新按钮尺寸
			std::vector<NppSSHDockPanel*>& panels = SSHPanel_GetGlobalPanels();
			for (auto* panel : panels) {
				if (panel != nullptr && panel->getHSelf() != nullptr && ::IsWindow(panel->getHSelf())) {
					panel->UpdateToolbarIconSize();
				}
			}
			break;
		}
		default:
			return;
	}
}


// Here you can process the Npp Messages 
// I will make the messages accessible little by little, according to the need of plugin development.
// Please let me know if you need to access to some messages :
// https://github.com/notepad-plus-plus/notepad-plus-plus/issues
//
extern "C" __declspec(dllexport) LRESULT messageProc(UINT /*Message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{/*
	if (Message == WM_MOVE)
	{
		::MessageBox(NULL, "move", "", MB_OK);
	}
*/
	return TRUE;
}

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode()
{
    return TRUE;
}
#endif //UNICODE
