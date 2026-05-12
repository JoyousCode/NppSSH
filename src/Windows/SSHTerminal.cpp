// SSHTerminal.cpp模拟终端，具体实现
#include "SSHTerminal.h"
static WNDPROC s_pOldEditProc = nullptr; // 保存 EDIT 原来的窗口过程
static std::vector<SSHTerminal*> vectorSSHTerminal;
static NppData s_nppData;
static HINSTANCE s_hInst;

// 新增：防重入标记（避免递归调用）
static thread_local bool s_bProcessingMsg = false;

// 传统编辑框子类化过程（解决消息拦截失效问题）
// ============return res = 0;拦截编辑器的操作，自定义具体操作。
// ============return res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);放行编辑器原始的操作，
LRESULT CALLBACK TerminalEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    bool bNeedProcess = true;
    switch (msg) {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
    case WM_PASTE:
    case WM_DEADCHAR:
    case WM_SYSKEYDOWN:
    case WM_SYSCHAR:
        bNeedProcess = true;
        break;
    default:
        bNeedProcess = false;
        break;
    }

    if (!bNeedProcess) {
        WNDPROC oldProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        return oldProc ? CallWindowProc(oldProc, hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam);
    }

    if (s_bProcessingMsg) {
        WNDPROC oldProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        return oldProc ? CallWindowProc(oldProc, hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam);
    }
    s_bProcessingMsg = true;

    LRESULT res = 0;
    try {
        NppSSH_LogInfoAuto("TerminalEditProc监听！msg=" + IntToStr(msg) + " hWnd=" + PtrToHexStr(hWnd));

        SSHTerminal* terminal = (SSHTerminal*)GetProp(hWnd, L"SSHTerminalInstance");
        if (!terminal) {
            for (auto& t : vectorSSHTerminal) {
                if (t && t->GetEditBoxHwnd() == hWnd) {
                    terminal = t;
                    SetProp(hWnd, L"SSHTerminalInstance", (HANDLE)terminal);
                    break;
                }
            }
        }

        if (!terminal) {
            NppSSH_LogInfoAuto("TerminalEditProc未找到终端！hWnd=" + PtrToHexStr(hWnd) + " msg=" + IntToStr(msg));
            WNDPROC oldProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            res = oldProc ? CallWindowProc(oldProc, hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam);
            s_bProcessingMsg = false;
            return res;
        }

        WNDPROC oldProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        if (!oldProc) {
            oldProc = DefWindowProc;
        }

        // 1. 全局放行复制操作（Ctrl+C / Ctrl+Insert）
        bool isCopy = (msg == WM_KEYDOWN &&
            ((GetKeyState(VK_CONTROL) < 0 && wParam == 'C') ||
                (GetKeyState(VK_CONTROL) < 0 && wParam == VK_INSERT)));
        if (isCopy) {
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            NppSSH_LogInfoAuto("【放行】全局复制操作！msg=" + IntToStr(msg) + " wParam=" + IntToStr(wParam));
            s_bProcessingMsg = false;
            return res;
        }

        // 3. 左右方向键放行（无控制）
        if (msg == WM_KEYDOWN && (wParam == VK_LEFT || wParam == VK_RIGHT)) {
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            NppSSH_LogInfoAuto("【放行】左右方向键操作！wParam=" + IntToStr(wParam));
            s_bProcessingMsg = false;
            return res;
        }

        // 4. 检查是否在可编辑区域
        bool canEdit = terminal->IsCursorInEditableArea();
        if (msg == WM_KEYDOWN && (wParam == VK_UP || wParam == VK_DOWN) && canEdit) {
            NppSSH_LogInfoAuto("调用远程服务器的历史记录，待实现去远程服务查询历史命令");// 待实现去远程服务查询历史命令
            NppSSH_LogInfoAuto("【拦截】上下方向键禁止操作！wParam=" + IntToStr(wParam));
            std::string result = NppSSH_ExecuteCommand(terminal -> GetPanelId(),"ls");
            SSHTerminal_AppendOutput(terminal -> GetPanelId(),"\r\n"+result,true);
            // ========== 追加内容后恢复光标和编辑状态 ==========
            HWND hEdit = terminal->GetEditBoxHwnd();
            if (IsWindow(hEdit))
            {
                // 强制模拟一次“失去焦点 + 重新获得焦点”，重置键盘输入状态
                SendMessage(hEdit, WM_KILLFOCUS, 0, 0);
                SendMessage(hEdit, WM_SETFOCUS, 0, 0);

                // 刷新光标显示
                //SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
            }
            // ======================================================

            res = 0;
            s_bProcessingMsg = false;
            return res;
        }
        // ==============================
        // ✅ 终极修复：同时拦截 WM_KEYDOWN + WM_CHAR 退格键
        // 禁止删除 prompt 末尾字符，其余操作全部正常
        // ==============================
        bool isBackspaceAtPromptEnd = false;
        if ((msg == WM_KEYDOWN && wParam == VK_BACK) || (msg == WM_CHAR && wParam == 8)) {
            DWORD selStart = 0;
            ::SendMessageW(hWnd, EM_GETSEL, (WPARAM)&selStart, NULL);
            DWORD cursorPos = selStart;

            std::wstring promptW = GBKToWstring(terminal->GetPrompt());
            int promptLen = (int)promptW.length();

            int totalLen = ::GetWindowTextLengthW(hWnd);
            std::wstring allText;
            allText.resize(totalLen + 1);
            ::GetWindowTextW(hWnd, &allText[0], totalLen + 1);

            size_t lineStart = 0;
            for (size_t i = cursorPos; i > 0; --i) {
                if (allText[i] == L'\n' || allText[i] == L'\r') {
                    lineStart = i + 1;
                    break;
                }
            }

            size_t promptEndPos = lineStart + promptLen;
            if (cursorPos == promptEndPos && canEdit) {
                isBackspaceAtPromptEnd = true;
            }
        }

        // 触发：禁止删除 prompt 末尾 → 直接拦截所有退格相关消息
        if (isBackspaceAtPromptEnd) {
            NppSSH_LogInfoAuto("【拦截】禁止删除prompt末尾字符！光标位置=" + IntToStr((int)::SendMessageW(hWnd, EM_GETSEL, 0, 0)));
            res = 0;
            s_bProcessingMsg = false;
            return res;
        }

        // 5. 处理删除键逻辑（原有逻辑不变）
        bool isDeleteKey = (msg == WM_KEYDOWN && (wParam == VK_BACK || wParam == VK_DELETE));
        if (isDeleteKey) {
            // 获取光标位置和前缀信息
            DWORD selStart = 0, selEnd = 0;
            ::SendMessageW(hWnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            DWORD cursorPos = selStart;
            std::string promptStr = terminal->GetPrompt();
            std::string cmdStr = terminal->GetCmd();
            std::wstring wPrompt = GBKToWstring(promptStr);
            int promptLen = (int)wPrompt.length();
            int cmdLen = (int)GBKToWstring(cmdStr).length();

            // 获取光标所在行完整信息
            int totalLen = ::GetWindowTextLengthW(hWnd);
            std::wstring allText;
            allText.resize(totalLen + 1);
            ::GetWindowTextW(hWnd, &allText[0], totalLen + 1);

            // 定位光标所在行的起始/结束位置
            size_t lineStart = 0;
            for (size_t i = cursorPos; i > 0; --i) {
                if (allText[i] == L'\n' || allText[i] == L'\r') {
                    lineStart = i + 1;
                    break;
                }
            }
            size_t lineEnd = allText.find_first_of(L"\r\n", cursorPos);
            if (lineEnd == std::wstring::npos) lineEnd = allText.length();
            std::wstring currentLine = allText.substr(lineStart, lineEnd - lineStart);

            // 原有删除逻辑
            size_t promptEndPosInLine = lineStart + promptLen;
            bool willModifyPrompt = false;

            if (wParam == VK_BACK) {
                willModifyPrompt = (cursorPos < promptEndPosInLine);
            }
            else if (wParam == VK_DELETE) {
                willModifyPrompt = (cursorPos < promptEndPosInLine);
            }

            if (willModifyPrompt) {
                NppSSH_LogInfoAuto("【拦截】删除操作将修改prompt区域，禁止删除！光标位置=" + IntToStr((int)cursorPos));
                res = 0;
                s_bProcessingMsg = false;
                return res;
            }

            // 核心判定2：仅prompt无命令时，禁止删除
            std::wstring cmdInLine = currentLine.substr(cmdLen);
            if (cmdLen == 0) {
                NppSSH_LogInfoAuto("【拦截】仅存在prompt无命令，禁止删除！");
                res = 0;
                s_bProcessingMsg = false;
                return res;
            }

            // 计算光标在命令中的位置，同步修改_cmd
            std::string currentCmd = terminal->GetCmd();
            int cursorInCmdPos = (int)(cursorPos - (lineStart + promptLen));
            bool isCmdModified = false;

            if (wParam == VK_BACK) {
                if (cursorInCmdPos > 0 && cursorInCmdPos <= (int)currentCmd.length()) {
                    currentCmd.erase(cursorInCmdPos - 1, 1);
                    isCmdModified = true;
                }
            }
            else if (wParam == VK_DELETE) {
                if (cursorInCmdPos < (int)currentCmd.length()) {
                    currentCmd.erase(cursorInCmdPos, 1);
                    isCmdModified = true;
                }
            }

            if (isCmdModified) {
                terminal->SetCmd(currentCmd.c_str());
                NppSSH_LogInfoAuto("【同步cmd】删除后：" + currentCmd);
            }

            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            NppSSH_LogInfoAuto("【放行】命令区域删除操作！wParam=" + IntToStr(wParam));
            s_bProcessingMsg = false;
            return res;
        }

        // 6. 处理字符输入（原有逻辑不变）
        if (msg == WM_CHAR && wParam >= 0x20 && wParam <= 0x7E) {
            if (!canEdit) {
                NppSSH_LogInfoAuto("【拦截】非可编辑区域，禁止字符输入！");
                res = 0;
            }
            else {
                DWORD selStart = 0, selEnd = 0;
                ::SendMessageW(hWnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
                DWORD cursorPos = selStart;
                std::string promptStr = terminal->GetPrompt();
                std::wstring wPrompt = GBKToWstring(promptStr);
                int promptLen = (int)wPrompt.length();

                int totalLen = ::GetWindowTextLengthW(hWnd);
                std::wstring allText;
                allText.resize(totalLen + 1);
                ::GetWindowTextW(hWnd, &allText[0], totalLen + 1);
                size_t lineStart = 0;
                for (size_t i = cursorPos; i > 0; --i) {
                    if (allText[i] == L'\n' || allText[i] == L'\r') {
                        lineStart = i + 1;
                        break;
                    }
                }

                int cursorInCmdPos = (int)(cursorPos - (lineStart + promptLen));
                std::string currentCmd = terminal->GetCmd();
                char c = (char)wParam;

                if (cursorInCmdPos <= (int)currentCmd.length()) {
                    currentCmd.insert(cursorInCmdPos, 1, c);
                    terminal->SetCmd(currentCmd.c_str());
                    NppSSH_LogInfoAuto("【同步cmd】插入字符：" + std::string(1, c));
                }

                res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
                NppSSH_LogInfoAuto("【放行】可编辑区域字符输入！");
            }
        }
        else if (!canEdit) {
            if (msg == WM_KEYDOWN || msg == WM_CHAR || msg == WM_KEYUP || msg == WM_PASTE ||
                msg == WM_DEADCHAR || msg == WM_SYSKEYDOWN || msg == WM_SYSCHAR) {
                NppSSH_LogInfoAuto("【拦截】非可编辑区域，禁止操作！msg=" + IntToStr(msg) + " wParam=" + IntToStr(wParam));
                res = 0;//拦截
            }
            else {
                res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);//放行
            }
        }
        else {
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            NppSSH_LogInfoAuto("【放行】可编辑区域合法操作！msg=" + IntToStr(msg) + " wParam=" + IntToStr(wParam));
        }

        NppSSH_LogInfoAuto("TerminalEditProc调用原过程！msg=" + IntToStr(msg) + " result=" + IntToStr((int)res));
    }
    catch (...) {
        NppSSH_LogInfoAuto("TerminalEditProc异常！msg=" + IntToStr(msg));
        res = 0;
    }

    s_bProcessingMsg = false;
    return res;
}
// 安全地把 std::wstring 转为 std::string 日志专用（避免乱码和异常）
inline std::string WStringToLogStr(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    std::string res;
    res.reserve(wstr.size());
    for (wchar_t wc : wstr) {
        if (wc <= 0x7F) { // 只打印ASCII字符，非ASCII直接替换为?
            res += static_cast<char>(wc);
        }
        else {
            res += '?';
        }
    }
    return res;
}
// 工具函数：指针转十六进制字符串（日志专用）
inline std::string PtrToHexStr(void* ptr) {
    char buf[32] = { 0 };
    sprintf_s(buf, "0x%p", ptr);
    return std::string(buf);
}

