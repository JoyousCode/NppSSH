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

//#include "PluginDefinition.h"
#include "SSHClient.h" 
//#include <SSHClient.cpp>

extern FuncItem funcItem[nbFunc];
extern NppData nppData;
extern NppData g_nppData;
extern HINSTANCE g_hInst;


BOOL APIENTRY DllMain(HANDLE hModule, DWORD  reasonForCall, LPVOID /*lpReserved*/)
{
	try {

		switch (reasonForCall)
		{
			case DLL_PROCESS_ATTACH:
				//::MessageBox(NULL, TEXT("NppSSH 插件初始化!"), TEXT("NppSSH提示"), MB_OK);
				// 捕获异常，避免插件崩溃导致Notepad++退出
				//::MessageBox(NULL, _T("NPPSSH插件初始化！"), NPP_PLUGIN_NAME, MB_ICONERROR);
				// 插件被加载时：初始化DLL实例句柄 + 插件核心逻辑
				g_hInst = (HINSTANCE)hModule;
				pluginInit(hModule);
				break;

			case DLL_PROCESS_DETACH:
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
	// 同样显示成功时的句柄值，方便对比
	wchar_t szSuccess[256] = { 0 };
	swprintf_s(szSuccess, 256,
		L"Notepad++插件环境已经初始化！\n\n"
		L"g_nppData._nppHandle = %p\n"
		L"g_hInst = %p",
		g_nppData._nppHandle,
		g_hInst);

	::MessageBoxW(NULL, szSuccess, L"NppSSH初始化g_nppData提示", MB_OK | MB_ICONINFORMATION);
	g_nppData = notpadPlusData;
	// 同样显示成功时的句柄值，方便对比
	wchar_t szSuccessMsg[256] = { 0 };
	swprintf_s(szSuccessMsg, 256,
		L"Notepad++插件环境已经初始化！\n\n"
		L"g_nppData._nppHandle = %p\n"
		L"g_hInst = %p",
		g_nppData._nppHandle,
		g_hInst);

	::MessageBoxW(NULL, szSuccessMsg, L"NppSSH初始化g_nppData提示", MB_OK | MB_ICONINFORMATION);
	nppData = notpadPlusData;
	commandMenuInit();
}

extern "C" __declspec(dllexport) const TCHAR * getName()
{
	return NPP_PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF)
{
	*nbF = nbFunc;
	return funcItem;
}


extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
	switch (notifyCode->nmhdr.code) 
	{
		case NPPN_SHUTDOWN:
		{
			//commandMenuCleanUp();// 注释掉这行，避免NPP关闭时清理面板窗口
			// 检查是否有任何面板处于SSH已连接状态
			bool hasActiveConnection = false;
			for (auto* panel : g_sshPanels) {
				if (panel && panel->isSSHConnected()) {
					hasActiveConnection = true;
					break;
				}
			}

			// 有连接则弹出提示
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
					panel->disconnectSSH();
				}
			}

			// 清理插件资源
			pluginCleanUp();
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
