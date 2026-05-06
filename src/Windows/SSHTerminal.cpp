// SSHTerminal.cpp模拟终端，具体实现
#include "SSHTerminal.h"

static std::vector<SSHTerminal*> vectorSSHTerminal;
static NppData s_nppData;
static HINSTANCE s_hInst;


// 编码转换工具（自动识别 UTF8 / GBK，彻底解决Windows弹框乱码）
inline std::wstring GBKToWstring(const std::string& str) {
    if (str.empty())
        return L"";

    wchar_t buf[1024] = { 0 };

    // 1. 优先按 UTF-8 转换（libssh2 错误信息都是 UTF-8）
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len > 0 && len < 1024) {
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf, len);
        return buf;
    }

    // 2. 失败则使用 GBK（系统本地编码）
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buf, _countof(buf));
    return buf;
}
SSHTerminal::SSHTerminal() {
    // 初始化成员变量（避免野指针）
    _hSelf = nullptr;
    _hOutputEdit = nullptr;
    _promptEndPos = 0;
    _cmd = nullptr; // 或根据实际类型初始化，比如空字符串
}
HWND SSHTerminal::GetOutputEditHandle() {
    return _hOutputEdit;
}
HWND SSHTerminal::InitTerminalEditBox(HWND hParent) {
    
    if (!::IsWindow(hParent)) {
        ::MessageBoxW(s_nppData._nppHandle, L"SSH_InitTerminalEditBox: 面板窗口句柄无效！", L"NppSSH调试提示", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    //获取编辑框
    _hOutputEdit = ::GetDlgItem(hParent, IDC_OUTPUT_EDIT);
    if (!_hOutputEdit) {
        ::MessageBoxW(s_nppData._nppHandle, L"SSH_InitTerminalEditBox: 编辑框句柄无效！", L"NppSSH调试提示", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    ::SetWindowTextW(_hOutputEdit, L"✅ NppSSH面板已创建\r\n等待SSH连接..._hOutputEdit");
    // 设置样式
    DWORD style = ::GetWindowLongPtrW(_hOutputEdit, GWL_STYLE);
    style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL;
    ::SetWindowLongPtrW(_hOutputEdit, GWL_STYLE, style);

    // 调整输出编辑框位置，避开顶部按钮栏
    //RECT rcClient;
    //::GetClientRect(hParent, &rcClient);

    //::SetWindowPos( //编辑框
    //    _hOutputEdit,
    //    HWND_TOP,
    //    5,                  // 左边距
    //    iconSize+ 12,     // 上边距（避开按钮栏）
    //    rcClient.right - 10,// 宽度（自适应面板）
    //    rcClient.bottom - 50,// 高度（预留底部空间）
    //    SWP_NOZORDER | SWP_NOACTIVATE
    //);

    //// 刷新
    //::RedrawWindow(_hOutputEdit, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    SizeSSHTerminal(hParent);

    vectorSSHTerminal.push_back(this);
    MessageBoxW(s_nppData._nppHandle, L"终端编辑框初始化完成 ✅", L"成功", MB_OK);
    return _hOutputEdit;
}
/*
*断开连接，改变终端内容
*/
void SSHTerminal::disConnection() {
    if (_hOutputEdit) {
        //DisconnectPanel(this -> _panelId);// 面板断开SSH函数
        ::SetWindowTextW(_hOutputEdit, L"✅ SSH已断开\n等待新的连接...");
    }
}
/*
* 重置终端内容
*/
void SSHTerminal::resetSSHTerminal() {
    if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
        ::SetWindowTextW(_hOutputEdit, L"🔌 SSH已断开\r\n等待新的连接...resetPanelToInit");
        ::SendMessage(_hOutputEdit, EM_SETREADONLY, TRUE, 0);
    }
}
/*
* 设置终端面板大小
*/
void SSHTerminal::SizeSSHTerminal(HWND hParent) {//hParent=面板的_hSelf
    //::MessageBoxW(s_nppData._nppHandle, L"SizeSSHTerminal", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);

    //_hOutputEdit = ::GetDlgItem(hParent, IDC_OUTPUT_EDIT);
    RECT rc;
    ::GetClientRect(hParent, &rc);
    ::SetWindowPos(
        _hOutputEdit,
        HWND_TOP,
        5, iconSize + 12, rc.right - 10, rc.bottom - 50,
        SWP_NOZORDER | SWP_NOACTIVATE
    );

    ::RedrawWindow(_hOutputEdit, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}


void SSHTerminal::AppendOutputText(const std::string& text) {
    // 迁移自SSHPanel::AppendOutputText的原有逻辑
    NppSSH_LogInfoAuto("输出文本到输出框" + std::string(text));
    if (!_hOutputEdit) return;
    std::wstring wtext = GBKToWstring(text);

    //// 3. 追加文本（只读控件临时取消只读）
    ::SendMessage(_hOutputEdit, EM_SETREADONLY, FALSE, 0);
    //// 光标移到末尾，追加文本
    int len = ::GetWindowTextLengthW(_hOutputEdit);
    ::SendMessage(_hOutputEdit, EM_SETSEL, len, len);
    ::SendMessage(_hOutputEdit, EM_REPLACESEL, FALSE, (LPARAM)wtext.c_str());
    //// 恢复只读
    ::SendMessage(_hOutputEdit, EM_SETREADONLY, TRUE, 0);

    // 3. 关键：如果输出包含命令提示符（如 [root@host ~]# ），更新锁定位置
    if (wtext.find(L"]# ") != wtext.npos || wtext.find(L"]$ ") != wtext.npos) {
        UpdatePrompt(wtext.substr(wtext.find_last_of(L"]#$") - 1));
    }
    else {
        // 普通输出，更新提示符结束位置为文本末尾
        _promptEndPos = ::GetWindowTextLengthW(_hOutputEdit);
    }
    // 4. 强制光标到可编辑区域
    ForceCursorToEditableEnd();

    //自动滚动到底部
    DWORD len_total = ::GetWindowTextLengthW(_hOutputEdit);
    ::SendMessageW(_hOutputEdit, EM_SETSEL, len_total, len_total);
    ::SendMessageW(_hOutputEdit, EM_SCROLLCARET, 0, 0);
    NppSSH_LogInfoAuto("文本追加完成，当前输出框总长度：" + std::to_string(len_total)
        + "，追加长度：" + std::to_string(text.length()));
}

void SSHTerminal::UpdatePrompt(const std::wstring& prompt) {
    // 迁移自SSHPanel::UpdatePrompt的原有逻辑
    if (!_hOutputEdit || !::IsWindow(_hOutputEdit)) return;

    // 1. 记录提示符文本和结束位置
    _promptText = prompt;
    // 2. 获取当前文本长度 = 提示符结束位置
    _promptEndPos = ::GetWindowTextLengthW(_hOutputEdit);
    // 3. 强制光标移到提示符末尾
    ForceCursorToEditableEnd();
}

bool SSHTerminal::IsCursorInEditableArea() {
    // 迁移自SSHPanel::IsCursorInEditableArea的原有逻辑
    if (!_hOutputEdit || !::IsWindow(_hOutputEdit)) return false;

    DWORD startPos = 0, endPos = 0;
    ::SendMessageW(_hOutputEdit, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
    // 光标起始位置 >= 提示符结束位置 → 合法
    return (startPos >= _promptEndPos) && (endPos >= _promptEndPos);
}

void SSHTerminal::ForceCursorToEditableEnd() {
    // 迁移自SSHPanel::ForceCursorToEditableEnd的原有逻辑
    if (!_hOutputEdit || !::IsWindow(_hOutputEdit)) return;

    DWORD totalLen = ::GetWindowTextLengthW(_hOutputEdit);
    // 仅允许光标在提示符后 → 锁定选择范围为 [_promptEndPos, totalLen]
    ::SendMessageW(_hOutputEdit, EM_SETSEL, _promptEndPos, totalLen);
    ::SendMessageW(_hOutputEdit, EM_SCROLLCARET, 0, 0); // 滚动到光标位置
    NppSSH_LogInfoAuto("强制移动到末尾" + std::to_string(totalLen) + "PanelPrompt=" + std::to_string(_promptEndPos));
}

void SSHTerminal::SetCmd(const char* cmdStr) {
    _cmd = cmdStr;
}

const char* SSHTerminal::GetCmd() const {
    return _cmd;
}

void SSHTerminal::SetPrompt(const std::string& promptStr) {
    _prompt = promptStr;
}

const std::string& SSHTerminal::GetPrompt() const {
    return _prompt;
}
INT_PTR CALLBACK SSHTerminal::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {      // 原生消息：面板窗口创建完成，初始化内部UI（输出文本框）
    case WM_INITDIALOG:
    {
        if (!_hSelf) {
            NppSSH_LogErrorAuto("面板窗口句柄无效！");
            ::MessageBox(NULL, TEXT("面板窗口句柄无效！"), TEXT("NppSSH错误提示"), MB_OK | MB_ICONERROR);
            return FALSE;
        }
        return TRUE;
    }
    // 面板大小变化时，自动适配输出文本框（防止遮挡/空白）（最小化关闭/打开notepad++会自动触发）
    case WM_SIZE:
    {
        ::MessageBoxW(s_nppData._nppHandle, L"SSH变化3", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
        //面板大小变化
        if (_hOutputEdit && ::IsWindow(_hOutputEdit)) {
            RECT rc;
            ::GetClientRect(_hSelf, &rc);
            // ====== 同步修改WM_SIZE中的编辑框位置，适配按钮栏 ======
            ::SetWindowPos(
                _hOutputEdit,
                NULL,
                5, 28 + 12, rc.right - 10, rc.bottom - 50,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
        }
        return TRUE;
    }
    // 处理按钮点击消息
    case WM_COMMAND:
    {
        break;
    }
    // 新增：拦截键盘消息（仅允许在合法区域输入）
    case WM_KEYDOWN: {
        break;
    }
                   // 响应NPP停靠管理器的浮动/停靠消息，更新面板状态
    case WM_NOTIFY:
    {
        break;
    }

    // 面板关闭：原生NPP消息，自动清理资源，无内存泄漏
    case WM_CLOSE:
    {
        auto it = std::find(vectorSSHTerminal.begin(), vectorSSHTerminal.end(), this);      // 从全局管理数组中移除
        if (it != vectorSSHTerminal.end()) vectorSSHTerminal.erase(it);

        // 原生停靠面板清理：先隐藏，再销毁
        this->display(false);
        ::DestroyWindow(_hSelf);
        this->destroy();
        delete this;                                // 释放面板实例
        return TRUE;
    }

    // 其他所有消息，交给DockingDlgInterface原生处理（避免NPP异常）
    default:
        return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
    }
    return TRUE;
}



HWND SSHTerminal_InitTerminalEditBox(HWND hParent) {
    SSHTerminal* _SSHTerminal = new SSHTerminal();
    return _SSHTerminal->InitTerminalEditBox(hParent);
}
void SSHTerminal_disconnectTerminalEditBox(int panelIndex) {
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    panel->disConnection();
}
void SSHTerminal_resetSSHTerminal(int panelIndex) {
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    panel->resetSSHTerminal();
}
void SSHTerminal_SizeSSHTerminal(HWND hParent,int panelIndex) {
    wchar_t szMsg[256] = { 0 };
    wsprintfW(szMsg, L"SSHTerminal_SizeSSHTerminal -> 面板序号：%d", panelIndex);
    //::MessageBoxW(s_nppData._nppHandle, szMsg, L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
    


    SSHTerminal* panel = getSSHTerminal(panelIndex);

    if (panel == nullptr) {
        ::MessageBoxW(s_nppData._nppHandle, L"SSHTerminal_SizeSSHTerminal: 面板指针为空！", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!::IsWindow(panel ->GetOutputEditHandle())) {
        ::MessageBoxW(s_nppData._nppHandle, L"SSHTerminal_SizeSSHTerminal: 编辑框句柄无效，跳过调整！", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    panel->SizeSSHTerminal(hParent);

}


void SSHTerminal_AppendOutput(int panelIndex, const std::string& text) {
    NppSSH_LogInfoAuto("输出指定面板" + std::to_string(panelIndex));
    NppSSH_LogInfoAuto("输出指定面板" + std::string(text));
    NppSSH_LogInfoAuto("输出指定面板" + std::to_string(vectorSSHTerminal.size()));

    if (panelIndex < 0) return;
    panelIndex = panelIndex - 1;
    SSHTerminal* panel = vectorSSHTerminal[panelIndex];
    if (!panel || !panel->GetOutputEditHandle())
        return;
    std::string fixedText;
    for (char c : text) {
        if (c == '\n') {
            // Linux \n → Windows \r\n
            fixedText += "\r\n";
        }
        else if (c != '\r') {
            fixedText += c;
        }
    }
    panel->AppendOutputText(fixedText);
}

/*
* 获取当前面板
*/
SSHTerminal* getSSHTerminal(int panelIndex) {
    // 直接使用普通字符串拼接，无宽字符，无转换
    char szDebugMsg[2048] = { 0 };
    sprintf_s(szDebugMsg, "面板大小改变 === vectorSSHTerminal 内容 ===\r\n总数：%d\r\n\r\n",
        (int)vectorSSHTerminal.size());

    // 遍历所有元素（直接拼接普通字符串）
    for (int i = 0; i < vectorSSHTerminal.size(); i++)
    {
        SSHTerminal* p = vectorSSHTerminal[i];
        char szLine[256] = { 0 };
        if (p == nullptr) sprintf_s(szLine, "[%d] 指针：空指针\r\n", i);
        else sprintf_s(szLine, "[%d] 指针：0x%p\r\n", i, p);

        // 普通字符串拼接
        strcat_s(szDebugMsg, szLine);
    }
    NppSSH_LogInfoAuto(szDebugMsg);

    if (panelIndex < 1) return nullptr;
    panelIndex = panelIndex - 1;
    return vectorSSHTerminal[panelIndex];
}