// 工具函数：数字转字符串（日志专用）
inline std::string IntToStr(int num) {
    return std::to_string(num);
}
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
    //_promptEndPos = 0;
    //_prompt = "[root@192.168.137.201 ~]# ";
    _cmd = ""; // 或根据实际类型初始化，比如空字符串
    _oldEditProc = nullptr;
}
// 析构函数：释放资源，防止内存泄漏
SSHTerminal::~SSHTerminal() {
    if (_hOutputEdit && _oldEditProc) {
        // 清理窗口属性（新增）
        RemoveProp(_hOutputEdit, L"SSHTerminalInstance");
        // 恢复原窗口过程（保留）
        SetWindowLongPtr(_hOutputEdit, GWLP_WNDPROC, (LONG_PTR)_oldEditProc);
        _oldEditProc = nullptr;
    }
    // 从vector移除自身（保留）
    auto it = std::find(vectorSSHTerminal.begin(), vectorSSHTerminal.end(), this);
    if (it != vectorSSHTerminal.end()) {
        vectorSSHTerminal.erase(it);
    }
}

HWND SSHTerminal::GetOutputEditHandle() {
    return _hOutputEdit;
}
HWND SSHTerminal::InitTerminalEditBox(HWND hParent) {
    
    if (!::IsWindow(hParent)) {
        ::MessageBoxW(s_nppData._nppHandle, L"SSH_InitTerminalEditBox: 面板窗口句柄无效！", L"NppSSH调试提示", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    // 保存父窗口（必须！解决 _hSelf 为空导致的崩溃）
    _hSelf = hParent;
    //获取编辑框
    _hOutputEdit = ::GetDlgItem(hParent, IDC_OUTPUT_EDIT);
    if (!_hOutputEdit) {
        ::MessageBoxW(s_nppData._nppHandle, L"SSH_InitTerminalEditBox: 编辑框句柄无效！", L"NppSSH调试提示", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    // 设置样式
    DWORD style = ::GetWindowLongPtrW(_hOutputEdit, GWL_STYLE);
    style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL;
    ::SetWindowLongPtrW(_hOutputEdit, GWL_STYLE, style);

    // 调整输出编辑框位置，避开顶部按钮栏
    SizeSSHTerminal(hParent);

    // ==== 挂载子类化 ====
    if (!_oldEditProc) {
        // 1. 获取原窗口过程
        _oldEditProc = (WNDPROC)GetWindowLongPtr(_hOutputEdit, GWLP_WNDPROC);
        if (!_oldEditProc) {
            _oldEditProc = DefWindowProc;
        }
        // 2. 保存原过程到GWLP_USERDATA（仅存原过程，避免偏移冲突）
        SetWindowLongPtr(_hOutputEdit, GWLP_USERDATA, (LONG_PTR)_oldEditProc);
        // 3. 用窗口属性存储终端实例（替代GWLP_USERDATA+偏移，避免越界）
        SetProp(_hOutputEdit, L"SSHTerminalInstance", (HANDLE)this);
        // 4. 设置新的窗口过程
        SetWindowLongPtr(_hOutputEdit, GWLP_WNDPROC, (LONG_PTR)TerminalEditProc);
        NppSSH_LogInfoAuto("编辑框子类化完成！hWnd=" + PtrToHexStr(_hOutputEdit)
            + " 原过程：" + PtrToHexStr(_oldEditProc)
            + " 新过程：" + PtrToHexStr(TerminalEditProc));
    }

    // 严格检查，避免重复添加终端实例到vector
    auto it = std::find(vectorSSHTerminal.begin(), vectorSSHTerminal.end(), this);
    if (it == vectorSSHTerminal.end()) {
        vectorSSHTerminal.push_back(this);
        NppSSH_LogInfoAuto("终端实例添加到vector，当前数量：" + std::to_string(vectorSSHTerminal.size()));
    }
    else {
        NppSSH_LogInfoAuto("终端实例已存在于vector，跳过添加");
    }

    //MessageBoxW(s_nppData._nppHandle, L"终端编辑框初始化完成 ✅", L"成功", MB_OK);
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
    if (!_hOutputEdit || !::IsWindow(_hOutputEdit))
        return;

    if (!::IsWindow(hParent))
        return;
    //::MessageBoxW(s_nppData._nppHandle, L"SizeSSHTerminal", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);

    //_hOutputEdit = ::GetDlgItem(hParent, IDC_OUTPUT_EDIT);
    RECT rc;
    if (!::GetClientRect(hParent, &rc))
        return;
    // 左边距
    const int LEFT = 5;
    // 上边距（避开按钮栏）
    const int TOP = iconSize + 12;
    // 右边距
    const int RIGHT = 10;
    // 底部边距
    const int BOTTOM = 10;

    int x = LEFT;
    int y = TOP;
    int cx = rc.right - LEFT - RIGHT;
    int cy = rc.bottom - TOP - BOTTOM;

    // 防止宽高为负数导致看不见
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    ::SetWindowPos(
        _hOutputEdit,
        HWND_TOP,
        x, y, cx, cy,
        SWP_NOZORDER | SWP_NOACTIVATE
    );
    //只重绘【终端编辑框】自己让编辑框立刻刷新、重新绘制自己的内容、文字、背景、边框。
    ::RedrawWindow(_hOutputEdit, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);// 刷新编辑框内容（防止文字不显示）
}

/*
* 追加编辑框终端模拟内容
*/
void SSHTerminal::AppendOutputText(const std::string& text) {
    // 空文本防护，避免非法字符串触发弹框
    if (text.empty() || !_hOutputEdit) {
        NppSSH_LogWarnAuto("AppendOutputText: 文本或编辑框为空");
        return;
    }
    // 迁移自SSHPanel::AppendOutputText的原有逻辑
    NppSSH_LogInfoAuto("输出文本到输出框" + std::string(text));
    if (!_hOutputEdit) return;
    std::wstring wtext = GBKToWstring(text);

    // 【优化6】临时关闭子类化，避免EM_SETSEL/EM_REPLACESEL触发循环
    WNDPROC tempOldProc = (WNDPROC)GetWindowLongPtr(_hOutputEdit, GWLP_WNDPROC);
    SetWindowLongPtr(_hOutputEdit, GWLP_WNDPROC, (LONG_PTR)_oldEditProc);
    //追加文本（只读控件临时取消只读）
    ::SendMessage(_hOutputEdit, EM_SETREADONLY, FALSE, 0);

    // 如果输出需要追加命令提示符（如 [root@host ~]# ），更新提示符具体的内容
    if (this->GetIsPrompt()) {
        std::string appendPrompt = this -> GetPrompt();
        wtext += GBKToWstring(appendPrompt);
        NppSSH_LogInfoAuto("面板==" + std::to_string(this->_panelId)
            + "追加的提示词=========" + appendPrompt);
    }
    


    // 光标移到末尾，追加文本
    int len = ::GetWindowTextLengthW(_hOutputEdit);
    ::SendMessage(_hOutputEdit, EM_SETSEL, len, len);
    ::SendMessage(_hOutputEdit, EM_REPLACESEL, FALSE, (LPARAM)wtext.c_str());
    //// 恢复只读
    ::SendMessage(_hOutputEdit, EM_SETREADONLY, TRUE, 0);

    // 恢复子类化
    SetWindowLongPtr(_hOutputEdit, GWLP_WNDPROC, (LONG_PTR)tempOldProc);


    // ========== 精准定位光标到新提示符末尾 ==========
    // 1. 重新获取追加后的总长度（避免wtext拼接导致的长度偏差）
    DWORD len_total = ::GetWindowTextLengthW(_hOutputEdit);
    // 2. 强制选中末尾（确保光标在最后）
    ::SendMessageW(_hOutputEdit, EM_SETSEL, len_total, len_total);
    // 3. 滚动到光标位置（视觉反馈）
    ::SendMessageW(_hOutputEdit, EM_SCROLLCARET, 0, 0);
    // 4. 确保编辑框保留焦点（关键：避免追加后焦点丢失）
    if (::GetFocus() != _hOutputEdit) {
        ::SetFocus(_hOutputEdit);
    }
    // ======================================================
    ////自动滚动到底部
    //DWORD len_total = ::GetWindowTextLengthW(_hOutputEdit);
    //::SendMessageW(_hOutputEdit, EM_SETSEL, len_total, len_total);
    //::SendMessageW(_hOutputEdit, EM_SCROLLCARET, 0, 0);
    NppSSH_LogInfoAuto("文本追加完成，当前输出框总长度：" + IntToStr((int)len_total)
        + "，追加长度：" + IntToStr((int)text.length()));
}


/*
* 控制键盘的输入操作
*/
bool SSHTerminal::IsCursorInEditableArea() {

    if (!_hOutputEdit || !::IsWindow(_hOutputEdit))
        return false;

    // 1. 获取光标位置
    DWORD selStart = 0;
    ::SendMessageW(_hOutputEdit, EM_GETSEL, (WPARAM)&selStart, NULL);
    DWORD cursorPos = selStart;

    // 2. 获取整行文本
    int totalLen = ::GetWindowTextLengthW(_hOutputEdit);
    std::wstring allText;
    allText.resize(totalLen + 1);
    ::GetWindowTextW(_hOutputEdit, &allText[0], totalLen + 1);

    // 3. 找光标所在行
    size_t lineStart = 0;
    for (size_t i = cursorPos; i > 0; --i) {
        if (allText[i] == L'\n' || allText[i] == L'\r') {
            lineStart = i + 1;
            break;
        }
    }

    // 4. 拿到当前行
    std::wstring currentLine = allText.substr(lineStart);

    // 5. 拿到原始 prompt（包含末尾空格）
    std::wstring promptW = GBKToWstring(_prompt);
    int promptLen = (int)promptW.length();

    // ==============================
    // 正常逻辑：支持 prompt 后任意位置编辑（输入/删除命令/光标移动）
    // 光标 >= prompt 结束位置 = 允许正常编辑
    // ==============================
    bool lineStartsWithPrompt = (currentLine.substr(0, promptLen) == promptW);
    bool cursorIsAfterPrompt = (cursorPos >= lineStart + promptLen);
    bool canEdit = lineStartsWithPrompt && cursorIsAfterPrompt && this->GetIsPrompt();

    // 日志
    NppSSH_LogInfoAuto(
        "[可编辑判定] "
        "光标位置=" + IntToStr((int)cursorPos) +
        " 行起始=" + IntToStr((int)lineStart) +
        " 提示符长度=" + IntToStr(promptLen) +
        " prompt结束位置=" + IntToStr((int)(lineStart + promptLen)) +
        " 行匹配提示符=" + IntToStr(lineStartsWithPrompt ? 1 : 0) +
        " 光标在提示符后=" + IntToStr(cursorIsAfterPrompt ? 1 : 0) +
        " 可编辑=" + IntToStr(canEdit ? 1 : 0)
    );

    return canEdit;
}

void SSHTerminal::SetCmd(const char* cmdStr) {
    if (cmdStr) {
        _cmd = cmdStr;
    }
    else {
        _cmd.clear();
    }
}

const char* SSHTerminal::GetCmd() const {
    return _cmd.c_str();
}

void SSHTerminal::SetPrompt(const std::string promptStr) {
    _prompt = promptStr;
}

const std::string& SSHTerminal::GetPrompt() const {
    return _prompt;
}
void SSHTerminal::SetIsPrompt(bool isPrompt) { _isPrompt = isPrompt; }
const bool SSHTerminal::GetIsPrompt() const{ return _isPrompt; }

HWND SSHTerminal_InitTerminalEditBox(HWND hParent, int panelId) {
    SSHTerminal* _SSHTerminal = new SSHTerminal();
    _SSHTerminal->SetPanelId(panelId);
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

void SSHTerminal_AppendOutput(int panelIndex, const std::string& text, bool isPrompt) {
    NppSSH_LogInfoAuto("输出指定面板" + std::to_string(panelIndex));
    NppSSH_LogInfoAuto("输出指定面板" + std::string(text));
    NppSSH_LogInfoAuto("输出指定面板" + std::to_string(vectorSSHTerminal.size()));

    if (panelIndex < 0) return;
    //panelIndex = panelIndex - 1;
    //SSHTerminal* panel = vectorSSHTerminal[panelIndex];
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    panel->SetIsPrompt(isPrompt);
    //panel->SetPrompt(SSH_Prompt(panel->GetPanelId()));

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
void SSHTerminal_PanelPrompt(int panelIndex, const std::string Prompt) {
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    panel->SetPrompt(Prompt);
}
void SSHTerminal_SetIsPrompt(int panelIndex, bool isPrompt) {
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    panel->SetIsPrompt(isPrompt);
}


/*
* 获取当前面板
*/
SSHTerminal* getSSHTerminal(int panelIndex) {
    std::string szDebugMsg = "面板大小改变 === vectorSSHTerminal 内容 ===\r\n总数：" + IntToStr((int)vectorSSHTerminal.size()) + "\r\n\r\n";

    for (int i = 0; i < vectorSSHTerminal.size(); i++)
    {
        SSHTerminal* p = vectorSSHTerminal[i];
        if (p == nullptr) {
            szDebugMsg += "[" + IntToStr(i) + "] 指针：空指针\r\n";
        }
        else {
            szDebugMsg += "[" + IntToStr(i) + "] 指针：" + PtrToHexStr(p) + "\r\n";
        }
    }
    NppSSH_LogInfoAuto(szDebugMsg);

    if (panelIndex < 1) return nullptr;
    panelIndex = panelIndex - 1;
    return vectorSSHTerminal[panelIndex];
}
