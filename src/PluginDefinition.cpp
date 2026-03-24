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

#include "PluginDefinition.h"
#include "menuCmdID.h"
#include <Windows.h>
#include <libssh2.h> 
#include <tchar.h>

#include <winsock2.h>
#include <ws2tcpip.h>
LIBSSH2_SESSION* g_sshSession = nullptr;
SOCKET g_sshSocket = INVALID_SOCKET;
bool g_isConnected = false;
//控件ID定义
#define IDC_HOST 1001
#define IDC_PORT 1002
#define IDC_USER 1003
#define IDC_PASS 1004
#define IDC_BTN_CONNECT 1005
// 窗口句柄
HWND hHost, hPort, hUser, hPass;



//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE /*hModule*/)
{
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

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
    setCommand(2, TEXT("NppSSH"), onNppSSH, NULL, false);
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
bool SSH_Connect(const char* host, int port, const char* user, const char* pass)
{
    g_isConnected = false;
    g_sshSession = nullptr;
    g_sshSocket = INVALID_SOCKET;

    TCHAR szPort[16];
    _stprintf_s(szPort, _T("%d"), port);
    ::MessageBox(NULL, szPort, TEXT("NppSSHRel定义"), MB_OK);
    
    //开始连接操作
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return false; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    TCHAR buf[64];
    wsprintf(buf, TEXT("端口(网络序): %d\n端口(真实端口): %d"),
        addr.sin_port,            // 网络字节序（数字很大）
        ntohs(addr.sin_port));    // 真实端口（22）
    ::MessageBox(NULL, buf, TEXT("NppSSH端口"), MB_OK);
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError(); // 获取具体错误码
        TCHAR errBuf[128];
        wsprintf(errBuf, TEXT("SOCKET_ERROR, 错误码: %d"), err);
        ::MessageBox(NULL, errBuf, TEXT("NppSSH提示"), MB_OK);
        ::MessageBox(NULL, TEXT("10060：连接超时::服务器无响应-防火墙拦截\n10061：连接被拒绝::服务器端口未开放\n10065：无路由到主机::网络不可达"),TEXT("NppSSH提示"), MB_OK);
        closesocket(sock); WSACleanup(); return false;
    }
    if (libssh2_init(0) != 0) { 
        ::MessageBox(NULL, TEXT("libssh2_init_ERROR"), TEXT("NppSSH提示"), MB_OK);
        closesocket(sock);
        WSACleanup();
        return false; 
    }
    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) { 
        ::MessageBox(NULL, TEXT("libssh2_session_init_ERROR"), TEXT("NppSSH提示"), MB_OK);
        libssh2_exit(); 
        closesocket(sock);
        WSACleanup(); 
        return false; 
    }
    libssh2_session_set_blocking(session, 1);
    if (libssh2_session_handshake(session, sock) != 0)
    {
        ::MessageBox(NULL, TEXT("libssh2_session_handshake_ERROR"), TEXT("NppSSH提示"), MB_OK);
        libssh2_session_free(session); libssh2_exit(); closesocket(sock); WSACleanup(); return false;
    }
    if (libssh2_userauth_password(session, user, pass) != 0)
    {
        libssh2_session_disconnect(session, "Auth Fail");
        ::MessageBox(NULL, TEXT("Auth Fail_ERROR!\n用户名或者密码错误！"), TEXT("NppSSH提示"), MB_OK);

        libssh2_session_free(session);
        libssh2_exit();
        closesocket(sock);
        WSACleanup();
        return false;
    }
    g_sshSocket = sock;
    g_sshSession = session;
    g_isConnected = true;
    return true;

}

void OnConnect(HWND hWnd) {
    char host[256] = { 0 };
    char port[32] = { 0 };
    char user[256] = { 0 };
    char pass[256] = { 0 };

    GetWindowTextA(hHost, host, 256);
    GetWindowTextA(hPort, port, 32);
    GetWindowTextA(hUser, user, 256);
    GetWindowTextA(hPass, pass, 256);
    int nPort = atoi(port);
    bool ok = SSH_Connect(host, nPort, user, pass);
    // 关闭窗口
    DestroyWindow(hWnd);
    // 结果提示（用Unicode避免乱码）
    if (ok)
        MessageBoxW(NULL, L"SSH 连接成功 ✅", L"NppSSH提示", MB_OK);
    else
        MessageBoxW(NULL, L"SSH 连接失败 ❌", L"NppSSH提示", MB_OK);
}

LRESULT CALLBACK SSH_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        // 标签（Unicode版，中文正常显示）
        CreateWindowW(L"STATIC", L"主机:", WS_VISIBLE | WS_CHILD, 20, 20, 50, 20, hWnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"端口:", WS_VISIBLE | WS_CHILD, 20, 50, 50, 20, hWnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"用户:", WS_VISIBLE | WS_CHILD, 20, 80, 50, 20, hWnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"密码:", WS_VISIBLE | WS_CHILD, 20, 110, 50, 20, hWnd, NULL, NULL, NULL);

        // 输入框
        hHost = CreateWindowA("EDIT", "36.33.27.234", WS_VISIBLE | WS_CHILD | WS_BORDER, 70, 20, 200, 25, hWnd, (HMENU)IDC_HOST, NULL, NULL);
        hPort = CreateWindowA("EDIT", "22", WS_VISIBLE | WS_CHILD | WS_BORDER, 70, 50, 200, 25, hWnd, (HMENU)IDC_PORT, NULL, NULL);
        hUser = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 70, 80, 200, 25, hWnd, (HMENU)IDC_USER, NULL, NULL);
        hPass = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD, 70, 110, 200, 25, hWnd, (HMENU)IDC_PASS, NULL, NULL);

        // 连接按钮（Unicode中文）
        CreateWindowW(L"BUTTON", L"确认连接", WS_VISIBLE | WS_CHILD, 70, 150, 80, 30, hWnd, (HMENU)IDC_BTN_CONNECT, NULL, NULL);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_CONNECT)
            OnConnect(hWnd);
            //::MessageBox(NULL, TEXT("OnConnect(hWnd)!"), TEXT("SSH提示"), MB_OK);
        return 0;

    case WM_DESTROY:
        //PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}


void onNppSSH()
{
    ::MessageBox(NULL, TEXT("NppSSH 连接!"), TEXT("提示"), MB_OK);

    //const char* CLASS_NAME = "NppSSHLoginWindow";
    const wchar_t* CLASS_NAME = L"NppSSHLoginWindow";


    //// 注册窗口类
    //WNDCLASSA wc{};
    // 注册窗口类（Unicode版）
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = SSH_WndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassExW(&wc);

    // 窗口尺寸
    int winW = 300;
    int winH = 240;
    // 计算屏幕居中坐标
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;
    // 创建窗口（Unicode标题 + 居中位置）
    HWND hWnd = CreateWindowExW(
        0, CLASS_NAME, L"NppSSH 连接",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, winW, winH,
        NULL, NULL, GetModuleHandleA(NULL), NULL
    );
    // 模态循环
    MSG msg;
    while (IsWindowVisible(hWnd) && GetMessageA(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }







    int rel = libssh2_init(0);
    TCHAR szRel[16];
    _stprintf_s(szRel, _T("%d"), rel);
    ::MessageBox(NULL, szRel, TEXT("NppSSHRel定义"), MB_OK);
    ::MessageBox(NULL, TEXT("NppSSH 连接!"), TEXT("NppSSH提示"), MB_OK);
}
