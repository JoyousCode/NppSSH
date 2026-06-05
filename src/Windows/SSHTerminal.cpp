// SSHTerminal.cpp模拟终端，具体实现
#include "SSHTerminal.h"
#include <thread>
static std::vector<SSHTerminal*> vectorSSHTerminal;
static NppData s_nppData;
static HINSTANCE s_hInst;



// 根据选中区间，算出不含尾部\r\n的真实结束下标，原文不动
static DWORD GetValidSelEnd(const std::wstring& fullText, DWORD selStart, DWORD selEnd)
{
    const DWORD txtLen = static_cast<DWORD>(fullText.size());
    DWORD finalSelEnd = selStart; // 最终有效选中终点（初始为选中起始）
    DWORD visibleCharCount = 0;   // 可见字符计数（排除\r\n）

    // 遍历选中区间，过滤\r\n，统计可见字符
    for (DWORD curPos = selStart; curPos < selEnd && curPos < txtLen; ++curPos)
    {
        wchar_t ch = fullText[curPos];
        // 跳过\r（且预判后续的\n也跳过，避免重复处理）
        if (ch == L'\r')
        {
            // 如果下一个字符是\n，直接跳过\n
            if (curPos + 1 < selEnd && fullText[curPos + 1] == L'\n')
            {
                ++curPos; // 跳过\n
            }
            continue; // 跳过当前\r
        }
        // 跳过单独的\n（容错场景：仅存在\n无\r）
        else if (ch == L'\n')
        {
            continue;
        }

        // 可见字符：计数+更新有效终点
        ++visibleCharCount;
        finalSelEnd = curPos + 1; // 有效终点为当前字符的下一位（符合EM_SETSEL规则）
    }

    // 容错：若全是换行（无可见字符），返回起始位置（取消选中）
    if (visibleCharCount == 0)
    {
        return selStart;
    }

    return finalSelEnd;
}
inline std::wstring CleanrrW(const std::wstring& allText) {
    std::wstring fixedText = allText;
    //for (wchar_t c : allText) {
    //    if (c == L'\r') continue; // 删掉 RichEdit 自动加的所有 \r
    //    fixedText += c;
    //}
    return fixedText;
}
inline std::wstring NormalizeTerminalLineFeed(std::wstring src)
{
    // 1. 第一步：全量删除所有 \r 字符（无论位置）
    src.erase(std::remove(src.begin(), src.end(), L'\r'), src.end());

    // 2. 第二步：把连续任意个\n 压缩成单个\n
    size_t findPos = 0;
    while ((findPos = src.find(L"\n\n", findPos)) != std::wstring::npos)
    {
        src.replace(findPos, 2, L"\n");
    }
    return src;
}
inline std::string Cleanrr(const std::string& input) {
    std::string out;
    bool lastWasR = false;   // 标记上一个是不是 \r
    bool hasR = false;      // 标记这段有没有累积 \r

    for (char c : input) {
        if (c == '\r') {
            hasR = true;        // 累积 \r
            lastWasR = true;
        }
        else if (c == '\n') {
            // 遇到 \n，前面所有 \r 全部删掉，只保留一个 \n
            out += '\n';
            hasR = false;
            lastWasR = false;
        }
        else {
            // 普通字符
            if (hasR) {
                // 前面是单独的 \r，要保留
                out += '\r';
                hasR = false;
            }
            out += c;
            lastWasR = false;
        }
    }

    // 处理字符串末尾剩下的单独 \r
    if (hasR) {
        out += '\r';
    }

    return out;
}
// 清理ANSI转义序列（解决乱码核心）
inline std::wstring CleanAnsiEscapeSequences(const std::wstring& input) {
    std::wstring out;

    enum class State {
        Normal,
        Escape,      // 读到 \x1B
        CSI,         // 读到 \x1B[
        OSC          // 读到 \x1B]
    };

    State state = State::Normal;

    for (wchar_t c : input) {
        // 过滤非法控制字符（0x80是常见乱码源，0x00-0x1F除\r\n\t外全部过滤）
        if ((c >= 0x00 && c <= 0x1F && c != L'\r' && c != L'\n' && c != L'\t') || c == 0x80 || c == 0x6F5F) {
            continue;
        }

        switch (state) {
        case State::Normal:
            if (c == L'\x1B') {
                state = State::Escape;
            }
            else {
                out += c; // 正常字符保留
            }
            break;

        case State::Escape:
            if (c == L'[') {
                state = State::CSI;
            }
            else if (c == L']') {
                state = State::OSC;
            }
            else {
                state = State::Normal; // 未知ESC后缀，切回普通状态
            }
            break;

        case State::CSI:
            // 大小写字母/问号(?)结束CSI序列（补充处理0x1B[?1034h这类序列）
            if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || c == L'?') {
                state = State::Normal;
            }
            break;

        case State::OSC:
            // \a / \r / \n / BEL 终止OSC序列（补充BEL字符0x07）
            if (c == L'\a' || c == L'\r' || c == L'\n' || c == L'\x07') {
                state = State::Normal;
            }
            break;
        }
    }

    return out;
}

