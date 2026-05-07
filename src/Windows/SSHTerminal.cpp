// SSHTerminal.cpp模拟终端，具体实现
#include "SSHTerminal.h"
static WNDPROC s_pOldEditProc = nullptr; // 保存 EDIT 原来的窗口过程
static std::vector<SSHTerminal*> vectorSSHTerminal;
static NppData s_nppData;
static HINSTANCE s_hInst;

// 传统编辑框子类化过程（解决消息拦截失效问题）
LRESULT CALLBACK TerminalEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 强制打印日志，确认函数进入（保留原有逻辑）
    NppSSH_LogInfoAuto("TerminalEditProc监听！msg=" + std::to_string(msg));

    // 优先从窗口附加数据获取终端实例（更可靠）
    SSHTerminal* terminal = reinterpret_cast<SSHTerminal*>(GetWindowLongPtr(hWnd, GWLP_USERDATA + sizeof(LONG_PTR)));
    // 降级兼容：如果附加数据未找到，再遍历vector（兜底）
    if (!terminal) {
        for (auto* t : vectorSSHTerminal) {
            if (t && t->GetOutputEditHandle() == hWnd) {
                terminal = t;
                break;
            }
        }
    }

    // 如果仍未找到终端，直接调用原窗口过程（避免消息丢失）
    if (!terminal) {
        WNDPROC oldProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        return CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
    }

    // 提前打印消息类型，便于调试
    NppSSH_LogInfoAuto("TerminalEditProc找到终端！msg=" + std::to_string(msg) + " wParam=" + std::to_string(wParam));

    // 核心：消息处理逻辑（WM_KEYDOWN/WM_CHAR拦截）
    switch (msg) {
    case WM_KEYDOWN: {
        // 强制打印，确认WM_KEYDOWN进入
        NppSSH_LogInfoAuto("WM_KEYDOWN监听！wParam=" + std::to_string(wParam));

        // 移除多余的return 0（原逻辑会直接截断所有按键，导致后续消息不触发）改为：仅拦截指定按键，其他按键放行
        if (wParam == VK_BACK || wParam == VK_DELETE || (wParam >= 0x20 && wParam <= 0x7E)) {
            NppSSH_LogInfoAuto("WM_KEYDOWN拦截按键：" + std::to_string(wParam));
            // 仅拦截时return 0，否则放行
            return 0;
        }
        // 非拦截按键，继续传递消息
        break;
    }
    case WM_KEYUP: {
        NppSSH_LogInfoAuto("WM_KEYUP监听！wParam=" + std::to_string(wParam));
        break;
    }
    case WM_CHAR: {
        NppSSH_LogInfoAuto("WM_CHAR监听！wParam=" + std::to_string(wParam));
        if (!terminal->IsCursorInEditableArea()) {
            NppSSH_LogInfoAuto("WM_CHAR：光标在非法区域，拦截输入！");
            if (wParam == VK_BACK || wParam == VK_DELETE || (wParam >= 0x20 && wParam <= 0x7E)) {
                return 0;
            }
        }
        break;
    }
    case WM_PASTE: {
        NppSSH_LogInfoAuto("WM_PASTE监听！");
        if (!terminal->IsCursorInEditableArea()) {
            NppSSH_LogInfoAuto("WM_PASTE：光标在非法区域，禁止粘贴！");
            return 0; // 禁止粘贴
        }
        break;
    }
    case WM_SETFOCUS: {
        NppSSH_LogInfoAuto("WM_SETFOCUS监听！");
        if (!terminal->IsCursorInEditableArea()) {
            terminal->ForceCursorToEditableEnd();
        }
        break;
    }
    case WM_MOUSEMOVE: {
        NppSSH_LogInfoAuto("WM_MOUSEMOVE监听！");
        if (!terminal->IsCursorInEditableArea()) {
            terminal->ForceCursorToEditableEnd();
        }
        break;
    }
    case WM_LBUTTONUP: {
        NppSSH_LogInfoAuto("WM_LBUTTONUP监听！");
        if (!terminal->IsCursorInEditableArea()) {
            terminal->ForceCursorToEditableEnd();
        }
        break;
    }
    }

    // 确保原窗口过程调用的正确性
    WNDPROC oldProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    LRESULT result = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
    NppSSH_LogInfoAuto("TerminalEditProc调用原过程！msg=" + std::to_string(msg) + " result=" + std::to_string(result));
    return result;
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
    _promptEndPos = 0;
    _cmd = nullptr; // 或根据实际类型初始化，比如空字符串
    _oldEditProc = nullptr;
}
// 析构函数：释放图标资源，防止内存泄漏
SSHTerminal::~SSHTerminal() {
    if (_hOutputEdit && _oldEditProc) {
        // 清理附加数据
        SetWindowLongPtr(_hOutputEdit, GWLP_USERDATA + sizeof(LONG_PTR), 0);
        SetWindowLongPtr(_hOutputEdit, GWLP_WNDPROC, (LONG_PTR)_oldEditProc);
        _oldEditProc = nullptr;
    }
    // 从全局vector移除自身
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
    // ==== 关键：给 挂载子类化 ====
    if (!_oldEditProc) {
        // 第一步：获取原窗口过程
        _oldEditProc = (WNDPROC)GetWindowLongPtr(_hOutputEdit, GWLP_WNDPROC);
        // 第二步：保存原过程到GWLP_USERDATA
        SetWindowLongPtr(_hOutputEdit, GWLP_USERDATA, (LONG_PTR)_oldEditProc);
        // 第三步：将终端实例附加到窗口（GWLP_USERDATA + sizeof(LONG_PTR) 避免覆盖）
        SetWindowLongPtr(_hOutputEdit, GWLP_USERDATA + sizeof(LONG_PTR), (LONG_PTR)this);
        // 第四步：设置新的窗口过程
        SetWindowLongPtr(_hOutputEdit, GWLP_WNDPROC, (LONG_PTR)TerminalEditProc);
        NppSSH_LogInfoAuto("编辑框子类化完成！原过程：0x%p 新过程：0x%p", _oldEditProc, TerminalEditProc);
    }
    // 确保vector中只添加一次
    if (std::find(vectorSSHTerminal.begin(), vectorSSHTerminal.end(), this) == vectorSSHTerminal.end()) {
        vectorSSHTerminal.push_back(this);
    }



    //vectorSSHTerminal.push_back(this);
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
    if (!_hOutputEdit || !::IsWindow(_hOutputEdit))
        return;

    if (!::IsWindow(hParent))
        return;
    //::MessageBoxW(s_nppData._nppHandle, L"SizeSSHTerminal", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);

    //_hOutputEdit = ::GetDlgItem(hParent, IDC_OUTPUT_EDIT);
    RECT rc;
    if (!::GetClientRect(hParent, &rc))
        return;
    //::GetClientRect(hParent, &rc);
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
