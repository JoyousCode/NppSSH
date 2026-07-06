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
#include "Windows/SSHLog.h"

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
				//if (g_SSHPanelSeqIdMap.empty()) {
				//	NppSSH_LogInfoAuto("卸载时删除INI配置");
				//	SSH_SettingsDeleteFile(); // 卸载时删除INI配置
				//	pluginCleanUp();
				//}
				
			
				pluginCleanUp();
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
	SSHLog_Init();
	SSH_SettingsInitRecreatePanels();// NPP插件环境初始化完成后，自动重建配置中记录的面板
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
			pluginCleanUp();
			break;
		}

		// 监听工具栏图标大小变化通知
		case NPPN_TOOLBARICONSETCHANGED:
		{
			// 遍历所有面板，更新按钮尺寸
			for (auto* panel : g_SSHPanelVec) {
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
	//bool isOk = WM_SETFONT || WM_INITDIALOG || WM_GETDLGCODE || WM_KILLFOCUS || WM_IME_SETCONTEXT || WM_SETFOCUS;
	char mbuf[64] = { 0 };
	sprintf(mbuf, "0x%04X", Message);
	std::string msgStr(mbuf);
	NppSSH_LogInfoAuto("【拦截PanelSubclassWndProc】消息message===" + msgStr);
	if (Message == WM_MOVE)
	{
		::MessageBox(NULL, L"WM_CLOSE", L"消息", MB_OK);
	}

	return TRUE;
*/
	return TRUE;
}

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode()
{
    return TRUE;
}
#endif //UNICODE
