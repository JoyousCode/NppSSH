// SSHTerminal.h模拟终端的头文件，只做声明
#pragma once
#define _WIN32_WINNT 0x0A00
#include "SSHWindow.h"
#include <shlwapi.h>
#include <algorithm>
#include <windows.h>
#include <consoleapi2.h>
#include <processenv.h>

class SSHTerminal {
public:
    SSHTerminal();
    ~SSHTerminal();
    // 子类化终端编辑框过程监听
    static LRESULT CALLBACK TerminalEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    // 初始化终端编辑框（迁移自initPanel的编辑框创建逻辑）
    HWND InitTerminalEditBox(HWND hParent);
    void disConnection();
    void resetSSHTerminal();
    void SizeSSHTerminal(HWND hParent);

    HWND Get_TerminalHandle() const;

    // 输出文本到输出框（迁移自AppendOutputText）
    void AppendOutputText(const std::string& text);

    // 检查光标位置是否合法（迁移自IsCursorInEditableArea）
    bool IsCursorInEditableArea() const;

    // 获取/设置回车要执行的命令（迁移自cmd）
    void SetCmd(const char* cmdStr);
    const char* GetCmd() const;

    // 获取/设置命令提示符（迁移自Prompt）
    void SetPrompt(const std::string promptStr);
    const std::string& GetPrompt() const;

    // 获取编辑框句柄
    int GetPanelId() const { return _panelId; }
    void SetPanelId(int panelId) { _panelId = panelId; }
    WNDPROC GetOldEditProc() const { return _oldEditProc; }
    void SetIsCommandRunning(bool isCommandRunning) {_isCommandRunning = isCommandRunning;}
    const bool GetIsCommandRunning() const {return _isCommandRunning;}
    HWND Get_hwndParent() const { return _hwndParent; }

private:
    HWND _hTerminal;
    HWND _hwndParent = nullptr;
    bool _initialized = false;

    int _panelId;
    std::string _cmd;             // 回车需要执行的命令（迁移自cmd）
    std::string _prompt;               // 命令提示符（迁移自Prompt）
    WNDPROC _oldEditProc = nullptr; // 传统子类化保存旧过程
    bool _isCommandRunning = false; // 标记后台命令是否正在执行
};

HWND SSHTerminal_InitTerminalEditBox(HWND hParent,int panelId);
void SSHTerminal_disconnectTerminalEditBox(int panelIndex);
void SSHTerminal_resetSSHTerminal(int panelIndex);
void SSHTerminal_SizeSSHTerminal(HWND hParent,int panelIndex);


void SSHTerminal_AppendOutput(int panelIndex, const std::string& text);
void SSHTerminal_PanelPrompt(int panelIndex, std::string prompt);
void SSHTerminal_SetIsCommandRunning(int panelIndex, bool isCommandRunning);
void SSHTerminal_SetEnglishType(int panelIndex);
std::string SSHTerminal_getPanelPrompt(int panelIndex);

SSHTerminal* getSSHTerminal(int panelIndex);

// 工具函数声明（日志专用）
inline std::wstring GBKToWstring(const std::string& gbkStr);
inline std::string PtrToHexStr(void* ptr);
inline std::string IntToStr(int num);