inline std::string CleanAnsiEscapeSequences(const std::string& input) {
    std::string out;

    enum class State {
        Normal,
        Escape,      // 读到 \x1B
        CSI,         // 读到 \x1B[
        OSC          // 读到 \x1B]
    };

    State state = State::Normal;

    for (unsigned char c : input) {
        switch (state) {
        case State::Normal:
            if (c == '\x1B') {
                // 进入转义序列
                state = State::Escape;
            }
            else if (c < 0x20 && c != '\r' && c != '\n' && c != '\t') {
                // 过滤除 \r\n\t 以外的控制字符
                continue;
            }
            else {
                // 正常字符保留
                out += c;
            }
            break;

        case State::Escape:
            if (c == '[') {
                // CSI 序列：\x1B[...]
                state = State::CSI;
            }
            else if (c == ']') {
                // OSC 序列：\x1B[...\a
                state = State::OSC;
            }
            else {
                // 未知转义，退出
                state = State::Normal;
            }
            break;

        case State::CSI:
            // 遇到字母结束CSI
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                state = State::Normal;
            }
            // 全程吞掉，不输出
            break;

        case State::OSC:
            // 遇到 \a 结束OSC
            if (c == '\a' || c == '\r' || c == '\n') {
                state = State::Normal;
            }
            // 全程吞掉，不输出
            break;
        }
    }

    return out;
}
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
// 编码转换工具（自动识别UTF8，解决Windows乱码）
inline std::wstring UTF8ToWstring(const std::string& str) {
    if (str.empty())
        return L"";

    // 第一步：先清理非法字符，避免转换失败
    //std::string cleanStr = CleanAnsiEscapeSequences(str);
    std::string cleanStr = (str);

    int len = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS, // 严格校验，非法字符返回错误
        cleanStr.c_str(),
        (int)cleanStr.size(),
        nullptr,
        0
    );

    // 容错：如果严格转换失败，用替换模式重试
    if (len == 0) {
        len = MultiByteToWideChar(
            CP_UTF8,
            0, // 忽略无效字符
            cleanStr.c_str(),
            (int)cleanStr.size(),
            nullptr,
            0
        );
        NppSSH_LogInfoAuto("【UTF8转换容错】检测到非法UTF8字符，已忽略");
    }

    std::wstring res(len, L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        cleanStr.c_str(),
        (int)cleanStr.size(),
        &res[0],
        len
    );
    NppSSH_LogInfoAuto("自动识别UTF8，解决Windows乱码,转换成功");
    return res;
}
// 编码转换工具（自动识别GBK，解决Windows乱码）
inline std::wstring GBKToWstring(const std::string& str) {
    if (str.empty())
        return L"";

    int len = MultiByteToWideChar(
        CP_ACP,         // 系统本地GBK
        0,
        str.c_str(),
        (int)str.size(),
        nullptr,
        0
    );

    std::wstring res(len, L'\0');
    MultiByteToWideChar(
        CP_ACP,
        0,
        str.c_str(),
        (int)str.size(),
        &res[0],
        len
    );

    return res;
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
    //PostMessage(HWND_BROADCAST, WM_INPUTLANGCHANGE, 0, 0);
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
/*
* ANSI 码	颜色
* \e[30m	黑
* \e[31m	红
* \e[32m	绿
* \e[33m	黄
* \e[34m	蓝 (当前 ls 目录色)
* \e[35m	紫
* \e[36m	青
* \e[37m	白
* \e[94m	亮蓝
* \e[0m	重置黑色默认
* \e[38;5;27m	自定义深蓝 (你现在 ls 蓝色)
**/
// 原有 ParseAnsiColorSequence 函数可以保留但不再在输出循环调用
void SSHTerminal::ParseAnsiParseOnly(const std::wstring& params, CHARFORMAT2W& outCf)
{
    std::wstring seq = params.substr(2);
    std::vector<int> codes;
    std::wstringstream ss(seq);
    std::wstring token;

    while (std::getline(ss, token, L';'))
    {
        try {
            if (!token.empty()) codes.push_back(std::stoi(token));
        }
        catch (...) {}
    }

    int state = 0; // 0空闲 1=38 2=5
    int colIdx = 0;
    for (int cd : codes)
    {
        if (state == 1)
        {
            if (cd == 5) state = 2;
            else state = 0;
            continue;
        }
        if (state == 2)
        {
            colIdx = cd;
            state = 0;
            if (colIdx == 27)
                outCf.crTextColor = RGB(0, 0, 200);
            continue;
        }
        switch (cd)
        {
        case 38: state = 1; break;
        case 0:
            outCf.crTextColor = RGB(0, 0, 0);
            outCf.dwEffects &= ~CFE_BOLD;
            break;
        case 1:
            outCf.dwEffects |= CFE_BOLD; break;
        case 30: outCf.crTextColor = ANSI_COLORS[0]; break;
        case 31: outCf.crTextColor = ANSI_COLORS[1]; break;
        case 32: outCf.crTextColor = ANSI_COLORS[2]; break;
        case 33: outCf.crTextColor = ANSI_COLORS[3]; break;
        case 34: outCf.crTextColor = ANSI_COLORS[4]; break;
        case 35: outCf.crTextColor = ANSI_COLORS[5]; break;
        case 36: outCf.crTextColor = ANSI_COLORS[6]; break;
        case 37: outCf.crTextColor = ANSI_COLORS[7]; break;
        case 90: outCf.crTextColor = ANSI_COLORS[8]; break;
        case 91: outCf.crTextColor = ANSI_COLORS[9]; break;
        case 92: outCf.crTextColor = ANSI_COLORS[10]; break;
        case 93: outCf.crTextColor = ANSI_COLORS[11]; break;
        case 94: outCf.crTextColor = ANSI_COLORS[12]; break;
        case 95: outCf.crTextColor = ANSI_COLORS[13]; break;
        case 96: outCf.crTextColor = ANSI_COLORS[14]; break;
        case 97: outCf.crTextColor = ANSI_COLORS[15]; break;
        default: break;
        }
    }
}
void SSHTerminal::StoreTerminalContent(wchar_t ch)
{
    _oldStoreContent += ch; // 无需判空，string自动扩容，只追加
}
void SSHTerminal::StoreTerminalContent(const std::string& str)
{
    if (str.empty())
        return;
    std::wstring wStr = UTF8ToWstring(str);
    _oldStoreContent += wStr;
}
#define ID_UNDO     1001
#define ID_CUT      1002
#define ID_COPY     1003
#define ID_PASTE    1004
#define ID_SELECTALL 1005
// 防重入标记（避免递归调用）
static thread_local bool s_bProcessingMsg = false;
// 传统伪终端子类化过程（解决消息拦截失效问题）
// ============return res = 0;拦截编辑器的操作，自定义具体操作。
// ============return res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);放行编辑器原始的操作，
LRESULT CALLBACK SSHTerminal::TerminalEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

    // ✅ 关键优化1：精确过滤消息，只对需要处理的消息使用防重入标记
    //bool isKeyboardMsg = (msg == WM_KEYDOWN || msg == WM_KEYUP || 
    //    msg == WM_CHAR || msg == WM_DEADCHAR ||
    //    msg == WM_SYSKEYDOWN || msg == WM_SYSCHAR ||
    //    msg == WM_PASTE || msg == WM_COPY || msg == WM_CUT || msg == EM_UNDO ||
    //    msg == WM_APPEND_OUTPUT_TEXT || msg == MSG_FIX_SELECT_TRAIL_NEWLINE ||
    //    msg == WM_NOTIFY || wParam == VK_SHIFT || wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_HOME || wParam == VK_END ||
    //    msg == WM_CONTEXTMENU || msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP || msg == WM_RBUTTONDBLCLK
    //    ); 
    bool isIMEMsg = (msg == WM_IME_STARTCOMPOSITION || msg == WM_IME_COMPOSITION || msg == WM_IME_ENDCOMPOSITION || msg == WM_IME_NOTIFY || msg == WM_IME_CHAR);
    bool isKeyboardMsg = (
        // 1.原生键盘全量消息
        msg == WM_KEYDOWN || msg == WM_KEYUP ||
        msg == WM_CHAR || msg == WM_DEADCHAR ||
        msg == WM_SYSKEYDOWN || msg == WM_SYSCHAR || msg == WM_SYSKEYUP ||
        // 2.剪贴板编辑消息
        msg == WM_PASTE || msg == WM_COPY || msg == WM_CUT || msg == WM_CLEAR ||
        // 3.鼠标相关（右键+左键拖动，选区变更依赖）
        //msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK ||
        //msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP || msg == WM_RBUTTONDBLCLK || msg == WM_CONTEXTMENU ||
        // msg == WM_MOUSEWHEEL || //msg == WM_MOUSEMOVE ||
        // 4.核心必须加：WM_NOTIFY（EN_SELCHANGE选区通知依赖这条，不加收不到选中回调）
        msg == WM_NOTIFY || wParam == EN_SELCHANGE ||
        // 5.自定义业务消息
        msg == WM_APPEND_OUTPUT_TEXT ||
        isIMEMsg
        );
    // 非键盘消息直接放行，不记录日志
    if (!isKeyboardMsg) {
        return CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
    }

    // ✅ 关键优化3：只对键盘消息使用防重入标记
    //if (s_bProcessingMsg) {
    //    NppSSH_LogInfoAuto("【防重入】跳过键盘消息 msg=" + IntToStr(msg));
    //    return CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
    //}

    //if (isKeyboardMsg) {
    //    s_bProcessingMsg = true;
    //}

    switch (msg) {
    
    //case WM_SYSKEYDOWN:
    //    NppSSH_LogInfoAuto("【输入法】WM_SYSKEYDOWN分支已进入");
    //    res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
    //    s_bProcessingMsg = false;
    //    return res;
    //case WM_SYSKEYUP:
    //    NppSSH_LogInfoAuto("【输入法】WM_SYSKEYUP分支已进入");
    //    res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
    //    s_bProcessingMsg = false;
    //    return res;
    //case WM_SYSCHAR:
    //    NppSSH_LogInfoAuto("【输入法】WM_SYSCHAR分支已进入");
    //    res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
    //    s_bProcessingMsg = false;
    //    return res;
    //case WM_KEYUP:
    //    NppSSH_LogInfoAuto("【输入法】WM_KEYUP分支已进入");
    //    res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
    //    s_bProcessingMsg = false;
    //    return res;
    // 
    // 
    // 
    case WM_IME_STARTCOMPOSITION:
        NppSSH_LogInfoAuto("【输入法】WM_IME_STARTCOMPOSITION分支已进入");
        res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
        s_bProcessingMsg = false;
        return res;
    case WM_IME_COMPOSITION:
        NppSSH_LogInfoAuto("【输入法】WM_IME_COMPOSITION分支已进入");
        res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
        s_bProcessingMsg = false;
        return res;
    case WM_IME_ENDCOMPOSITION:
        NppSSH_LogInfoAuto("【输入法】WM_IME_ENDCOMPOSITION分支已进入");
        res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
        s_bProcessingMsg = false;
        return res;
    case WM_IME_NOTIFY:
        NppSSH_LogInfoAuto("【输入法】WM_IME_NOTIFY分支已进入");
        res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
        s_bProcessingMsg = false;
        return res;
    case WM_IME_CHAR:
        NppSSH_LogInfoAuto("【输入法】WM_IME_CHAR分支已进入");
        res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
        s_bProcessingMsg = false;
        return res;
    case WM_APPEND_OUTPUT_TEXT:
        NppSSH_LogInfoAuto("【WM_APPEND_OUTPUT_TEXT】分支已进入");
        {
            std::wstring* pText = (std::wstring*)lParam;
            if (!pText) {
                NppSSH_LogInfoAuto("【WM_APPEND_OUTPUT_TEXT】lParam 为空，直接返回");
                s_bProcessingMsg = false;
                return 0;
            }
            NppSSH_LogInfoAuto("【WM_APPEND_OUTPUT_TEXT】收到文本，长度=" + IntToStr((int)pText->size()));

            // 打印每个字符的 Unicode 编码（比如 ✅ 的编码是 0x2705）
            std::string charCodes;
            for (wchar_t ch : *pText) {
                charCodes += "0x" + IntToHexStr((DWORD)ch) + " ";
            }
            NppSSH_LogInfoAuto("【宽字符编码】" + charCodes);

            std::wstring text = *pText;
            //text = CleanAnsiEscapeSequences(text);

            //解析颜色
            // 步骤1：先全量清理非法字符和ANSI序列（核心修复）
            //std::wstring rawText = CleanAnsiEscapeSequences(*pText);
            std::wstring raw = *pText;
            delete pText; // 提前释放，避免内存泄漏

            // 步骤2：解析ANSI颜色序列（仅处理合法颜色控制）
            //===== 临时测试：手动拼接 \e[34mTEST\e[0m 强制蓝色，测试上色是否可用
    //raw = L"\x1B[34mtest_blue\x1B[0m\n" + raw;

            char tmpLog[512] = { 0 };
            sprintf(tmpLog, "【原始接收字符串长度:%d】", (int)raw.size());
            //NppSSH_LogInfoAuto(tmpLog);

            std::wstring ansiBuf;
            bool inCSI = false;
            bool inOSC = false;
            bool pendingNewColor = false;

            CHARFORMAT2W curCf = { 0 };
            curCf.cbSize = sizeof(curCf);
            curCf.dwMask = CFM_COLOR | CFM_BOLD;
            curCf.crTextColor = RGB(0, 0, 0);
            curCf.crBackColor = RGB(255, 255, 255);
            curCf.dwEffects = 0;

            // 【优化点：重构DrawChar，先设格式再写入字符，pendingNewColor前置消耗】
            auto DrawChar = [&](wchar_t ch)
                {
                    char logBuf[256] = { 0 };
                    sprintf(logBuf, "【DrawChar输出字符:%c | pendingNewColor:%d】", (char)ch, pendingNewColor);
                    //NppSSH_LogInfoAuto(logBuf);

                    wchar_t buf[2] = { ch,0 };
                    int pos = GetWindowTextLengthW(hWnd);

                    // 优化时序：光标定位→应用颜色→写入字符，保证字符使用最新颜色
                    SendMessageW(hWnd, EM_SETSEL, pos, pos);
                    // 存在待生效新颜色，立刻应用到当前选中区域
                    if (pendingNewColor)
                    {
                        SendMessageW(hWnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&curCf);
                        //NppSSH_LogInfoAuto("【本字符是ANSI后首字符，用完新色标记，pendingNewColor=false】");
                        pendingNewColor = false;
                    }
                    // 写入单个字符
                    SendMessageW(hWnd, EM_REPLACESEL, FALSE, (LPARAM)buf);
                    // 光标移动到末尾
                    int newEnd = GetWindowTextLengthW(hWnd);
                    SendMessageW(hWnd, EM_SETSEL, newEnd, newEnd);

                    terminal->StoreTerminalContent(ch);
                        
                };

            // 整段循环前统一关闭只读，避免单次DrawChar反复开关（高频丢色诱因）
            //SendMessageW(hWnd, EM_SETREADONLY, FALSE, 0);废除：重复只读设置会输入法锁定

            int idx = 0;
            for (wchar_t ch : raw)
            {
                char logBuf[512] = { 0 };
                sprintf(logBuf, "【循环下标%d | 当前字符:%c | inCSI:%d pendingNewColor:%d】", idx++, (char)ch, inCSI, pendingNewColor);
                //NppSSH_LogInfoAuto(logBuf);

                if (!inCSI && !inOSC && ch == L'\x1B')
                {
                    //NppSSH_LogInfoAuto("【捕获ESC起始符】");
                    ansiBuf.clear();
                    ansiBuf += ch;
                    continue;
                }
                if (ansiBuf.size() == 1 && ansiBuf[0] == L'\x1B')
                {
                    if (ch == L'[') {
                        //NppSSH_LogInfoAuto("【进入CSI序列】");
                        inCSI = true; ansiBuf += ch; continue;
                    }
                    else if (ch == L']') {
                        //NppSSH_LogInfoAuto("【进入OSC序列】");
                        inOSC = true; ansiBuf += ch; continue;
                    }
                    else
                    {
                        //NppSSH_LogInfoAuto("【孤立ESC字符，直接输出】");
                        DrawChar(ansiBuf[0]);
                        ansiBuf.clear();
                        DrawChar(ch);
                        continue;
                    }
                }
                if (inOSC)
                {
                    ansiBuf += ch;
                    if (ch == L'\x07') {
                        //NppSSH_LogInfoAuto("【OSC序列结束】");
                        inOSC = false; ansiBuf.clear();
                    }
                    continue;
                }
                if (inCSI)
                {
                    ansiBuf += ch;
                    bool letterEnd = ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z'));
                    bool otherEnd = !((ch >= L'0' && ch <= L'9') || ch == L';' || ch == L'?');

                    if (letterEnd || otherEnd)
                    {
                        if (ch == L'J')
                        {
                            NppSSH_LogInfoAuto("【检测到ANSI清屏指令，执行清空】");
                            // 调用外部清屏接口
                            SSHTerminal_ClearOutputText(terminal->GetPanelId());
                        }

                        char log1[512] = { 0 };
                        sprintf(log1, "【CSI序列结束，结束字符:%c】", (char)ch);
                        //NppSSH_LogInfoAuto(log1);

                        CHARFORMAT2W tmpCf = curCf;
                        if (terminal) terminal->ParseAnsiParseOnly(ansiBuf, tmpCf);
                        curCf = tmpCf;

                        char log2[512] = { 0 };
                        sprintf(log2, "【ANSI解析完成，更新颜色R:%d G:%d B:%d，置pendingNewColor=true】",
                            GetRValue(curCf.crTextColor), GetGValue(curCf.crTextColor), GetBValue(curCf.crTextColor));
                        //NppSSH_LogInfoAuto(log2);

                        pendingNewColor = true;
                        inCSI = false;
                        ansiBuf.clear();

                        if (!letterEnd)
                        {
                            //NppSSH_LogInfoAuto("【非字母结束符，输出当前控制字符】");
                            DrawChar(ch);
                        }
                        else
                        {
                            //NppSSH_LogInfoAuto("【字母m结束符，丢弃本字符，不Draw】");
                        }
                        continue;
                    }
                    continue;
                }

                // 普通可见字符直接绘制（pendingNewColor在DrawChar内部前置消耗）
                DrawChar(ch);
            }

            // 【优化：循环结束兜底，处理残留孤立ESC，避免隐形字符吞首字符】
            if (!ansiBuf.empty())
            {
                NppSSH_LogInfoAuto("【存在未闭合ANSI缓存，输出残留ESC】");
                for (auto ch : ansiBuf)
                    DrawChar(ch);
                ansiBuf.clear();
            }

            // 循环全部结束后恢复只读+滚动光标
            //SendMessageW(hWnd, EM_SETREADONLY, TRUE, 0);// 废除：重复只读设置会输入法锁定

            // 写完所有字符后自动滚动光标
            SendMessageW(hWnd, EM_SCROLLCARET, 0, 0);
            NppSSH_LogInfoAuto("【WM_APPEND_OUTPUT_TEXT】追加完成");
        }
        s_bProcessingMsg = false;
        return 0;
    
    case WM_NOTIFY:
    {
        NMHDR* pNmh = reinterpret_cast<NMHDR*>(lParam);
        // 选区发生任何变化：单选/多选/Shift/CTRL+A全选全部触发
        if (pNmh->code == EN_SELCHANGE && !s_bProcessingMsg)
        {

            DWORD selStart = 0, selEnd = 0;
            //NppSSH_LogInfoAuto("【EM_GETSEL 执行前】准备获取选区");
            SendMessageW(hWnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            //NppSSH_LogInfoAuto("【原始选区 selStart=" + IntToStr(selStart) + " , selEnd=" + IntToStr(selEnd) + "】");
            const DWORD selLen = selEnd - selStart;
            std::wstring selText(selLen, 0);
            if (selStart == selEnd)
            {
                //NppSSH_LogInfoAuto("【选区为空 selStart==selEnd，直接退出】");
                goto endNotifyProc;
            }

            
            NppSSH_LogInfoAuto("【选中长度 selLen=" + IntToStr(selLen) + "】");

            if (selLen <= 0)
            {
                NppSSH_LogInfoAuto("【异常：选区长度<=0，放弃处理】");
                goto endNotifyProc;
            }

            // 读取选中的文本内容
            
            NppSSH_LogInfoAuto("【创建selText完毕，容量=" + IntToStr(selText.size()) + "】");
            const LRESULT retGetText = SendMessageW(hWnd, EM_GETSELTEXT, (WPARAM)selText.size(), (LPARAM)selText.data());
            NppSSH_LogInfoAuto("【EM_GETSELTEXT 返回值=" + IntToStr(retGetText) + "】");

            // 场景1：选中内容纯换行（无可见字符）→ 直接取消选中
            if (retGetText == 0)
            {
                NppSSH_LogInfoAuto("【选中内容为空，纯隐藏换行，直接取消选中】");
                s_bProcessingMsg = true;
                SendMessageW(hWnd, EM_SETSEL, selStart, selStart);
                s_bProcessingMsg = false;
                NppSSH_LogInfoAuto("【空换行拦截处理完毕】");
                goto endNotifyProc;
            }

            // 核心：计算剔除所有换行符后的有效选中终点
            const DWORD validEndInSel = GetValidSelEnd(selText, 0, selLen);
            NppSSH_LogInfoAuto("【GetValidSelEnd 返回 validEndInSel=" + IntToStr(validEndInSel) + "】");

            // 场景2：单字符选中且为换行 → 取消选中
            if (selLen == 1 && validEndInSel == 0)
            {
                NppSSH_LogInfoAuto("【单字符选中为换行，直接取消选中】");
                s_bProcessingMsg = true;
                SendMessageW(hWnd, EM_SETSEL, selStart, selStart);
                s_bProcessingMsg = false;
                NppSSH_LogInfoAuto("【单换行字符拦截处理结束】");
                goto endNotifyProc;
            }

            // 计算全局有效选中终点（原始起始 + 选区内有效长度）
            const DWORD finalSelEnd = selStart + validEndInSel;
            NppSSH_LogInfoAuto("【计算修正后finalSelEnd=" + IntToStr(finalSelEnd) + "】");

            // 场景3：需要修正选区（有效终点 < 原始终点）
            if (finalSelEnd < selEnd)
            {
                NppSSH_LogInfoAuto("【需要修正选区：剔除中间/末尾换行符】");
                s_bProcessingMsg = true;
                SendMessageW(hWnd, EM_SETSEL, selStart, finalSelEnd);
                s_bProcessingMsg = false;
                NppSSH_LogInfoAuto("【EM_SETSEL执行完毕，修正后选区：" + IntToStr(selStart) + " - " + IntToStr(finalSelEnd) + "】");
            }
            else
            {
                NppSSH_LogInfoAuto("【无需修正，选区无隐藏换行符】");
            }

            NppSSH_LogInfoAuto("【选区修正完成】原始len=" + IntToStr(selLen) + " 有效len=" + IntToStr(validEndInSel));

        endNotifyProc:
            ;
        }

        // 不要return 0阻断消息，原样转交原过程
        res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
        return res;
    }
    break;
    
    case WM_GETDLGCODE:
        NppSSH_LogInfoAuto("【完全拦截】处理键盘消息");
        res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
        s_bProcessingMsg = false;
        //return res | DLGC_WANTCHARS | DLGC_WANTMESSAGE | DLGC_HASSETSEL;
        return res | DLGC_WANTCHARS;
    case WM_SETFOCUS:
        s_bProcessingMsg = false;
        break;
    }

    try {
        NppSSH_LogInfoAuto("TerminalEditProc监听！msg=" + IntToStr(msg) + " hWnd=" + PtrToHexStr(hWnd));

        if (msg == WM_CONTEXTMENU)
        {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            HMENU hPop = CreatePopupMenu();
            AppendMenu(hPop, MF_STRING, ID_UNDO, L"撤销");
            AppendMenu(hPop, MF_STRING, ID_CUT, L"剪切");
            AppendMenu(hPop, MF_STRING, ID_COPY, L"复制");
            AppendMenu(hPop, MF_STRING, ID_PASTE, L"粘贴");
            AppendMenu(hPop, MF_SEPARATOR, 0, L"");
            AppendMenu(hPop, MF_STRING, ID_SELECTALL, L"全选");
            UINT nSel = TrackPopupMenu(hPop, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hPop);
            switch (nSel)
            {
            case ID_PASTE: SendMessageW(hWnd, WM_PASTE, 0, 0); break;
            case ID_COPY: SendMessageW(hWnd, WM_COPY, 0, 0); break;
            case ID_CUT: SendMessageW(hWnd, WM_CUT, 0, 0); break;
            case ID_UNDO: SendMessageW(hWnd, EM_UNDO, 0, 0); break;
            case ID_SELECTALL:
                SendMessageW(hWnd, EM_SETSEL, 0, -1);
                if (!s_bProcessingMsg)
                    PostMessageW(hWnd, MSG_FIX_SELECT_TRAIL_NEWLINE, 0, 1);
                break;
            }
            return 0;
        }
        // 选区发生任何变化：单选/多选/Shift/CTRL+A全选全部触发
        if (lParam == EN_SELCHANGE && !s_bProcessingMsg)
        {
            //NppSSH_LogInfoAuto("EN_SELCHANGE触发选区修正");
            PostMessageW(hWnd, MSG_FIX_SELECT_TRAIL_NEWLINE, 0, 1);
            res = 0;
            s_bProcessingMsg = false;
            return res;
        }

        // 1. 全局放行复制操作
        bool isCopy = (msg == WM_KEYDOWN &&
            ((GetKeyState(VK_CONTROL) < 0 && wParam == 'C') ||
                (GetKeyState(VK_CONTROL) < 0 && wParam == VK_INSERT)));
        if (isCopy) {
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            NppSSH_LogInfoAuto("【放行】全局复制操作！msg=" + IntToStr(msg) + " wParam=" + IntToStr(wParam));
            s_bProcessingMsg = false;
            return res;
        }

        // 2. 检查是否在可编辑区域
        bool canEdit = terminal->IsCursorInEditableArea();

        // 3. 左右方向键放行
        if (msg == WM_KEYDOWN && (wParam == VK_LEFT || wParam == VK_RIGHT)) {
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            NppSSH_LogInfoAuto("【放行】左右方向键操作！wParam=" + IntToStr(wParam));
            s_bProcessingMsg = false;
            return res;
        }

        // 4. 上下方向键拦截
        if (msg == WM_KEYDOWN && (wParam == VK_UP || wParam == VK_DOWN) && canEdit) {
            if (terminal->GetPTYFeatures().supportCursorMove) {
                NppSSH_LogInfoAuto("调用远程服务器的历史记录，待实现去远程服务查询历史命令");
                NppSSH_LogInfoAuto("【拦截】上下方向键禁止操作！wParam=" + IntToStr(wParam));
                res = 0;
            }
            else {
                // 不支持光标移动的PTY（如dumb），放行上下键
                res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            }
            s_bProcessingMsg = false;
            return res;
        }

        // 新增：Ctrl+V / Shift+Insert 粘贴快捷键捕获，对齐Ctrl+C逻辑
        bool isPasteHotkey = (msg == WM_KEYDOWN &&
            ((GetKeyState(VK_CONTROL) < 0 && wParam == 'V') ||
                (GetKeyState(VK_SHIFT) < 0 && wParam == VK_INSERT)));
        // 5. 粘贴处理
        if ((msg == WM_PASTE || isPasteHotkey) && terminal->IsCursorInEditableArea()) {
            NppSSH_LogInfoAuto("【进入粘贴处理】");
            std::wstring pasteStr;
            //1、直接从剪贴板取出粘贴内容
            if (OpenClipboard(hWnd))
            {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData != nullptr)
                {
                    LPWSTR pBuf = (LPWSTR)GlobalLock(hData);
                    if (pBuf) pasteStr = pBuf;
                    GlobalUnlock(hData);
                }
                CloseClipboard();
            }
            // 无粘贴内容直接放行原生逻辑
            if (pasteStr.empty())
            {
                res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
                s_bProcessingMsg = false;
                return res;
            }

            //2、截取当前行、提取命令
            std::wstring allText = terminal->GetStoreContent();
            allText = CleanrrW(allText);
            DWORD cursorPos = 0;
            SendMessageW(hWnd, EM_GETSEL, (WPARAM)&cursorPos, NULL);
            size_t lineStart = 0;
            for (size_t i = cursorPos; i > 0; --i) {
                if (allText[i] == L'\n' || allText[i] == L'\r') { lineStart = i + 1; break; }
            }

            std::string promptStr = terminal->GetPrompt();
            std::wstring wPrompt = UTF8ToWstring(promptStr);
            int promptLen = (int)wPrompt.length();
            int cursorInCmdPos = (int)(cursorPos - (lineStart + promptLen));
            std::string currentCmd = terminal->GetCmd();

            // 起始下标边界修正
            int insertIdx = cursorInCmdPos;
            const int maxLegalPos = static_cast<int>(currentCmd.size());
            if (insertIdx < 0) insertIdx = 0;
            if (insertIdx > maxLegalPos) insertIdx = maxLegalPos;

            for (wchar_t wch : pasteStr)
            {
                if (wch == L'\r' || wch == L'\n')
                    continue;
                char ch = static_cast<char>(wch);
                if (insertIdx >= 0 && insertIdx <= (int)currentCmd.size())
                {
                    currentCmd.insert(insertIdx, 1, ch);
                    insertIdx++;
                    NppSSH_LogInfoAuto("【粘贴同步cmd插入字符：" + std::string(1, ch) + "】");
                }
                if (insertIdx > (int)currentCmd.size())
                    insertIdx = static_cast<int>(currentCmd.size());
            }
            terminal->SetCmd(currentCmd.c_str());
            NppSSH_LogInfoAuto("【粘贴同步cmd】最终：" + currentCmd);

            //4、手动把粘贴文本写入编辑框
            //int pos = GetWindowTextLengthW(hWnd);
            //SendMessageW(hWnd, EM_SETSEL, pos, pos);
            //SendMessageW(hWnd, EM_REPLACESEL, FALSE, (LPARAM)pasteStr.c_str());
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            s_bProcessingMsg = false;
            return res;
        }

        bool isCutHotkey = (msg == WM_KEYDOWN &&
            ((GetKeyState(VK_CONTROL) < 0 && wParam == 'X') ||
                (GetKeyState(VK_DELETE) && GetKeyState(VK_SHIFT) < 0)));
        if ((msg == WM_CUT || isPasteHotkey) && terminal->IsCursorInEditableArea()) {
            NppSSH_LogInfoAuto("【进入剪切处理】");

            //2、截取当前行、提取命令
            std::wstring allText = terminal->GetStoreContent();
            allText = CleanrrW(allText);
            DWORD cursorPos = 0;
            SendMessageW(hWnd, EM_GETSEL, (WPARAM)&cursorPos, NULL);
            size_t lineStart = 0;
            for (size_t i = cursorPos; i > 0; --i) {
                if (allText[i] == L'\n' || allText[i] == L'\r') { lineStart = i + 1; break; }
            }

            std::string promptStr = terminal->GetPrompt();
            std::wstring wPrompt = UTF8ToWstring(promptStr);
            int promptLen = (int)wPrompt.length();
            int cursorInCmdPos = (int)(cursorPos - (lineStart + promptLen));
            std::string currentCmd = terminal->GetCmd();

            // 起始下标边界修正
            int insertIdx = cursorInCmdPos;
            const int maxLegalPos = static_cast<int>(currentCmd.size());
            if (insertIdx < 0) insertIdx = 0;
            if (insertIdx > maxLegalPos) insertIdx = maxLegalPos;
            terminal->SetCmd(currentCmd.c_str());
            NppSSH_LogInfoAuto("【剪切同步cmd】最终：" + currentCmd);
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            s_bProcessingMsg = false;
            return res;
        }

        // 6. 回车处理
        if (msg == WM_KEYDOWN && (wParam == VK_RETURN || wParam == 13)) {
            canEdit = terminal->IsCursorInEditableArea();
            //if (!canEdit) {
            //    // 临时关闭只读，让系统处理输入
            //    SendMessageW(hWnd, EM_SETREADONLY, FALSE, 0);//废除
            //    res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            //    // 临时关闭只读，让系统处理输入
            //    SendMessageW(hWnd, EM_SETREADONLY, TRUE, 0);//废除
            //    s_bProcessingMsg = false;
            //    return res;
            //}

            // ============= 【从伪终端提取真实命令】=============
            DWORD cursorPos = 0;
            SendMessageW(hWnd, EM_GETSEL, (WPARAM)&cursorPos, NULL);

            // 执行
            NppSSH_LogInfoAuto("【执行】回车触发命令执行！光标位置=" + IntToStr((int)cursorPos)
                + "命令===" + terminal->GetCmd() + "命令提示符===" + terminal->GetPrompt());

            std::string cmdToExecute = terminal->GetCmd();
            if (cmdToExecute.empty()) {
                NppSSH_LogInfoAuto("【跳过】无命令可执行，仅换行");
                SSHTerminal_AppendOutput(terminal->GetPanelId(), "\r\n" + terminal->GetPrompt());
                res = 0;
                s_bProcessingMsg = false;
                return res;
            }

            // 执行命令
            terminal->SetIsCommandRunning(true); // 标记后台命令开始执行
            // 立即放行，不等待
            std::string cmdCopy = cmdToExecute;
            int panelId = terminal->GetPanelId();
            terminal->StoreTerminalContent(cmdCopy);
            
            std::thread([panelId, cmdCopy]() {
                bool result = NppSSH_ExecuteCommand(panelId, cmdCopy);
                }).detach();
            //terminal->SetPrompt(NppSSH_PanelPrompt(terminal->GetPanelId()));
            NppSSH_LogInfoAuto("【调试】TerminalEditProc设置提示符，命令提示符====" + terminal->GetPrompt());
            NppSSH_LogInfoAuto("【命令执行结果】面板ID=" + IntToStr(terminal->GetPanelId())
                + " 命令=" + cmdToExecute + " ，命令提示符====" + terminal->GetPrompt());

            // 清空命令缓存
            terminal->SetCmd("");

            res = 0;
            s_bProcessingMsg = false;
            return res;
        }

         //7. 退格保护
        bool isBackspaceAtPromptEnd = false;
        if ((msg == WM_KEYDOWN && wParam == VK_BACK) || (msg == WM_CHAR && wParam == 8)) {
            DWORD selStart = 0;
            ::SendMessageW(hWnd, EM_GETSEL, (WPARAM)&selStart, NULL);
            DWORD cursorPos = selStart;
            std::wstring promptW = UTF8ToWstring(terminal->GetPrompt());
            int promptLen = (int)promptW.length();
            //int totalLen = ::GetWindowTextLengthW(hWnd);
            //std::wstring allText;
            //allText.resize(totalLen + 1);
            //::GetWindowTextW(hWnd, &allText[0], totalLen + 1);
            //allText = CleanrrW(allText);
            std::wstring allText = terminal -> GetStoreContent();
            allText = CleanrrW(allText);
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
            NppSSH_LogInfoAuto("【拦截】命令提示符=" + terminal->GetPrompt() + "，禁止删除prompt末尾字符！");
            res = 0;
            s_bProcessingMsg = false;
            return res;
        }

        // 8. 删除键逻辑
        bool isDeleteKey = (msg == WM_KEYDOWN && (wParam == VK_BACK || wParam == VK_DELETE));
        if (isDeleteKey) {
            DWORD selStart = 0, selEnd = 0;
            ::SendMessageW(hWnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            DWORD cursorPos = selStart;
            std::string promptStr = terminal->GetPrompt();
            std::string cmdStr = terminal->GetCmd();
            std::wstring wPrompt = UTF8ToWstring(promptStr);
            int promptLen = (int)wPrompt.length();
            int cmdLen = (int)UTF8ToWstring(cmdStr).length();
            //int totalLen = ::GetWindowTextLengthW(hWnd);
            //std::wstring allText;
            //allText.resize(totalLen + 1);
            //::GetWindowTextW(hWnd, &allText[0], totalLen + 1);
            //allText = CleanrrW(allText);
            std::wstring allText = terminal->GetStoreContent();
            allText = CleanrrW(allText);
            size_t lineStart = 0;
            for (size_t i = cursorPos; i > 0; --i) {
                if (allText[i] == L'\n' || allText[i] == L'\r') {
                    lineStart = i + 1; break;
                }
            }
            size_t lineEnd = allText.find_first_of(L"\n", cursorPos);
            if (lineEnd == std::wstring::npos) lineEnd = allText.length();
            std::wstring currentLine = allText.substr(lineStart, lineEnd - lineStart);
            size_t promptEndPosInLine = lineStart + promptLen;
            bool willModifyPrompt = false;
            if (wParam == VK_BACK) { willModifyPrompt = (cursorPos <= promptEndPosInLine); }
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
            NppSSH_LogInfoAuto("【提示】当前命令=="+currentCmd);
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
            // ✅ 关键修复：临时禁用防重入标记，确保编辑框能完整处理删除操作
            //bool wasProcessing = s_bProcessingMsg;
            //s_bProcessingMsg = false;  // 临时禁用，让编辑框能处理所有相关消息
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            //s_bProcessingMsg = wasProcessing;  // 恢复原来的防重入状态
            s_bProcessingMsg = false;

            return res;
        }

        // 9. 字符输入 - 核心修复
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
                std::wstring wPrompt = UTF8ToWstring(promptStr);
                int promptLen = (int)wPrompt.length();
                //int totalLen = ::GetWindowTextLengthW(hWnd);
                //std::wstring allText;
                //allText.resize(totalLen + 1);
                //::GetWindowTextW(hWnd, &allText[0], totalLen + 1);
                //allText = CleanrrW(allText);
                std::wstring allText = terminal->GetStoreContent();
                allText = CleanrrW(allText);
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

                // 直接调用原过程，不要额外处理
                res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
                NppSSH_LogInfoAuto("【放行】可编辑区域字符输入！currentCmd.c_str()====" + currentCmd);
            }
            s_bProcessingMsg = false;
            return res;
        }

        // 非字符输入的其他消息
        else if (!canEdit) {
            if (isKeyboardMsg) {
                NppSSH_LogInfoAuto("【拦截】非可编辑区域，禁止操作！msg=" + IntToStr(msg) + " wParam=" + IntToStr(wParam));
                res = 0;
            }
            else {
                res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
                NppSSH_LogInfoAuto("【非字符输入最终放行】可编辑区域合法操作！msg=" + IntToStr(msg) + " wParam=" + IntToStr(wParam));
            }
        }
        else {
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            NppSSH_LogInfoAuto("【放行】可编辑区域合法操作！msg=" + IntToStr(msg) + " wParam=" + IntToStr(wParam));
        }

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
    // 1. 加载库
    // 放弃 MsftEdit.dll，直接使用系统自带的标准 RichEdit 控件
    //_hRichEditLib = LoadLibraryW(L"Riched20.dll");
    _hRichEditLib = LoadLibraryW(L"Msftedit.dll");;
    if (!_hRichEditLib)
    {
        MessageBoxW(s_nppData._nppHandle, L"无法加载系统 RichEdit 库（Riched20.dll）！", L"NppSSH调试提示", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    NppSSH_LogInfoAuto("成功加载系统标准 Riched20.dll");

    // 保存父窗口（必须！解决 _hSelf 为空导致的崩溃）
    _hwndParent = hParent;

    RECT rc;
    if (!::GetClientRect(_hwndParent, &rc)) {
        return nullptr;
    }
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
    // 2. 正式创建控件 (使用 s_hInst)style |= ES_MULTILINE | ES_AUTOHSCROLL | WS_VSCROLL;
    _hTerminal = CreateWindowExW(
        WS_EX_CLIENTEDGE | WS_EX_NOPARENTNOTIFY | WS_EX_ACCEPTFILES,
        MSFTEDIT_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOHSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
        x, y, cx, cy,
        _hwndParent,
        (HMENU)IDC_OUTPUT_EDIT,
        s_hInst,
        this
    );
    if (!_hTerminal) {
        DWORD err = GetLastError();
        wchar_t errMsg[256];
        if (err == ERROR_CANNOT_FIND_WND_CLASS) {
            swprintf(errMsg, L"创建终端控件底层失败！错误码: %d (找不到RichEdit20W窗口类，可能是库加载失败或类未注册)", err);
        }
        else {
            swprintf(errMsg, L"创建终端控件底层失败！错误码: %d", err);
        }
        MessageBoxW(s_nppData._nppHandle, errMsg, L"NppSSH调试提示", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    // WS_EX_TRANSPARENT + 控件默认风格 不支持 IME
    //_hTerminal = ::CreateWindowExW(//ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_TABSTOP
    //    WS_EX_CLIENTEDGE | WS_EX_NOPARENTNOTIFY | WS_EX_ACCEPTFILES,
    //    L"RichEdit20W",
    //    L"初始化成功", // 文字设为空
    //    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
    //    x, y, cx, cy,// 初始大小
    //    _hwndParent,
    //    (HMENU)IDC_OUTPUT_EDIT,
    //    s_hInst, // 用全局插件实例句柄
    //    this
    //);
    //开启RichEdit内置右键菜单 
    //SendMessage(_hTerminal, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS);

    
    

    // 创建控件后，设置字符集
    CHARFORMAT2W cf = { 0 };
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_CHARSET;
    //cf.bCharSet = CP_UTF8; // 强制 UTF-8 字符集
    //cf.bCharSet = DEFAULT_CHARSET;
    cf.bCharSet = GB2312_CHARSET;
    SendMessageW(_hTerminal, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    // 设置样式
    //DWORD style = ::GetWindowLongPtrW(_hTerminal, GWL_STYLE);
    //style |= ES_MULTILINE | ES_AUTOHSCROLL | WS_VSCROLL;
    //::SetWindowLongPtrW(_hTerminal, GWL_STYLE, style);
    int fontSize = 28;
    // 设置默认字体（等宽字体，适配终端）
    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Courier New");// 备选："Courier New"、"Lucida Console"、"Microsoft YaHei"
    SendMessageW(_hTerminal, WM_SETFONT, (WPARAM)hFont, TRUE);
    // 关闭横向自动滚动：超出宽度下沉换行，禁用水平滚动条
    SendMessageW(_hTerminal, EM_SETTARGETDEVICE, 0, 0);

    PARAFORMAT2 pf = { 0 };
    pf.cbSize = sizeof(PARAFORMAT2);
    // 启用：对齐+行间距，其余全部关闭
    pf.dwMask = PFM_ALIGNMENT | PFM_LINESPACING | PFM_SPACEBEFORE | PFM_SPACEAFTER;
    pf.wAlignment = PFA_LEFT;
    // 段前段后0空白，杜绝自动空行
    pf.dySpaceBefore = 0;
    pf.dySpaceAfter = 0;
    // 固定紧凑行高，适配Courier New等宽终端字体
    pf.bLineSpacingRule = 3;
    // 换算字体高度（字体30pt，行高匹配）
    pf.dyLineSpacing = MulDiv(fontSize, 1440, 96);
    // 缩进全部关闭
    pf.dxStartIndent = 0;
    pf.dxRightIndent = 0;
    pf.dxOffset = 0;

    SendMessageW(_hTerminal, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
    // 仅保留开启富文本，启用颜色渲染【关键】
    SendMessageW(_hTerminal, EM_SETTEXTMODE, TM_RICHTEXT, 0);

    // 按位或 追加需要的事件（不覆盖原有）
    // 1、事件掩码 EVENTMASK：只加ENM_系列，管控WM_NOTIFY通知
    DWORD dwMask = SendMessage(_hTerminal, EM_GETEVENTMASK, 0, 0);
    dwMask |= ENM_MOUSEEVENTS;   // 保留右键、鼠标事件（原有需求：内置右键菜单）
    dwMask |= ENM_SELCHANGE;     // 选区变化通知（用来自动修正末尾换行）
    dwMask |= ENM_SCROLL;
    dwMask |= ENM_PROTECTED;
    //dwMask &= ~(SES_UPPERCASE | SES_LOWERCASE | SES_BIDI);
    //dwMask &= ~SES_USECRLF;         // 禁用旧CRLF模式（废弃标记，配套关闭）
    //dwMask &= ~(SES_UPPERCASE | SES_LOWERCASE | SES_BIDI);//关闭自动大小写、无用输入限制
    // 3.写回掩码
    SendMessage(_hTerminal, EM_SETEVENTMASK, 0, dwMask);

    //2、 编辑样式 EDITSTYLE：只加SES_系列，管控输入/IME/换行 
    DWORD dwEditStyle = SendMessage(_hTerminal, EM_GETEDITSTYLE, 0, 0);
    dwEditStyle |= SES_USECTF;        //启用TSF微软拼音(可切换中英文)
    //dwEditStyle |= SES_NOIME; 
    dwEditStyle |= SES_XLTCRCRLFTOCR; //用户回车\r\n自动转\n
    dwEditStyle &= ~(SES_UPPERCASE | SES_LOWERCASE | SES_BIDI);//关闭自动大小写、双向文字
    SendMessage(_hTerminal, EM_SETEDITSTYLE, dwEditStyle, 0);


    SetPTYType("xterm-256color");// 初始化默认PTY类型（可从配置/用户选择动态修改）
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

// 设置当前PTY类型（自动加载对应特性）
void SSHTerminal::SetPTYType(const std::string& ptyType) {
    _currentPTYType = ptyType;
    // 查找特性配置，找不到则用dumb（最基础）
    auto it = g_ptyFeatureMap.find(ptyType);
    if (it != g_ptyFeatureMap.end()) {
        _currentPTYFeatures = it->second;
    }
    else {
        _currentPTYFeatures = g_ptyFeatureMap["dumb"];
    }
    NppSSH_LogInfoAuto("【PTY适配】切换到" + ptyType +
        "，ANSI支持：" + std::to_string(_currentPTYFeatures.supportANSI) +
        "，256色：" + std::to_string(_currentPTYFeatures.support256Color));
}
// 获取当前PTY特性（对外提供只读访问）
const PTYFeatures& SSHTerminal::GetPTYFeatures() const {
    return _currentPTYFeatures;
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

// 宽字符版本调试打印
void DeBugOutPutText(const std::wstring& text) {
    std::wstring rawCharLog = L"[宽字符text全字符拆解] 总字节数=" + UTF8ToWstring(IntToStr((int)text.size())) + L" | 字符序列：";

    for (wchar_t ch : text)
    {
        switch (ch)
        {
        case L'\r': rawCharLog += L"\\r "; break;
        case L'\n': rawCharLog += L"\\n "; break;
        case L'\t': rawCharLog += L"\\t "; break;
        case L' ':  rawCharLog += L"SP "; break;
        default:
            if (ch < 0x20 || ch >= 0x7F)
            {
                // 不可见控制字符，打印十六进制
                wchar_t buf[16] = { 0 };
                swprintf_s(buf, L"0x%04X ", (DWORD)ch);
                rawCharLog += buf;
            }
            else
            {
                // 普通可见字符
                rawCharLog += (wchar_t)ch;
                rawCharLog += L" ";
            }
            break;
        }
    }

    // 宽字符转窄字符日志输出
    NppSSH_LogInfoAuto(WStringToLogStr(rawCharLog));
    NppSSH_LogInfoAuto("输出文本到输出框(宽字符): " + WStringToLogStr(text));
}
void DeBugOutPutText(const std::string& text) {
    // ======================【新增：完整字符日志打印，解析所有转义符号】======================
    std::string rawCharLog = "[原始text全字符拆解] 总字节数=" + IntToStr((int)text.size()) + " | 字符序列：";
    for (unsigned char ch : text)
    {
        switch (ch)
        {
        case '\r': rawCharLog += "\\r "; break;
        case '\n': rawCharLog += "\\n "; break;
        case '\t': rawCharLog += "\\t "; break;
        case ' ':  rawCharLog += "SP "; break;
        default:
            if (ch < 0x20 || ch >= 0x7F)
            {
                // 不可见控制字符，打印十六进制
                char buf[16] = { 0 };
                sprintf_s(buf, "0x%02X ", ch);
                rawCharLog += buf;
            }
            else
            {
                // 普通可见字符
                rawCharLog += (char)ch;
                rawCharLog += " ";
            }
            break;
        }
    }
    NppSSH_LogInfoAuto(rawCharLog);
    NppSSH_LogInfoAuto("输出文本到输出框" + std::string(text));
    // ==================================================================================
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


    //std::string cleanText = CleanAnsiEscapeSequences(text);
    std::string cleanText = text;

    //cleanText = Cleanrr(cleanText);//清除所有\n前面的\r
    
    // 1、转宽字符
    std::wstring rawW = UTF8ToWstring(cleanText);
    // 2、统一换行预处理（删除全部\r、合并连续\n）
    std::wstring normW = NormalizeTerminalLineFeed(rawW);
    std::wstring* wtext = new std::wstring(normW);
    NppSSH_LogInfoAuto("【原始宽字符内容】" + WStringToLogStr(*wtext));

    //NppSSH_LogInfoAuto("清除输出文本到输出框前");
    //DeBugOutPutText(text);
    //NppSSH_LogInfoAuto("清除输出文本到输出框后");
    //DeBugOutPutText(cleanText);
    NppSSH_LogInfoAuto("原始宽字符内容");
    DeBugOutPutText(wtext->c_str());
    NppSSH_LogInfoAuto("【原始宽字符内容清除后】");
    DeBugOutPutText(normW);
    // ✅ 投递到主线程（绝对安全）

    PostMessage(_hTerminal, WM_APPEND_OUTPUT_TEXT, 0, (LPARAM)wtext);
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
    DWORD cpMin = 0, cpMax = 0;
    ::SendMessageW(_hTerminal, EM_GETSEL, (WPARAM)&cpMin, (LPARAM)&cpMax);
    DWORD cursorPos = cpMin;
    std::wstring allText = GetStoreContent();
    allText = CleanrrW(allText);// 删掉 RichEdit 自动加的所有 \r
    DeBugOutPutText(allText);
    // 3. 找光标所在行
    size_t lastLineStart = 0;
    // 从后往前找，跳过所有空白、换行、不可见字符
    for (size_t i = cursorPos; i > 0; --i) {
        if (allText[i] == L'\n' || allText[i] == L'\r') {
            lastLineStart = i + 1; break;
        }
    }
    DeBugOutPutText(allText);
    std::string loginfoDebug = "【可编辑判定调试】 "
        "_prompt=[" + _prompt + "] "
        "lastLineStart=[" + IntToStr(lastLineStart) + "] "
        "全局光标=[" + IntToStr((int)cursorPos) + "] ";

    NppSSH_LogInfoAuto(loginfoDebug);
    // 光标不在最后一行 → 不可编辑
    if (cursorPos < lastLineStart) {
        NppSSH_LogInfoAuto("[可编辑判定] 光标不在最后一行，禁止编辑");
        return false;
    }
    // 只取最后一行内容，在本行内计算！
    std::wstring lastLine = allText.substr(lastLineStart);

    // 6. 处理提示符（清理乱码）
    //std::wstring promptW = CleanAnsiEscapeSequences(UTF8ToWstring(_prompt));
    std::wstring promptW = (UTF8ToWstring(_prompt));
    int promptLen = promptW.size();

    if (promptLen <= 0) {
        NppSSH_LogInfoAuto("[可编辑判定] 清理后提示符为空，禁止编辑");
        return false;
    }


    // 6. 最后一行必须以提示符开头
    // 判断最后一行是否以提示符开头
    bool lineStartsWithPrompt = false;
    std::wstring prefix;
    if (lastLine.size() >= promptLen) {
        prefix = lastLine.substr(0, promptLen);
        //prefix = CleanAnsiEscapeSequences(prefix);
        lineStartsWithPrompt = (prefix == promptW);
    }
    if (!lineStartsWithPrompt) {
        NppSSH_LogInfoAuto("prefix = " + WStringToLogStr(prefix) + ",promptW=" + WStringToLogStr(promptW)+ ",提示符长度=[" + IntToStr(promptLen) + "] ");

        NppSSH_LogInfoAuto("[可编辑判定] 最后一行不以提示符开头");
        return false;
    }
    int cursorInLine = cursorPos - lastLineStart;
    bool cursorIsAfterPrompt = (cursorInLine >= promptLen);
    // 结合_cmd校验：如果已有命令，光标需在命令范围内（增强校验）
    bool cmdAreaValid = true;
    std::wstring cmdW = UTF8ToWstring(_cmd);
    if (!cmdW.empty()) {
        int cmdEndInLine = promptLen + cmdW.size();
        cmdAreaValid = (cursorInLine >= promptLen && cursorInLine <= cmdEndInLine);
    }

    bool canEdit = cursorIsAfterPrompt && cmdAreaValid;
    
    std::string loginfo = "【可编辑判定】 "
        "_prompt=[" + _prompt + "] "
        "lastLineStart=[" + IntToStr(lastLineStart) + "] "
        "全局光标=[" + IntToStr((int)cursorPos) + "] "
        "本行光标=[" + IntToStr(cursorInLine) + "]本行光标 = 全局光标 - lastLineStart"
        "提示符长度=[" + IntToStr(promptLen) + "] "
        "本行开头匹配=[" + IntToStr(lineStartsWithPrompt) + "] "
        "光标在提示符后=[" + IntToStr(cursorIsAfterPrompt) + "] "
        "可编辑=[" + IntToStr(canEdit ? 1 : 0) + "]";

    NppSSH_LogInfoAuto(loginfo);

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
    //std::string fixedText;
    //for (size_t i = 0; i < text.length(); i++) {
    //    if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) {
    //        fixedText += "\r\n";
    //    }
    //    else {
    //        fixedText += text[i];
    //    }
    //}

    //// 处理开头的换行
    //// 判断是否以 \r\n 开头，如果不是，则在开头追加
    //bool hasLeadingNewLine = false;
    //if (fixedText.size() >= 2) {
    //    if (fixedText[0] == '\r' && fixedText[1] == '\n') {
    //        hasLeadingNewLine = true;
    //    }
    //}

    //// 如果没有开头换行，则追加
    //if (!hasLeadingNewLine && !fixedText.empty()) {
    //    fixedText.insert(0, "\r\n");
    //    NppSSH_LogInfoAuto("【自动换行】在输出开头追加 \\r\\n");
    //}

    panel->AppendOutputText(text);

}
void SSHTerminal_PanelPrompt(int panelIndex, const std::string Prompt) {
    NppSSH_LogInfoAuto("【调试】SSHTerminal_PanelPrompt设置提示符");
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    std::string prompt = Prompt;
    prompt = CleanAnsiEscapeSequences(prompt);
    panel->SetPrompt(prompt);
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
            NppSSH_LogInfoAuto("【命令完全结束】恢复伪终端焦点，可直接输入");

            // ✅ 新增：强制刷新可编辑状态
            PostMessageW(hEdit, WM_KEYDOWN, VK_F5, 0);
        }
    }
}
/*
* 强制改为英文
*/
void SSHTerminal_SetEnglishType(int panelIndex) {
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    if (panel)
    {
        HWND TerminalHWND = panel->Get_TerminalHandle();
        panel->SetCmd("");
        imm_chineseType(TerminalHWND);
    }
}
void SSHTerminal_ClearOutputText(int panelIndex) {

    SSHTerminal* panel = getSSHTerminal(panelIndex);
    if (panel)
    {
        HWND TerminalHWND = panel->Get_TerminalHandle();
        ::SetWindowTextW(TerminalHWND, L"");
        panel->SetStoreContent(L"");
    }
}
std::string SSHTerminal_getPanelPrompt(int panelIndex) {
    SSHTerminal* panel = getSSHTerminal(panelIndex);
    return panel->GetPrompt();
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
