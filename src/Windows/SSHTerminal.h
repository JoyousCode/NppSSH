// SSHTerminal.h模拟终端的头文件，只做声明
#pragma once
#define _WIN32_WINNT 0x0A00
#include "SSHWindow.h"
#include <shlwapi.h>
#include <algorithm>


#include <windows.h>
#include <consoleapi2.h>
#include <processenv.h>

class SSHTerminal : public DockingDlgInterface {
public:
    SSHTerminal();
    ~SSHTerminal() = default;
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    // 初始化终端编辑框（迁移自initPanel的编辑框创建逻辑）
    HWND InitTerminalEditBox(HWND hParent);
    void disConnection();
    void resetSSHTerminal();
    void SizeSSHTerminal(HWND hParent);

    HWND GetOutputEditHandle();

    // 输出文本到输出框（迁移自AppendOutputText）
    void AppendOutputText(const std::string& text);

    // 更新提示符并锁定光标位置（迁移自UpdatePrompt）
    void UpdatePrompt(const std::wstring& prompt);

    // 检查光标位置是否合法（迁移自IsCursorInEditableArea）
    bool IsCursorInEditableArea();

    // 强制将光标移到可编辑区域末尾（迁移自ForceCursorToEditableEnd）
    void ForceCursorToEditableEnd();

    // 获取/设置回车要执行的命令（迁移自cmd）
    void SetCmd(const char* cmdStr);
    const char* GetCmd() const;

    // 获取/设置命令提示符（迁移自Prompt）
    void SetPrompt(const std::string& promptStr);
    const std::string& GetPrompt() const;

    // 获取编辑框句柄
    HWND GetEditBoxHwnd() const { return _hEditBox; }

private:
    HWND _hOutputEdit;

    int _panelId;
    HWND _hEditBox = nullptr;          // 终端编辑框句柄
    std::wstring _promptText;          // 存储当前命令提示符（迁移自_promptText）
    DWORD _promptEndPos = 0;           // 命令提示符结束位置（迁移自_promptEndPos）
    const char* _cmd = "";             // 回车需要执行的命令（迁移自cmd）
    std::string _prompt;               // 命令提示符（迁移自Prompt）
};

HWND SSHTerminal_InitTerminalEditBox(HWND hParent);
void SSHTerminal_disconnectTerminalEditBox(int panelIndex);
void SSHTerminal_resetSSHTerminal(int panelIndex);
void SSHTerminal_SizeSSHTerminal(HWND hParent,int panelIndex);


void SSHTerminal_AppendOutput(int panelIndex, const std::string& text);
SSHTerminal* getSSHTerminal(int panelIndex);