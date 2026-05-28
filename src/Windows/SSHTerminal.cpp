// SSHTerminal.cpp模拟终端，具体实现
#include "SSHTerminal.h"
#include <thread>
static std::vector<SSHTerminal*> vectorSSHTerminal;
static NppData s_nppData;
static HINSTANCE s_hInst;

// 新增：防重入标记（避免递归调用）
static thread_local bool s_bProcessingMsg = false;

// 安全地把 std::wstring 转为 std::string 日志专用（避免乱码和异常）
inline std::string WStringToLogStr(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &out[0], len, nullptr, nullptr);
    return out;
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
// 辅助函数：转16进制字符串，方便日志查看
inline std::string IntToHexStr(DWORD val) {
    char buf[32];
    sprintf_s(buf, "%08X", val);
    return std::string(buf);
}
// 输入法中英文切换（真正安全、无循环）
// bForceEnglish: true=强制英文(修复时用) | false=手动切换(Shift用)
inline void imm_chineseType(HWND hEdit)
{
    if (!hEdit) {
        NppSSH_LogInfoAuto("【IME错误】句柄无效");
        return;
    }

    NppSSH_LogInfoAuto("【IME调用】强制微软拼音→英文，hWnd=" + PtrToHexStr(hEdit));

    // 1. 设置焦点（调用imm_chineseType函数前已经设置，暂时废弃）
    HWND hFocus = ::GetFocus();
    bool isEditFocused = (hFocus != hEdit);
    if (isEditFocused) {
        SetFocus(hEdit); // 仅恢复缓存的焦点状态
        NppSSH_LogInfoAuto("【设置焦点3333333333333333】");

    }
    //SetFocus(hEdit);
    //Sleep(10); // 极短等待，让系统同步

    // 2. 跨线程输入同步
    DWORD currTid = GetCurrentThreadId();
    DWORD editTid = GetWindowThreadProcessId(hEdit, NULL);
    AttachThreadInput(editTid, currTid, TRUE);

    // 3. 获取 IME 上下文 系统自带的IME（不再手动创建！）
    HIMC hImc = ImmGetContext(hEdit);
    if (!hImc) {
        hImc = ImmCreateContext();
        ImmAssociateContext(hEdit, hImc);
        NppSSH_LogInfoAuto("【IME】创建新上下文");
    }

    // 读取原始状态
    DWORD conv = 0, sentence = 0;
    ImmGetConversionStatus(hImc, &conv, &sentence);
    NppSSH_LogInfoAuto("【IME修改前】conv=0x" + IntToHexStr(conv));

    // 【微软拼音 官方正确英文模式】
    conv = IME_CMODE_ALPHANUMERIC; // 0x0004 → 纯英文
    sentence = IME_SMODE_NONE;

    // 先打开IME，再设置英文！
    ImmSetOpenStatus(hImc, TRUE);       // 必须打开
    ImmSetConversionStatus(hImc, conv, sentence);
    ImmSetOpenStatus(hImc, FALSE);      // 关闭中文输入

    // 验证结果
    DWORD newConv = 0;
    ImmGetConversionStatus(hImc, &newConv, &sentence);
    NppSSH_LogInfoAuto("【IME修改后】conv=0x" + IntToHexStr(newConv));

    // 绑定生效
    ImmAssociateContext(hEdit, hImc);
    ImmReleaseContext(hEdit, hImc);
    AttachThreadInput(editTid, currTid, FALSE);

    // 强制刷新任务栏
    PostMessage(HWND_BROADCAST, WM_INPUTLANGCHANGE, 0, 0);
    PostMessage(hEdit, WM_IME_NOTIFY, IMN_SETOPENSTATUS, 0);

    NppSSH_LogInfoAuto("【✅ 最终成功】微软拼音已锁定 英文模式");
}
// ========== 【自动唤醒伪终端输入状态，解决命令后无法输入】 ==========
static void FixEditInputState_Final(HWND hEdit)
{
    if (!IsWindow(hEdit))
        return;
    NppSSH_LogInfoAuto("【修复】开始修复输入未显示，光标消失，焦点丢失");

    // 1. 光标定位到末尾（你原有逻辑）
    int len = GetWindowTextLengthW(hEdit);
    SendMessageW(hEdit, EM_SETSEL, len, len);
    SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);

    // ==============================
    // 【新增：强制重建光标（插入符）】
    // 解决：对话框销毁后光标消失但能输入
    // ==============================
    SetForegroundWindow(hEdit);
    SetFocus(hEdit);
    NppSSH_LogInfoAuto("【设置焦点11111111111111111】");


    // 获取字体高度
    HDC hdc = GetDC(hEdit);
    TEXTMETRIC tm = { 0 };
    GetTextMetrics(hdc, &tm);
    ReleaseDC(hEdit, hdc);

    // 销毁旧光标 + 创建新光标（Win32标准）
    DestroyCaret();
    CreateCaret(hEdit, nullptr, 1, tm.tmHeight);

    // 再次定位到末尾
    SendMessage(hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessage(hEdit, EM_SCROLLCARET, 0, 0);

    // 显示光标（关键！）
    ShowCaret(hEdit);
    // ==============================
    // 标准 Win32 跨线程输入复活（你原有逻辑）
    // ==============================
    DWORD editTID = GetWindowThreadProcessId(hEdit, NULL);
    DWORD currTID = GetCurrentThreadId();

    // 绑定线程输入上下文
    AttachThreadInput(editTID, currTID, TRUE);

    // 重新绑定键盘输入
    SetFocus(hEdit);
    NppSSH_LogInfoAuto("【设置焦点2222222222222222】");

    ShowCaret(hEdit); // 再次确保显示

    // 解绑
    AttachThreadInput(editTID, currTID, FALSE);

    // 标准刷新
    InvalidateRect(hEdit, NULL, FALSE);
    UpdateWindow(hEdit);

    // 发送焦点消息，通知系统更新输入状态
    SendMessageW(hEdit, WM_SETFOCUS, 0, 0);

    // 最后再确保一次光标显示（终极保险）
    ShowCaret(hEdit);
}
// 传统伪终端子类化过程（解决消息拦截失效问题）
// ============return res = 0;拦截编辑器的操作，自定义具体操作。
// ============return res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);放行编辑器原始的操作，
LRESULT CALLBACK SSHTerminal::TerminalEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    bool bNeedProcess = true;
    LRESULT res = 0;
    SSHTerminal* terminal = (SSHTerminal*)GetProp(hWnd, L"SSHTerminalInstance");
    if (!terminal) {
        for (auto& t : vectorSSHTerminal) {
            if (t && t->Get_TerminalHandle() == hWnd) {
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
    if (msg == WM_GETDLGCODE)// 告诉系统所有键盘我全吃了，不发给父窗口
    {
        NppSSH_LogInfoAuto("【完全拦截】处理键盘消息");
        return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTMESSAGE;
    }
    switch (msg) {
    case WM_USER + 1001:
        //NppSSH_LogInfoAuto("【修复】调用FixEditInputState_Final函数修复失效！");
        //FixEditInputState_Final(hWnd);
        return 0;
    case WM_APPEND_OUTPUT_TEXT:
    {
        NppSSH_LogInfoAuto("【WM_APPEND_OUTPUT_TEXT】分支已进入");
        if (!terminal) {
            NppSSH_LogInfoAuto("【WM_APPEND_OUTPUT_TEXT】terminal 为空，直接返回");
            break;
        }

        HWND TerminalHandle = terminal->Get_TerminalHandle();
        if (!IsWindow(TerminalHandle)) {
            NppSSH_LogInfoAuto("【WM_APPEND_OUTPUT_TEXT】TerminalHandle 无效");
            break;
        }

        std::wstring* pText = (std::wstring*)lParam;
        if (!pText) {
            NppSSH_LogInfoAuto("【WM_APPEND_OUTPUT_TEXT】lParam 为空，直接返回");
            break;
        }
        NppSSH_LogInfoAuto("【WM_APPEND_OUTPUT_TEXT】收到文本，长度=" + IntToStr((int)pText->size()));

        // ✅ 1. 光标到末尾
        int len = GetWindowTextLengthW(TerminalHandle);
        SendMessageW(TerminalHandle, EM_SETSEL, len, len);
        SendMessageW(TerminalHandle, EM_REPLACESEL, FALSE, (LPARAM)pText->c_str());
        // ✅ 5. 简单滚动
        SendMessageW(TerminalHandle, EM_SCROLLCARET, 0, 0);
        delete pText;
        NppSSH_LogInfoAuto("【WM_APPEND_OUTPUT_TEXT】追加完成");
        return 0;
    }
    // 焦点变化时的处理
    //case WM_SETFOCUS:
    //    imm_chineseType(hWnd); // 窗口获得焦点时，再次强制英文
    //    break;
    //case WM_KILLFOCUS:
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

    
    try {
        NppSSH_LogInfoAuto("TerminalEditProc监听！msg=" + IntToStr(msg) + " hWnd=" + PtrToHexStr(hWnd));

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
            NppSSH_LogInfoAuto("调用远程服务器的历史记录，待实现去远程服务查询历史命令");
            NppSSH_LogInfoAuto("【拦截】上下方向键禁止操作！wParam=" + IntToStr(wParam));
            res = 0;
            s_bProcessingMsg = false;
            return res;
        }

        // ==============================================
        // 【终极修复3】粘贴自动同步到 _cmd（解决粘贴空命令）
        // ==============================================
        if (msg == WM_PASTE && canEdit)
        {
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);

            int len = GetWindowTextLengthW(hWnd);
            std::wstring buf;
            buf.resize(len + 100);
            GetWindowTextW(hWnd, &buf[0], len + 10);

            DWORD cursorPos = 0;
            SendMessageW(hWnd, EM_GETSEL, (WPARAM)&cursorPos, NULL);
            size_t lineStart = 0;
            for (size_t i = cursorPos; i > 0; --i) {
                if (buf[i] == L'\n' || buf[i] == L'\r') { lineStart = i + 1; break; }
            }

            std::wstring promptW = GBKToWstring(terminal->GetPrompt());
            std::wstring cmdW = buf.substr(lineStart + promptW.size());
            std::string cmd;
            for (auto c : cmdW) { if (c != L'\r' && c != L'\n') cmd += (char)c; }
            terminal->SetCmd(cmd.c_str());

            NppSSH_LogInfoAuto("【粘贴同步cmd】成功：" + cmd);
            // 修复粘贴输入状态
            //FixEditInputState(hWnd);
            s_bProcessingMsg = false;
            return res;
        }

        // ========== 回车按键处理逻辑 ==========
        if (msg == WM_KEYDOWN && (wParam == VK_RETURN || wParam == 13))
        {
            canEdit = terminal->IsCursorInEditableArea();
            if (!canEdit) {
                res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
                s_bProcessingMsg = false;
                return res;
            }

            // ============= 【核心：从伪终端提取真实命令】=============
            DWORD cursorPos = 0;
            SendMessageW(hWnd, EM_GETSEL, (WPARAM)&cursorPos, NULL);
            int totalLen = GetWindowTextLengthW(hWnd);
            std::wstring allText;
            allText.resize(totalLen + 100);
            GetWindowTextW(hWnd, &allText[0], totalLen + 10);

            size_t lineStart = 0;
            for (size_t i = cursorPos; i > 0; --i) {
                if (allText[i] == L'\n' || allText[i] == L'\r') {
                    lineStart = i + 1; break;
                }
            }

            std::wstring promptW = GBKToWstring(terminal->GetPrompt());
            std::wstring realCmdW = allText.substr(lineStart + promptW.length());
            std::string realCmd;
            for (auto c : realCmdW) {
                if (c != L'\r' && c != L'\n') realCmd += (char)c;
            }

            // 强制覆盖 _cmd，永远不会为空
            terminal->SetCmd(realCmd.c_str());
            NppSSH_LogInfoAuto("【回车同步】从伪终端提取真实命令：" + realCmd);

            // 执行
            NppSSH_LogInfoAuto("【执行】回车触发命令执行！光标位置=" + IntToStr((int)cursorPos)
                + "命令===" + terminal->GetCmd() + "命令提示符===" + terminal->GetPrompt());

            std::string cmdToExecute = terminal->GetCmd();
            if (cmdToExecute.empty()) {
                NppSSH_LogInfoAuto("【跳过】无命令可执行，仅换行");
                SSHTerminal_AppendOutput(terminal->GetPanelId(), "\r\n"+ terminal->GetPrompt());
                PostMessageW(terminal->Get_TerminalHandle(), WM_USER + 1001, 0, 0);
                res = 0;
                s_bProcessingMsg = false;
                return res;
            }

            // 执行命令
            terminal->SetIsCommandRunning(true); // 标记后台命令开始执行
            // 立即放行，不等待
            std::string cmdCopy = cmdToExecute;
            int panelId = terminal->GetPanelId();

            std::thread([panelId, cmdCopy]() {
                bool result = NppSSH_ExecuteCommand(panelId, cmdCopy);
                }).detach();
            //terminal->SetPrompt(NppSSH_PanelPrompt(terminal->GetPanelId()));
            NppSSH_LogInfoAuto("【调试】TerminalEditProc设置提示符，命令提示符====" + terminal->GetPrompt());
            NppSSH_LogInfoAuto("【命令执行结果】面板ID=" + IntToStr(terminal->GetPanelId())
                + " 命令=" + cmdToExecute +  " ，命令提示符====" + terminal->GetPrompt());

            // 清空命令缓存
            //terminal->SetCmd("");

            res = 0;
            s_bProcessingMsg = false;
            return res;
        }

        // ==============================
        // 退格保护（不动）
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
                    lineStart = i + 1; break;
                }
            }
            size_t promptEndPos = lineStart + promptLen;
            if (cursorPos == promptEndPos && canEdit) {
                isBackspaceAtPromptEnd = true;
            }
        }
        if (isBackspaceAtPromptEnd) {
            NppSSH_LogInfoAuto("【拦截】命令提示符="+ terminal->GetPrompt() +"，禁止删除prompt末尾字符！光标位置=" + IntToStr((int)::SendMessageW(hWnd, EM_GETSEL, 0, 0)));
            res = 0;
            s_bProcessingMsg = false;
            return res;
        }

        // 5. 删除键逻辑（不动）
        bool isDeleteKey = (msg == WM_KEYDOWN && (wParam == VK_BACK || wParam == VK_DELETE));
        if (isDeleteKey) {
            DWORD selStart = 0, selEnd = 0;
            ::SendMessageW(hWnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            DWORD cursorPos = selStart;
            std::string promptStr = terminal->GetPrompt();
            std::string cmdStr = terminal->GetCmd();
            std::wstring wPrompt = GBKToWstring(promptStr);
            int promptLen = (int)wPrompt.length();
            int cmdLen = (int)GBKToWstring(cmdStr).length();
            int totalLen = ::GetWindowTextLengthW(hWnd);
            std::wstring allText;
            allText.resize(totalLen + 1);
            ::GetWindowTextW(hWnd, &allText[0], totalLen + 1);
            size_t lineStart = 0;
            for (size_t i = cursorPos; i > 0; --i) {
                if (allText[i] == L'\n' || allText[i] == L'\r') {
                    lineStart = i + 1; break;
                }
            }
            size_t lineEnd = allText.find_first_of(L"\r\n", cursorPos);
            if (lineEnd == std::wstring::npos) lineEnd = allText.length();
            std::wstring currentLine = allText.substr(lineStart, lineEnd - lineStart);
            size_t promptEndPosInLine = lineStart + promptLen;
            bool willModifyPrompt = false;
            if (wParam == VK_BACK) { willModifyPrompt = (cursorPos < promptEndPosInLine); }
            else if (wParam == VK_DELETE) { willModifyPrompt = (cursorPos < promptEndPosInLine); }
            if (willModifyPrompt) {
                NppSSH_LogInfoAuto("【拦截】删除操作将修改prompt区域，禁止删除！光标位置=" + IntToStr((int)cursorPos));
                res = 0;
                s_bProcessingMsg = false;
                return res;
            }
            std::wstring cmdInLine = currentLine.substr(cmdLen);
            if (cmdLen == 0) {
                NppSSH_LogInfoAuto("【拦截】仅存在prompt无命令，禁止删除！");
                res = 0;
                s_bProcessingMsg = false;
                return res;
            }
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

        // 6. 字符输入（不动）
        if (msg == WM_CHAR && wParam >= 0x20 && wParam <= 0x7E) {
            canEdit = terminal->IsCursorInEditableArea();
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
                        lineStart = i + 1; break;
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
                //FixEditInputState(hWnd);
                NppSSH_LogInfoAuto("【放行】可编辑区域字符输入！currentCmd.c_str()====" + currentCmd);
            }
        }
        // 非字符输入的其他消息
        else if (!canEdit) {
            canEdit = terminal->IsCursorInEditableArea();
            if (!canEdit) {
                if (msg == WM_KEYDOWN || msg == WM_CHAR || msg == WM_KEYUP || msg == WM_PASTE ||
                    msg == WM_DEADCHAR || msg == WM_SYSKEYDOWN || msg == WM_SYSCHAR) {
                    NppSSH_LogInfoAuto("【拦截】非可编辑区域，禁止操作！msg=" + IntToStr(msg) + " wParam=" + IntToStr(wParam));
                    res = 0;
                }
                else {
                    res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
                }
            }
            else {
                res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
                NppSSH_LogInfoAuto("【放行】可编辑区域合法操作！msg=" + IntToStr(msg) + " wParam=" + IntToStr(wParam));
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

SSHTerminal::SSHTerminal() {
    // 初始化成员变量（避免野指针）

    _prompt = "";               // 命令提示符（迁移自Prompt）
    _isCommandRunning = false; // 标记后台命令是否正在执行
    _hwndParent = nullptr;
    _hTerminal = nullptr;
    _cmd = ""; // 或根据实际类型初始化，比如空字符串
    _oldEditProc = nullptr;
}
// 析构函数：释放资源，防止内存泄漏
SSHTerminal::~SSHTerminal() {
    if (_hTerminal && _oldEditProc) {
        // 清理窗口属性（新增）
        RemoveProp(_hTerminal, L"SSHTerminalInstance");
        // 恢复原窗口过程（保留）
        SetWindowLongPtr(_hTerminal, GWLP_WNDPROC, (LONG_PTR)_oldEditProc);
        _oldEditProc = nullptr;
    }
    // 从vector移除自身（保留）
    auto it = std::find(vectorSSHTerminal.begin(), vectorSSHTerminal.end(), this);
    if (it != vectorSSHTerminal.end()) {
        vectorSSHTerminal.erase(it);
    }
}

HWND SSHTerminal::Get_TerminalHandle() const {
    return _hTerminal;
}
HWND SSHTerminal::InitTerminalEditBox(HWND hParent) {
        
    if (!::IsWindow(hParent)) {
        ::MessageBoxW(s_nppData._nppHandle, L"SSH_InitTerminalEditBox: 面板窗口句柄无效！", L"NppSSH调试提示", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    // 保存父窗口（必须！解决 _hSelf 为空导致的崩溃）
    _hwndParent = hParent;

    RECT rc;
    if (!::GetClientRect(hParent, &rc))
        return nullptr;
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

    // WS_EX_TRANSPARENT + 控件默认风格 不支持 IME
    _hTerminal = ::CreateWindowExW(//ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_TABSTOP
        WS_EX_CLIENTEDGE | WS_EX_NOPARENTNOTIFY | WS_EX_ACCEPTFILES,
        L"EDIT",
        L"初始化成功", // 文字设为空
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        x, y, cx, cy,// 初始大小
        _hwndParent,
        (HMENU)IDC_OUTPUT_EDIT,
        s_hInst, // 用全局插件实例句柄
        this
    );

    //强制将微软拼音的输入模式改为英文模式（开启 IME 支持）,已废弃，由imm_chineseType函数内容直接开启支持实现
    //ImmAssociateContext(_hTerminal, ImmCreateContext());

    if (!_hTerminal) {
        ::MessageBoxW(s_nppData._nppHandle, L"SSH_InitTerminalEditBox: 伪终端句柄无效！", L"NppSSH调试提示", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    
    //防闪烁样式设置
    DWORD exStyle = ::GetWindowLongPtr(_hTerminal, GWL_EXSTYLE);
    ::SetWindowLongPtr(_hTerminal, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
    // 设置样式
    DWORD style = ::GetWindowLongPtrW(_hTerminal, GWL_STYLE);
    style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL;
    ::SetWindowLongPtrW(_hTerminal, GWL_STYLE, style);

    // 调整输出伪终端位置，避开顶部按钮栏
    SizeSSHTerminal(hParent);

    // ==== 挂载子类化 ====
    if (!_oldEditProc) {
        // 1. 获取原窗口过程
        _oldEditProc = (WNDPROC)GetWindowLongPtr(_hTerminal, GWLP_WNDPROC);
        if (!_oldEditProc) {
            _oldEditProc = DefWindowProc;
        }
        // 2. 保存原过程到GWLP_USERDATA（仅存原过程，避免偏移冲突）
        SetWindowLongPtr(_hTerminal, GWLP_USERDATA, (LONG_PTR)_oldEditProc);
        // 3. 用窗口属性存储终端实例（替代GWLP_USERDATA+偏移，避免越界）
        SetProp(_hTerminal, L"SSHTerminalInstance", (HANDLE)this);
        // 4. 设置新的窗口过程
        SetWindowLongPtr(_hTerminal, GWLP_WNDPROC, (LONG_PTR)TerminalEditProc);
        NppSSH_LogInfoAuto("伪终端子类化完成！hWnd=" + PtrToHexStr(_hTerminal)
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
    _initialized = true;
    //MessageBoxW(s_nppData._nppHandle, L"终端伪终端初始化完成 ✅", L"成功", MB_OK);
    return _hTerminal;
}
/*
*断开连接，改变终端内容
*/
void SSHTerminal::disConnection() {
    if (_hTerminal) {
        //DisconnectPanel(this -> _panelId);// 面板断开SSH函数
        ::SetWindowTextW(_hTerminal, L"✅ SSH已断开\n等待新的连接...");
    }
}
/*
* 重置终端内容
*/
void SSHTerminal::resetSSHTerminal() {
    if (_hTerminal && ::IsWindow(_hTerminal)) {
        ::SetWindowTextW(_hTerminal, L"🔌 SSH已断开\r\n等待新的连接...resetPanelToInit");
        ::SendMessage(_hTerminal, EM_SETREADONLY, TRUE, 0);
    }
}
/*
* 设置终端面板大小
*/
void SSHTerminal::SizeSSHTerminal(HWND hParent) {//hParent=面板的_hSelf
    if (!_hTerminal || !::IsWindow(_hTerminal))
        return;

    if (!::IsWindow(hParent))
        return;
    //::MessageBoxW(s_nppData._nppHandle, L"SizeSSHTerminal", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);

    //_hTerminal = ::GetDlgItem(hParent, IDC_OUTPUT_EDIT);
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
        _hTerminal,
        HWND_TOP,
        x, y, cx, cy,
        SWP_NOZORDER | SWP_NOACTIVATE
    );
    //只重绘【伪终端】自己让伪终端立刻刷新、重新绘制自己的内容、文字、背景、边框。
    ::RedrawWindow(_hTerminal, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);// 刷新伪终端内容（防止文字不显示）
}

/*
* 追加伪终端终端模拟内容
*/
void SSHTerminal::AppendOutputText(const std::string& text) {
    // 空文本防护，避免非法字符串触发弹框
    if (text.empty() || !_hTerminal) {
        NppSSH_LogWarnAuto("AppendOutputText: 文本或伪终端为空");
        return;
    }
    // ✅ 防止子类化未完成就发消息
    if (!_initialized) {
        NppSSH_LogWarnAuto("AppendOutputText: 伪终端尚未初始化完成，丢弃输出");
        return;
    }
    NppSSH_LogInfoAuto("输出文本到输出框" + std::string(text));
    // ✅ 转成 wstring（堆分配）
    std::wstring* pText = new std::wstring(GBKToWstring(text));

    // ✅ 投递到主线程（绝对安全）
    PostMessage(_hTerminal, WM_APPEND_OUTPUT_TEXT, 0, (LPARAM)pText);
    // 提前缓存焦点状态，避免操作后丢失
    //HWND hFocus = ::GetFocus();
    //bool isEditFocused = (hFocus == _hTerminal);

    //// 临时关闭子类化，避免EM_SETSEL/EM_REPLACESEL触发循环
    //WNDPROC tempOldProc = (WNDPROC)GetWindowLongPtr(_hTerminal, GWLP_WNDPROC);
    //SetWindowLongPtr(_hTerminal, GWLP_WNDPROC, (LONG_PTR)_oldEditProc);
    ////追加文本（只读控件临时取消只读）
    //::SendMessage(_hTerminal, EM_SETREADONLY, FALSE, 0);

    //std::wstring wtext = GBKToWstring(text);
    //// 光标移到末尾，追加文本
    //int len = ::GetWindowTextLengthW(_hTerminal);
    //::SendMessage(_hTerminal, EM_SETSEL, len, len);
    //::SendMessage(_hTerminal, EM_REPLACESEL, FALSE, (LPARAM)wtext.c_str());
    ////// 恢复只读
    //::SendMessage(_hTerminal, EM_SETREADONLY, TRUE, 0);

    //// 恢复子类化
    //SetWindowLongPtr(_hTerminal, GWLP_WNDPROC, (LONG_PTR)tempOldProc);


    //// ========== 精准定位光标到新提示符末尾 ==========
    //// 1. 重新获取追加后的总长度（避免wtext拼接导致的长度偏差）
    //DWORD len_total = ::GetWindowTextLengthW(_hTerminal);
    //// 2. 强制选中末尾（确保光标在最后）
    //::SendMessageW(_hTerminal, EM_SETSEL, len_total, len_total);
    //// 3. 滚动到光标位置（视觉反馈）
    //::SendMessageW(_hTerminal, EM_SCROLLCARET, 0, 0);
    //// 4. 强制刷新光标渲染（解决系统未重绘光标问题）
    //::SendMessageW(_hTerminal, WM_PAINT, 0, 0);
    //// 强制刷新
    //::RedrawWindow(_hTerminal, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    ////FixEditInputState(_hTerminal);
    //// 若当前伪终端是焦点，不重复抢焦；若非焦点，不主动设置（遵循用户操作）
    //if (isEditFocused) {
    //    SetFocus(_hTerminal); // 仅恢复缓存的焦点状态
    //    NppSSH_LogInfoAuto("【设置焦点3333333333333333】");

    //}

    //NppSSH_LogInfoAuto("文本追加完成，当前输出框总长度：" + IntToStr((int)len_total)
    //    + "，追加长度：" + IntToStr((int)text.length()));
}


/*
* 控制键盘的输入操作
*/
bool SSHTerminal::IsCursorInEditableArea() const {

    if (!_hTerminal || !::IsWindow(_hTerminal))
        return false;

    if (_isCommandRunning) {
        NppSSH_LogInfoAuto("[可编辑判定] 后台命令执行中，禁止编辑");
        return false;
    }
    if (_prompt.empty()) {
        NppSSH_LogInfoAuto("[可编辑判定] prompt 为空，允许编辑（防止死锁）");
        return true; // ✅ 兜底
    }
    // 1. 获取光标位置
    DWORD selStart = 0;
    ::SendMessageW(_hTerminal, EM_GETSEL, (WPARAM)&selStart, NULL);
    DWORD cursorPos = selStart;

    // 2. 获取整行文本
    int totalLen = ::GetWindowTextLengthW(_hTerminal);
    std::wstring allText;
    allText.resize(totalLen + 1);
    ::GetWindowTextW(_hTerminal, &allText[0], totalLen + 1);

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

    bool canEdit = false;
    if (promptLen > 0) { // 场景1：提示符非空（命令执行完成）- 原有逻辑
        bool lineStartsWithPrompt = (currentLine.substr(0, promptLen) == promptW);
        bool cursorIsAfterPrompt = (cursorPos >= lineStart + promptLen);
        canEdit = lineStartsWithPrompt && cursorIsAfterPrompt;
    }
    else {// 场景2：提示符为空（命令执行中）- 不允许编辑（避免修改输出）
        canEdit = false;
    }

    // 日志
    NppSSH_LogInfoAuto(
        "【可编辑判定】 "
        "_prompt="+ _prompt+
        "光标位置=" + IntToStr((int)cursorPos) +
        " 行起始=" + IntToStr((int)lineStart) +
        " 提示符长度=" + IntToStr(promptLen) +
        " prompt结束位置=" + IntToStr((int)(lineStart + promptLen)) +
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
    //FixEditInputState(_hTerminal); // 复用原有修复函数
    NppSSH_LogInfoAuto("【提示符更新】主动修复输入状态，新提示符：" + promptStr);
}

const std::string& SSHTerminal::GetPrompt() const {
    return _prompt;
}

HWND SSHTerminal_InitTerminalEditBox(HWND hParent, int panelId) {
    SSHTerminal* _SSHTerminal = new SSHTerminal();
    NppSSH_LogInfoAuto("终端绑定的面板ID==" + std::to_string(panelId));
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
    if (!::IsWindow(panel ->Get_TerminalHandle())) {
        ::MessageBoxW(s_nppData._nppHandle, L"SSHTerminal_SizeSSHTerminal: 伪终端句柄无效，跳过调整！", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    panel->SizeSSHTerminal(hParent);

}

void SSHTerminal_AppendOutput(int panelIndex, const std::string& text) {
    NppSSH_LogInfoAuto("输出指定面板" + std::to_string(panelIndex)+"输出具体内容======"+ std::string(text));

    if (panelIndex < 0) return;
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    //if (panel->GetIsCommandRunning()) {
    //    NppSSH_LogInfoAuto("【后台执行中】暂不处理输入状态，面板ID=" + IntToStr(panelIndex));
    //}
    // 
    // 每次追加实时更新命令提示符（暂时抛弃，由于有新的命令提示符会自动设置）
    //std::string prompt = NppSSH_PanelPrompt(panel->GetPanelId());
    //panel->SetPrompt(prompt);
    NppSSH_LogInfoAuto("【调试】SSHTerminal_AppendOutput设置提示符，命令提示符====" + panel->GetPrompt());


    if (!panel || !panel->Get_TerminalHandle())
        return;
    // 修复换行处理：只替换孤立的\n，保留\r\n
    std::string fixedText;
    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) {
            fixedText += "\r\n";
        }
        else {
            fixedText += text[i];
        }
    }

    // 处理开头的换行
    // 判断是否以 \r\n 开头，如果不是，则在开头追加
    bool hasLeadingNewLine = false;
    if (fixedText.size() >= 2) {
        if (fixedText[0] == '\r' && fixedText[1] == '\n') {
            hasLeadingNewLine = true;
        }
    }

    // 如果没有开头换行，则追加
    if (!hasLeadingNewLine && !fixedText.empty()) {
        fixedText.insert(0, "\r\n");
        NppSSH_LogInfoAuto("【自动换行】在输出开头追加 \\r\\n");
    }

    panel->AppendOutputText(fixedText);

}
void SSHTerminal_PanelPrompt(int panelIndex, const std::string Prompt) {
    NppSSH_LogInfoAuto("【调试】SSHTerminal_PanelPrompt设置提示符");
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    panel->SetPrompt(Prompt);
}
void SSHTerminal_SetIsCommandRunning(int panelIndex, bool isCommandRunning) {
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    if (panel)
    {
        panel->SetIsCommandRunning(isCommandRunning);

        // ==============================
        // 【最终修复】命令执行完成 = false
        // 只有这里调用，才不会破坏流程
        // ==============================
        if (!isCommandRunning)
        {
            // 确保在主线程执行UI操作（关键：PostMessage到主窗口，避免线程跨域）
            HWND hEdit = panel->Get_TerminalHandle();
            //NppSSH_LogInfoAuto("【修复】修复4444444444444444444444444444444");
            //FixEditInputState_Final(hEdit);
            PostMessageW(hEdit, WM_USER + 1001, 0, 0); // 自定义消息触发修复
            NppSSH_LogInfoAuto("【命令完全结束】恢复伪终端焦点，可直接输入");

            // ✅ 新增：强制刷新可编辑状态
            PostMessageW(hEdit, WM_KEYDOWN, VK_F5, 0);
        }
    }
}
void SSHTerminal_SetEnglishType(int panelIndex) {
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    if (panel)
    {
        HWND TerminalHWND = panel->Get_TerminalHandle();
        imm_chineseType(TerminalHWND);
    }
}
std::string SSHTerminal_getPanelPrompt(int panelIndex) {
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    if (panel)
    {
        return panel->GetPrompt();
    }
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
    //NppSSH_LogInfoAuto(szDebugMsg);

    if (panelIndex < 1) return nullptr;
    panelIndex = panelIndex - 1;
    return vectorSSHTerminal[panelIndex];
}
