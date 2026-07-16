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
#include <gdiplus.h>
using namespace Gdiplus;

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

ULONG_PTR g_gdiToken = 0;

void InitGDIPlus()
{
    if (g_gdiToken != 0) return;
    GdiplusStartupInput input;
    input.GdiplusVersion = 1;
    input.SuppressBackgroundThread = FALSE;
    GdiplusStartup(&g_gdiToken, &input, nullptr);
}
void UninitGDIPlus()
{
    if (g_gdiToken)
    {
        GdiplusShutdown(g_gdiToken);
        g_gdiToken = 0;
    }
}
//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE hModule)
{
    InitGDIPlus(); // NPP加载插件时，启动GDI+
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
    SSH_HandAllFree();
    UninitGDIPlus(); // NPP关闭/卸载插件，释放GDI+
}
ShortcutKey SSHTermPanel_OpenPanelShortcut = {
        true,   // _isCtrl
        false,   // _isAlt
        true,  // _isShift
        'G'     // _key
};
ShortcutKey SSHAppPanel_OpenPanelShortcut = {
        true,   // _isCtrl
        false,   // _isAlt
        true,  // _isShift
        'H'     // _key
};
//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{

    //--------------------------------------------//
    //-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
    //--------------------------------------------//
    // with function :
    // setCommand(int index,                      // zero based number to indicate the order of command
    //            TCHAR *commandName,             // the command name that you want to see in plugin menu
    //            PFUNCPLUGINCMD functionPointer, // the symbol of function (function pointer) associated with this command. The body should be defined below. See Step 4.
    //            ShortcutKey *shortcut,          // optional. Define a shortcut to trigger this command
    //            bool check0nInit                // optional. Make this menu item be checked visually
    //            );
    setCommand(0, TEXT("Hello Notepad++"), hello, NULL, false);
    setCommand(1, TEXT("Hello (with dialog)"), helloDlg, NULL, false);
    setCommand(2, TEXT("测试连接状态"), onNppSSH, NULL, false);
    setCommand(3, TEXT("创建终端面板"), onNppSSHTermPanel, &SSHTermPanel_OpenPanelShortcut, false);
    setCommand(4, TEXT("创建软件面板"), onNppSSHAppPanel, &SSHAppPanel_OpenPanelShortcut, false);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}


//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit) 
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}

//----------------------------------------------//
//-- STEP 4. DEFINE YOUR ASSOCIATED FUNCTIONS --//
//----------------------------------------------//
void hello()
{
    // Open a new document
    ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);

    // Get the current scintilla
    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    if (which == -1)
        return;
    HWND curScintilla = (which == 0)?nppData._scintillaMainHandle:nppData._scintillaSecondHandle;

    // Say hello now :
    // Scintilla control has no Unicode mode, so we use (char *) here
    ::SendMessage(curScintilla, SCI_SETTEXT, 0, (LPARAM)"Hello, Notepad++!");
}

void helloDlg()
{
    ::MessageBox(NULL, TEXT("Hello, Notepad++!"), TEXT("Notepad++ Plugin Template"), MB_OK);
}

void onNppSSH()
{
    bool ok = SSH_ConnectionIsConn(1);
    if (ok)
        ::MessageBoxW(NULL, L"SSH 连接成功 ✅", L"NppSSH提示", MB_OK);
    else
        ::MessageBoxW(NULL, L"SSH 连接失败 ❌", L"NppSSH提示", MB_OK);
}

void onNppSSHTermPanel()
{
    CreateNppSSHTermPanel();  // 调用 SSHClient.cpp 里的实现
}
void onNppSSHAppPanel()
{
    CreateNppSSHAppPanel();
}

