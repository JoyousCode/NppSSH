// SSHTermHandle.cpp模拟终端，具体实现
#include "SSHTermHandle.h"

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
            //if (curPos + 1 < selEnd && fullText[curPos + 1] == L'\n')
            if ((curPos + 1 < selEnd) && (curPos + 1 < txtLen) && fullText[curPos + 1] == L'\n')
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
    for (wchar_t c : allText) {
        if (c == L'\r') continue; // 删掉 RichEdit 自动加的所有 \r
        fixedText += c;
    }
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
void SSHTermHandle::ParseAnsiParseOnly(const std::wstring& params, CHARFORMAT2W& outCf)
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
void SSHTermHandle::StoreTerminalContent(wchar_t ch)
{
    _oldStoreContent += ch; // 无需判空，string自动扩容，只追加
    NppSSH_LogInfoAuto("存储最后一行：" + WStringToLogStr(_oldStoreContent));
}
void SSHTermHandle::StoreTerminalContent(const std::string& str)
{
    if (str.empty())
        return;
    std::wstring wStr = UTF8ToWstring(str);
    _oldStoreContent += wStr;
}

//void SSHTermHandle::StoreTerminalContent(wchar_t ch)
//{
//    //_oldStoreContent += ch; // 旧：全文累加，删掉
//    if (ch == L'\n' || ch == L'\r')
//    {
//        // 换行：结束当前行，清空缓存，准备新一行
//        _oldStoreContent.clear();
//    }
//    else
//    {
//        // 普通字符：追加到【仅最后一行缓存】
//        _oldStoreContent += ch;
//    }
//}
//
//void SSHTermHandle::StoreTerminalContent(const std::string& str)
//{
//    if (str.empty())
//        return;
//    std::wstring wStr = UTF8ToWstring(str);
//    //_oldStoreContent += wStr; //旧全文拼接，替换下面逻辑
//    for (wchar_t ch : wStr)
//    {
//        StoreTerminalContent(ch); // 复用单字符版本，统一逻辑
//    }
//}

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
LRESULT CALLBACK SSHTermHandle::TerminalEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LRESULT res = 0;
    wchar_t buf[128]{};
    swprintf_s(buf, _countof(buf), L"SSHTermHandle-%p", hWnd);
    SSHTermHandle* terminal = (SSHTermHandle*)GetProp(hWnd, buf);

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
        msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP || msg == WM_RBUTTONDBLCLK || msg == WM_CONTEXTMENU ||
        // msg == WM_MOUSEWHEEL || //msg == WM_MOUSEMOVE ||
        // 4.核心必须加：WM_NOTIFY（EN_SELCHANGE选区通知依赖这条，不加收不到选中回调）
        msg == WM_NOTIFY || wParam == EN_SELCHANGE ||
        // 5.自定义业务消息
        msg == WM_APPEND_OUTPUT_TEXT || msg == WM_USER_RESIZE_PTY ||
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
    case WM_USER_RESIZE_PTY:
    {
        int w = (int)(LONG_PTR)GetProp(hWnd, L"NppSSH_PanelW");
        int h = (int)(LONG_PTR)GetProp(hWnd, L"NppSSH_PanelH");

        if (w <= 0 || h <= 0)
            break;

        // ✅ 字体计算
        HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
        HDC hdc = GetDC(hWnd);
        SelectObject(hdc, hFont);

        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        //NppSSH_LogInfoAuto("【WM_USER_RESIZE_PTY】宽度="+IntToStr(tm.tmAveCharWidth)+",高度=" + IntToStr(tm.tmHeight));
        int cols = w / tm.tmAveCharWidth;
        int rows = h / (tm.tmHeight + tm.tmExternalLeading);

        ReleaseDC(hWnd, hdc);
        int panelId = terminal->Get_panelSeqId();
        if (cols > 0 && rows > 0)
        {
            SSH_ConnectionPtySize(terminal->_hwndParent, cols, rows);
        }
        res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
        s_bProcessingMsg = false;
        return res;
    }
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
                            terminal->executeClear();
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

            // 直接赋值更新，读取RichEdit全部文本，截取最后一行
            std::wstring terminalLastLine;
            int totalLen = GetWindowTextLengthW(hWnd);
            if (totalLen > 0)
            {
                std::wstring fullText(totalLen + 1, 0);
                GetWindowTextW(hWnd, &fullText[0], totalLen + 1);
                size_t lastNL = fullText.find_last_of(L"\n");
                if (lastNL == std::wstring::npos)
                {
                    terminalLastLine = fullText;
                }
                else
                {
                    terminalLastLine = fullText.substr(lastNL + 1);
                }
            }
            terminal->SetStoreContent(terminalLastLine);
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
            if (retGetText >= 0)
                selText.resize(retGetText); // 裁剪为真实获取到的字符长度
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

        // Ctrl+Del 快捷键捕获（直接拦截禁用，不执行删除）
        bool isCtrlDelHotkey = (msg == WM_KEYDOWN && (GetKeyState(VK_CONTROL) < 0 && wParam == VK_DELETE));
        if (isCtrlDelHotkey && terminal->IsCursorInEditableArea())
        {
            NppSSH_LogInfoAuto("【Ctrl+Del】拦截该快捷键，禁止执行删除操作");
            res = 0;
            s_bProcessingMsg = false;
            return res;
        }

        // Ctrl+V / Shift+Insert 粘贴快捷键捕获，对齐Ctrl+C逻辑
        bool isPasteHotkey = (msg == WM_KEYDOWN &&
            ((GetKeyState(VK_CONTROL) < 0 && wParam == 'V') ||
                (GetKeyState(VK_SHIFT) < 0 && wParam == VK_INSERT)));
        // 5. 粘贴处理
        if ((msg == WM_PASTE || isPasteHotkey) && terminal->IsCursorInEditableArea())
        {
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

            // =====【改造：和字符/删除逻辑统一，复用工具函数】=====
            DWORD cursorPos = 0;
            SendMessageW(hWnd, EM_GETSEL, (WPARAM)&cursorPos, NULL);

            LONG lastLineGlobalStart = GetLastLineGlobalStart(hWnd);
            std::string promptStr = terminal->GetPrompt();
            std::wstring wPrompt = UTF8ToWstring(promptStr);
            int promptLen = static_cast<int>(wPrompt.length());

            int localCursorInLine = static_cast<int>(cursorPos) - static_cast<int>(lastLineGlobalStart);
            int cursorInCmdPos = localCursorInLine - promptLen;
            // =====================================================

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

            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            s_bProcessingMsg = false;
            return res;
        }

        bool isCutHotkey = (msg == WM_KEYDOWN &&
            ((GetKeyState(VK_CONTROL) < 0 && wParam == 'X') ||
                (GetKeyState(VK_DELETE) && GetKeyState(VK_SHIFT) < 0)));
        if ((msg == WM_CUT || isCutHotkey) && terminal->IsCursorInEditableArea())
        {
            NppSSH_LogInfoAuto("【进入剪切处理】");

            DWORD selStart = 0, selEnd = 0;
            ::SendMessageW(hWnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            DWORD cursorPos = selStart;

            // 和字符/删除/粘贴统一：获取最后一行全局起始位置
            LONG lastLineGlobalStart = GetLastLineGlobalStart(hWnd);
            std::string promptStr = terminal->GetPrompt();
            std::wstring wPrompt = UTF8ToWstring(promptStr);
            int promptLen = static_cast<int>(wPrompt.length());

            int localCursorInLine = static_cast<int>(cursorPos) - static_cast<int>(lastLineGlobalStart);
            int cursorInCmdPos = localCursorInLine - promptLen;

            // 选中区间换算为本行偏移，再映射到cmd下标
            int localSelStart = static_cast<int>(selStart) - static_cast<int>(lastLineGlobalStart);
            int localSelEnd = static_cast<int>(selEnd) - static_cast<int>(lastLineGlobalStart);

            int cmdSelBegin = localSelStart - promptLen;
            int cmdSelEnd = localSelEnd - promptLen;

            std::string currentCmd = terminal->GetCmd();
            const int cmdTotalLen = static_cast<int>(currentCmd.size());

            // 边界保护
            if (cmdSelBegin < 0) cmdSelBegin = 0;
            if (cmdSelEnd > cmdTotalLen) cmdSelEnd = cmdTotalLen;
            if (cmdSelBegin >= cmdSelEnd)
            {
                // 没有选中内容，无需修改cmd，直接放行原生剪切
                res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
                NppSSH_LogInfoAuto("【剪切调试】无选中文本，不修改cmd");
                s_bProcessingMsg = false;
                return res;
            }

            // 删除选中区间字符
            int eraseCount = cmdSelEnd - cmdSelBegin;
            currentCmd.erase(cmdSelBegin, eraseCount);
            NppSSH_LogInfoAuto("【剪切调试】删除选中字符，数量=" + IntToStr(eraseCount));

            terminal->SetCmd(currentCmd.c_str());
            NppSSH_LogInfoAuto("【剪切同步cmd】最终：" + currentCmd);

            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            s_bProcessingMsg = false;
            return res;
        }

        // 6. 回车处理
        if (msg == WM_KEYDOWN && (wParam == VK_RETURN || wParam == 13)) {

            // ============= 【从伪终端提取真实命令】=============
            DWORD cursorPos = 0;
            SendMessageW(hWnd, EM_GETSEL, (WPARAM)&cursorPos, NULL);

            // 执行
            NppSSH_LogInfoAuto("【执行】回车触发命令执行！光标位置=" + IntToStr((int)cursorPos)
                + "命令===" + terminal->GetCmd() + "命令提示符===" + terminal->GetPrompt());

            std::string cmdToExecute = terminal->GetCmd();
            if (cmdToExecute.empty()) {
                NppSSH_LogInfoAuto("【跳过】无命令可执行，仅换行");
                terminal ->  AppendOutputText("\n" + terminal->GetPrompt());
                res = 0;
                s_bProcessingMsg = false;
                return res;
            }

            // 执行命令
            terminal->SetIsCommandRunning(true); // 标记后台命令开始执行
            // 立即放行，不等待
            std::string cmdCopy = cmdToExecute;
            int panelId = terminal->Get_panelSeqId();

            std::thread([terminal, cmdCopy]() {
                bool result = SSH_ConnectionExecuteCommand(terminal->_hwndParent, cmdCopy);
                }).detach();
            NppSSH_LogInfoAuto("【调试】TerminalEditProc设置提示符，命令提示符====" + terminal->GetPrompt());
            NppSSH_LogInfoAuto("【命令执行结果】面板ID=" + IntToStr(terminal->Get_panelSeqId())
                + " 命令=" + cmdToExecute + " ，命令提示符====" + terminal->GetPrompt());

            // 清空命令缓存
            terminal->SetCmd("");
            terminal->SetStoreContent(L"");

            res = 0;
            s_bProcessingMsg = false;
            return res;
        }

        //7. 退格、删除键统一处理
        bool isDeleteKey = (msg == WM_KEYDOWN && (wParam == VK_BACK || wParam == VK_DELETE));
        if (isDeleteKey)
        {
            bool canEdit = terminal->IsCursorInEditableArea();
            if (!canEdit)
            {
                NppSSH_LogInfoAuto("【拦截】非可编辑区域，禁止删除操作");
                res = 0;
                s_bProcessingMsg = false;
                return res;
            }

            DWORD selStart = 0, selEnd = 0;
            ::SendMessageW(hWnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            DWORD cursorPos = selStart;

            // 获取最后一行全局起始位置
            LONG lastLineGlobalStart = GetLastLineGlobalStart(hWnd);

            std::string promptStr = terminal->GetPrompt();
            std::wstring wPrompt = UTF8ToWstring(promptStr);
            int promptLen = static_cast<int>(wPrompt.length());

            // 本行光标偏移（变量名与字符输入保持一致）
            int localCursorInLine = static_cast<int>(cursorPos) - static_cast<int>(lastLineGlobalStart);
            int cursorInCmdPos = localCursorInLine - promptLen;
            if (cursorInCmdPos < 0)
                cursorInCmdPos = 0;

            // 退格特殊保护逻辑
            if (wParam == VK_BACK)
            {
                if (localCursorInLine == promptLen)
                {
                    NppSSH_LogInfoAuto("【拦截】退格：光标紧贴提示符末尾，禁止回删prompt");
                    res = 0;
                    s_bProcessingMsg = false;
                    return res;
                }
            }

            std::string currentCmd = terminal->GetCmd();
            NppSSH_LogInfoAuto("【删除调试】currentCmd=" + currentCmd);
            NppSSH_LogInfoAuto("【删除调试】localCursorInLine=" + IntToStr(localCursorInLine) + ",cursorInCmdPos=" + IntToStr(cursorInCmdPos));

            bool isCmdModified = false;
            if (wParam == VK_BACK)
            {
                // 退格：删除光标前一个字符
                if (cursorInCmdPos > 0 && cursorInCmdPos <= static_cast<int>(currentCmd.length()))
                {
                    currentCmd.erase(cursorInCmdPos - 1, 1);
                    isCmdModified = true;
                    NppSSH_LogInfoAuto("【同步cmd】退格删除字符");
                }
            }
            else if (wParam == VK_DELETE)
            {
                // DEL键：删除光标当前位置字符
                if (cursorInCmdPos < static_cast<int>(currentCmd.length()))
                {
                    currentCmd.erase(cursorInCmdPos, 1);
                    isCmdModified = true;
                    NppSSH_LogInfoAuto("【同步cmd】DEL删除字符");
                }
            }

            if (isCmdModified)
            {
                terminal->SetCmd(currentCmd.c_str());
                NppSSH_LogInfoAuto("【同步cmd】删除后命令=" + currentCmd);
            }

            // 交给原生RichEdit执行删除
            res = CallWindowProc(oldProc, hWnd, msg, wParam, lParam);
            NppSSH_LogInfoAuto("【放行】删除按键操作");
            s_bProcessingMsg = false;
            return res;
        }
        
        // HOME / END 键合并处理
        bool isHomeOrEndKey = (msg == WM_KEYDOWN && (wParam == VK_HOME || wParam == VK_END));
        if (isHomeOrEndKey)
        {
            std::string promptStr = terminal->GetPrompt();
            std::string currentCmd = terminal->GetCmd();

            // 判断prompt和cmd都不为空，执行自定义光标跳转逻辑
            if (!promptStr.empty())
            {
                std::wstring wPrompt = UTF8ToWstring(promptStr);
                int promptLen = static_cast<int>(wPrompt.length());
                std::wstring wCmd = UTF8ToWstring(currentCmd);
                int cmdLen = static_cast<int>(wCmd.length());

                // 复用工具函数获取最后一行全局起始位置，变量命名保持统一
                LONG lastLineGlobalStart = GetLastLineGlobalStart(hWnd);
                DWORD targetGlobalCursorPos = 0;

                if (wParam == VK_HOME)
                {
                    // HOME：跳转到提示符末尾（命令输入区开头）
                    targetGlobalCursorPos = static_cast<DWORD>(lastLineGlobalStart + promptLen);
                    NppSSH_LogInfoAuto("【HOME键】光标跳转至提示符末尾");
                }
                else if (wParam == VK_END)
                {
                    // END：跳转到prompt + cmd整体末尾，也就是命令结尾
                    targetGlobalCursorPos = static_cast<DWORD>(lastLineGlobalStart + promptLen + cmdLen);
                    NppSSH_LogInfoAuto("【END键】光标跳转至命令末尾");
                }

                // 设置光标，无选中
                ::SendMessageW(hWnd, EM_SETSEL, targetGlobalCursorPos, targetGlobalCursorPos);
                res = 0;
                s_bProcessingMsg = false;
                return res;
            }
            // 不满足条件：拦截原生HOME/END，不放行RichEdit默认行为
            NppSSH_LogInfoAuto("【HOME/END键】条件不满足，拦截原生按键");
            res = 0;
            s_bProcessingMsg = false;
            return res;
        }

        // 9. 字符输入
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

                LONG lastLineGlobalStart = GetLastLineGlobalStart(terminal->Get_TerminalHandle());

                std::string promptStr = terminal->GetPrompt();
                std::wstring wPrompt = UTF8ToWstring(promptStr);
                int promptLen = (int)wPrompt.length();
                std::wstring wLastLine = terminal->GetStoreContent();
                wLastLine = CleanrrW(wLastLine);
                int lastLineTotalLen = static_cast<int>(wLastLine.length());
                int localCursorInLine = static_cast<int>(cursorPos) - static_cast<int>(lastLineGlobalStart);
                int cursorInCmdPos = localCursorInLine - promptLen;
                if (cursorInCmdPos < 0)
                    cursorInCmdPos = 0;

                std::string currentCmd = terminal->GetCmd();
                char c = static_cast<char>(wParam);

                if (cursorInCmdPos < static_cast<int>(currentCmd.length())) {
                    currentCmd.insert(cursorInCmdPos, 1, c);
                    NppSSH_LogInfoAuto("【同步cmd】插入字符：" + std::string(1, c));
                }
                else
                {
                    // 场景2：光标在命令末尾之后，直接追加字符
                    currentCmd += c;
                    NppSSH_LogInfoAuto("【同步cmd】末尾追加字符：" + std::string(1, c));
                }
                terminal->SetCmd(currentCmd.c_str());
                terminal->StoreTerminalContent((wchar_t)wParam);// 更新最后一行存储

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
// 扩展断行回调（Msftedit 高版本 RichEdit 专用，优先级更高）
LRESULT CALLBACK ForceBreakWordBreakProcEx(
    LPTSTR lpch,
    int ichCurrent,
    int cch,
    int code,
    DWORD dwFlags,
    UINT codepage
)
{
    // WB_ISDELIMITER：所有字符均视为分隔符 → 任意位置可换行
    if (code == WB_ISDELIMITER)
    {
        return TRUE;
    }
    // WB_CLASSIFY：当前字符后允许换行
    if (code == WB_CLASSIFY)
    {
        return WBF_BREAKAFTER;
    }
    return 0;
}

// 原有传统断行回调（保留，兼容低版本）
LRESULT CALLBACK ForceBreakWordBreakProc(
    LPARAM /*lParam*/,
    int code,
    WPARAM wParam,
    LPARAM /*lParam2*/
)
{
    if (code == WB_ISDELIMITER)
    {
        return TRUE;
    }
    if (code == WB_CLASSIFY)
    {
        return WBF_BREAKAFTER;
    }
    return 0;
}
SSHTermHandle::SSHTermHandle() {
    // 初始化成员变量（避免野指针）

    _prompt = "";               // 命令提示符（迁移自Prompt）
    _isCommandRunning = false; // 标记后台命令是否正在执行
    _hwndParent = nullptr;
    _hTerminal = nullptr;
    _cmd = ""; // 或根据实际类型初始化，比如空字符串
    _oldEditProc = nullptr;
}
// 析构函数：释放资源，防止内存泄漏
SSHTermHandle::~SSHTermHandle() {
    if (_hTerminal && _oldEditProc) {
        // 清理窗口属性（新增）
        RemoveProp(_hTerminal, _titleBuf);
        // 恢复原窗口过程（保留）
        SetWindowLongPtr(_hTerminal, GWLP_WNDPROC, (LONG_PTR)_oldEditProc);
        _oldEditProc = nullptr;
    }
}

HWND SSHTermHandle::Get_TerminalHandle() const {
    return _hTerminal;
}
HWND SSHTermHandle::InitTerminalEditBox(HWND hParent) {

    if (!::IsWindow(hParent)) {
        ::MessageBoxW(g_nppData._nppHandle, L"SSH_InitTerminalEditBox: 面板窗口句柄无效！", L"NppSSH调试提示", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    // 1. 加载库
    // 放弃 MsftEdit.dll，直接使用系统自带的标准 RichEdit 控件
    //_hRichEditLib = LoadLibraryW(L"Riched20.dll");
    _hRichEditLib = LoadLibraryW(L"Msftedit.dll");;
    if (!_hRichEditLib)
    {
        MessageBoxW(g_nppData._nppHandle, L"无法加载系统 RichEdit 库（Riched20.dll）！", L"NppSSH调试提示", MB_OK | MB_ICONERROR);
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
    //const int TOP = iconSize + 12;
    const int TOP = 24 + 12;
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
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN | ES_NOHIDESEL,
        x, y, cx, cy,
        _hwndParent,
        (HMENU)IDC_OUTPUT_EDIT,
        g_hInst,
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
        MessageBoxW(g_nppData._nppHandle, errMsg, L"NppSSH调试提示", MB_OK | MB_ICONERROR);
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

    // 隐藏水平滚动条（双重保险）
    ShowScrollBar(_hTerminal, SB_HORZ, FALSE);
    // 强制隐藏水平滚动条，禁止横向延伸
    SetWindowLongPtrW(_hTerminal, GWL_STYLE,
        GetWindowLongPtrW(_hTerminal, GWL_STYLE) & ~WS_HSCROLL);
    // 2. 绑定【扩展断行回调 EM_SETWORDBREAKPROCEX】(Msftedit 首选接口)
    // 高版本 RichEdit 优先使用该接口，传统 EM_SETWORDBREAKPROC 作为兼容兜底
    //SendMessageW(_hTerminal, EM_SETWORDBREAKPROCEX, 0, (LPARAM)ForceBreakWordBreakProcEx);
    // 传统断行回调 兼容旧逻辑
    //SendMessageW(_hTerminal, EM_SETWORDBREAKPROC, 0, (LPARAM)ForceBreakWordBreakProc);

    // 3. 绑定换行宽度为控件自身宽度（永久生效，布局刷新也保留）
    HDC hdc = GetDC(_hTerminal);
    SendMessageW(_hTerminal, EM_SETTARGETDEVICE, (WPARAM)hdc, (LPARAM)0);
    ReleaseDC(_hTerminal, hdc);

    // 创建控件后，设置字符集
    CHARFORMAT2W cf = { 0 };
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_CHARSET;
    //cf.bCharSet = CP_UTF8; // 强制 UTF-8 字符集
    cf.bCharSet = DEFAULT_CHARSET;
    //cf.bCharSet = GB2312_CHARSET;
    SendMessageW(_hTerminal, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    int fontSize = 28;
    // 设置默认字体（等宽字体，适配终端）
    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Courier New");// 备选："Courier New"、"Lucida Console"、"Microsoft YaHei"
    SendMessageW(_hTerminal, WM_SETFONT, (WPARAM)hFont, TRUE);
    // 关闭横向自动滚动：超出宽度下沉换行，禁用水平滚动条
    //SendMessageW(_hTerminal, EM_SETTARGETDEVICE, 0, 0);

    PARAFORMAT2 pf{};
    pf.cbSize = sizeof(PARAFORMAT2);
    // 启用：对齐+行间距，其余全部关闭
    pf.dwMask = PFM_ALIGNMENT | PFM_LINESPACING | PFM_SPACEBEFORE | PFM_SPACEAFTER | PFM_OFFSET;
    pf.wAlignment = PFA_LEFT;
    pf.dySpaceBefore = 0; // 段前段后0空白，杜绝自动空行
    pf.dySpaceAfter = 0;
    pf.bLineSpacingRule = 0;// 固定紧凑行高，适配Courier New等宽终端字体
    pf.dyLineSpacing = MulDiv(fontSize, 1440, 96);// 换算字体高度（字体30pt，行高匹配）
    pf.dxStartIndent = 0;// 缩进全部关闭
    pf.dxRightIndent = 0;
    pf.dxOffset = 0;

    SendMessageW(_hTerminal, EM_SETPARAFORMAT, 0, (LPARAM)&pf);

    // 按位或 追加需要的事件（不覆盖原有）
    // 1、事件掩码 EVENTMASK：只加ENM_系列，管控WM_NOTIFY通知
    DWORD dwMask = SendMessage(_hTerminal, EM_GETEVENTMASK, 0, 0);
    dwMask |= ENM_MOUSEEVENTS;   // 保留右键、鼠标事件（原有需求：内置右键菜单）
    dwMask |= ENM_SELCHANGE;     // 选区变化通知（用来自动修正末尾换行）
    dwMask |= ENM_SCROLL;
    dwMask |= ENM_PROTECTED;
    // 3.写回掩码
    SendMessage(_hTerminal, EM_SETEVENTMASK, 0, dwMask);

    //2、 编辑样式 EDITSTYLE：只加SES_系列，管控输入/IME/换行 
    DWORD dwEditStyle = SendMessage(_hTerminal, EM_GETEDITSTYLE, 0, 0);
    dwEditStyle |= SES_USECTF;        //启用TSF微软拼音(可切换中英文)
    dwEditStyle |= SES_NOIME;
    dwEditStyle |= SES_XLTCRCRLFTOCR; //用户回车\r\n自动转\n
    dwEditStyle |= SES_NOEALINEHEIGHTADJUST; //防止 Courier New + 中文 行高抖动
    //dwEditStyle |= SES_DRAFTMODE; ; //草稿模式 简化格式
    dwEditStyle &= ~SES_USECRLF;  // 禁用旧CRLF模式（废弃标记，配套关闭）
    dwEditStyle &= ~(SES_UPPERCASE | SES_LOWERCASE | SES_BIDI);//关闭自动大小写、双向文字
    //dwEditStyle &= ~SES_WORDWRAP;  // 禁用原生整词换行，配合自定义断行回调
    SendMessage(_hTerminal, EM_SETEDITSTYLE, dwEditStyle, 0);

    // 🔑 关键：设置排版选项（强制简单换行）
    //BOOL r = SendMessageW(_hTerminal, EM_SETTYPOGRAPHYOPTIONS, TO_DISABLECUSTOMTEXTOUT, TO_DISABLECUSTOMTEXTOUT);
    //NppSSH_LogInfoAuto("【EM_SETTYPOGRAPHYOPTIONS 】result=" + IntToStr(r ? 1 : 0));
    // 确保文本模式
    SendMessageW(_hTerminal, EM_SETTEXTMODE, TM_RICHTEXT, 0);

    // 设置文本边距为 0
    RECT zeroMargin = { 0, 0, 0, 0 };
    SendMessageW(_hTerminal, EM_SETRECTNP, 0, (LPARAM)&zeroMargin);




    SetPTYType("xterm-256color");// 初始化默认PTY类型（可从配置/用户选择动态修改）
    // 调整输出伪终端位置，避开顶部按钮栏
    SizeSSHTermHandle(hParent);
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
        swprintf_s(_titleBuf, _countof(_titleBuf), L"SSHTermHandle-%p", _hTerminal);
        SetProp(_hTerminal, _titleBuf, (HANDLE)this);
        // 4. 设置新的窗口过程
        SetWindowLongPtr(_hTerminal, GWLP_WNDPROC, (LONG_PTR)TerminalEditProc);
        NppSSH_LogInfoAuto("伪终端子类化完成！hWnd=" + PtrToHexStr(_hTerminal)
            + " 原过程：" + PtrToHexStr(_oldEditProc)
            + " 新过程：" + PtrToHexStr(TerminalEditProc));
    }
    _initialized = true;
    //MessageBoxW(s_nppData._nppHandle, L"终端伪终端初始化完成 ✅", L"成功", MB_OK);
    return _hTerminal;
}

// 设置当前PTY类型（自动加载对应特性）
void SSHTermHandle::SetPTYType(const std::string& ptyType) {
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
const PTYFeatures& SSHTermHandle::GetPTYFeatures() const {
    return _currentPTYFeatures;
}

/*
*断开连接，改变终端内容
*/
void SSHTermHandle::disConnection() {//暂未使用
    if (_hTerminal) {
        //DisconnectPanel(this -> _panelId);// 面板断开SSH函数
        ::SetWindowTextW(_hTerminal, L"✅ SSH已断开\n等待新的连接...");
    }
}
/*
* 重置终端内容
*/
void SSHTermHandle::resetSSHTermHandle() {//暂未使用
    if (_hTerminal && ::IsWindow(_hTerminal)) {
        ::SetWindowTextW(_hTerminal, L"🔌 SSH已断开\n等待新的连接...resetPanelToInit");
    }
}
/*
* 设置终端面板大小
*/
void SSHTermHandle::SizeSSHTermHandle(HWND hParent) {//hParent=面板的_hSelf
    if (!_hTerminal || !::IsWindow(_hTerminal))
        return;

    if (!::IsWindow(hParent))
        return;
    //::MessageBoxW(s_nppData._nppHandle, L"SizeSSHTermHandle", L"NppSSH提示", MB_OK | MB_ICONINFORMATION);

    //_hTerminal = ::GetDlgItem(hParent, IDC_OUTPUT_EDIT);
    RECT rc;
    if (!::GetClientRect(hParent, &rc))
        return;
    // 左边距
    const int LEFT = 5;
    // 上边距（避开按钮栏）
    //const int TOP = iconSize + 12;
    const int TOP = 24 + 12;
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
    SendMessageW(_hTerminal, EM_REQUESTRESIZE, 0, 0);

    // 窗口缩放后，强制刷新换行宽度
    //HDC hdc = GetDC(_hTerminal);
    //SendMessageW(_hTerminal, EM_SETTARGETDEVICE, (WPARAM)hdc, (LPARAM)0);
    //ReleaseDC(_hTerminal, hdc);
    //// 重绑定断行回调（防止布局重置）
    //SendMessageW(_hTerminal, EM_SETWORDBREAKPROCEX, 0, (LPARAM)ForceBreakWordBreakProcEx);

    //只重绘【伪终端】自己让伪终端立刻刷新、重新绘制自己的内容、文字、背景、边框。
    ::RedrawWindow(_hTerminal, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);// 刷新伪终端内容（防止文字不显示）
}
/*
* 追加伪终端终端模拟内容
*/
void SSHTermHandle::AppendOutputText(const std::string& text) {
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
    NppSSH_LogInfoAuto("【原始字符内容=======================================】");
    DeBugOutPutText(cleanText);
    //cleanText = Cleanrr(cleanText);//清除所有\n前面的\r

    // 1、转宽字符
    std::wstring rawW = UTF8ToWstring(cleanText);
    // 2、统一换行预处理（删除全部\r、合并连续\n）
    std::wstring normW = NormalizeTerminalLineFeed(rawW);

    // 3、调用自动换行函数（封装后的核心逻辑）
    //std::wstring wrapText = AutoWrapText(normW);
    ////////////////////////////wrapText


    std::wstring* wtext = new std::wstring(normW);
    //NppSSH_LogInfoAuto("【原始宽字符内容】" + WStringToLogStr(*wtext));
    NppSSH_LogInfoAuto("【原始宽字符内容=======================================】");
    //DeBugOutPutText(wtext->c_str());
    NppSSH_LogInfoAuto("【原始宽字符内容,删除全部\r、合并连续\n处理后=======================================】");
    DeBugOutPutText(wtext->c_str());

    // ✅ 投递到主线程（绝对安全）

    PostMessage(_hTerminal, WM_APPEND_OUTPUT_TEXT, 0, (LPARAM)wtext);
}


/*
* 控制键盘的输入操作
* 1、是否光标不在最后一行，禁止编辑
* 2、是否提示符为空，禁止编辑
* 3、最后一行是否不以提示符开头
* 4、光标是否在提示符后
*/
bool SSHTermHandle::IsCursorInEditableArea() const {

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

    LONG lastLineGlobalStart = GetLastLineGlobalStart(_hTerminal);
    
    bool isCursorAtLastLine = (cursorPos >= lastLineGlobalStart);
    if (!isCursorAtLastLine)
    {
        NppSSH_LogInfoAuto("[可编辑判定] 光标不在最后一行，禁止编辑");
        return false;
    }
    // 2. 获取 store 内容（无 \r）
    //std::wstring allText = GetStoreContent();
    //allText = CleanrrW(allText);// 双重保险，\n前面紧挨的所有 \r
    std::wstring lastLine = GetStoreContent();
    lastLine = CleanrrW(lastLine);
    //DWORD storedCursorPos = cursorPos;

    //if (cursorPos > 0) {
    //    // 获取 RichEdit 中 [0, cursorPos) 的文本
    //    std::wstring prefix(cursorPos + 1, 0);
    //    TEXTRANGE tr{ {0, (LONG)cursorPos}, &prefix[0] };
    //    SendMessageW(_hTerminal, EM_GETTEXTRANGE, 0, (LPARAM)&tr);

    //    // 计算 \r 的数量（RichEdit 独有）
    //    int crCount = std::count(prefix.begin(), prefix.end(), L'\r');
    //    storedCursorPos = cursorPos - crCount;

    //    // 防御性修正
    //    if (storedCursorPos > allText.size())
    //        storedCursorPos = static_cast<DWORD>(allText.size());
    //}
    //NppSSH_LogInfoAuto("【调试11111111111111111】");
    //DeBugOutPutText(allText);
    //// 3. 找光标所在行
    //size_t lastLineStart = 0;
    //// 从后往前找，跳过所有空白、换行、不可见字符
    //for (size_t i = cursorPos; i > 0; --i) {
    //    if (allText[i - 1] == L'\n' || allText[i - 1] == L'\r') {
    //        lastLineStart = i; break;
    //    }
    //}
    //NppSSH_LogInfoAuto("【调试2222222222222222】");
    //NppSSH_LogInfoAuto("【调试】RichEdit光标=" + IntToStr(cursorPos)
    //    + " 映射后store光标=" + IntToStr(storedCursorPos)
    //    + " store大小=" + IntToStr(allText.size())
    //    + " lastLineStart=" + IntToStr(lastLineStart));

    //DeBugOutPutText(allText);
    //std::string loginfoDebug = "【可编辑判定调试】 "
    //    "_prompt=[" + _prompt + "] "
    //    "lastLineStart=[" + IntToStr(lastLineStart) + "] "
    //    "全局光标=[" + IntToStr((int)cursorPos) + "] ";

    //NppSSH_LogInfoAuto(loginfoDebug);
    //// 光标不在最后一行 → 不可编辑
    //if (storedCursorPos < lastLineStart) {
    //    NppSSH_LogInfoAuto("[可编辑判定] 光标不在最后一行，禁止编辑");
    //    return false;
    //}
    //// 只取最后一行内容，在本行内计算！
    //std::wstring lastLine = allText.substr(lastLineStart);

    // 6. 处理提示符（清理乱码）
    //std::wstring promptW = CleanAnsiEscapeSequences(UTF8ToWstring(_prompt));
    std::wstring promptW = (UTF8ToWstring(_prompt));
    int promptLen = static_cast<int>(promptW.size());

    if (promptLen <= 0) {
        NppSSH_LogInfoAuto("[可编辑判定] 清理后提示符为空，禁止编辑");
        return false;
    }


    // 6. 最后一行必须以提示符开头
    if (lastLine.size() < promptLen ||
        lastLine.substr(0, promptLen) != promptW) {
        NppSSH_LogInfoAuto("promptW=" + WStringToLogStr(promptW) + ",提示符长度=[" + IntToStr(promptLen) + "] ");

        NppSSH_LogInfoAuto("[可编辑判定] 最后一行不以提示符开头");
        return false;
    }
    //int lastLineSize = static_cast<int>(cursorPos - lastLineStart);
    //DWORD lineCursorOffset = cursorPos;
    DWORD lineCursorOffset = cursorPos - static_cast<DWORD>(lastLineGlobalStart);
    bool cursorIsAfterPrompt = (static_cast<int>(lineCursorOffset) >= promptLen);
    // 结合_cmd校验：如果已有命令，光标需在命令范围内（增强校验）
    bool cmdAreaValid = true;
    std::wstring cmdW = UTF8ToWstring(_cmd);
    if (!cmdW.empty()) {
        int cmdEndInLine = promptLen + static_cast<int>(cmdW.size());
        cmdAreaValid = (lineCursorOffset >= promptLen && promptLen <= cmdEndInLine);
    }

    bool canEdit = cursorIsAfterPrompt && cmdAreaValid;

    std::string loginfo = "【可编辑判定】 "
        "_prompt=[" + _prompt + "] "
        "lastLine=[" + WStringToLogStr(lastLine) + "] "
        "全局光标=[" + IntToStr((int)cursorPos) + "] "
        "提示符长度=[" + IntToStr(promptLen) + "] "
        "命令长度=[" + IntToStr(cmdW.size()) + "] "
        "光标在提示符后=[" + IntToStr(cursorIsAfterPrompt) + "] "
        "可编辑=[" + IntToStr(canEdit ? 1 : 0) + "]";

    NppSSH_LogInfoAuto(loginfo);

    return canEdit;
}
void SSHTermHandle::executeClear() {
    ::SetWindowTextW(_hTerminal, L"");
    this->SetStoreContent(L"");
}

void SSHTermHandle::SetCmd(const char* cmdStr) {
    if (cmdStr) {
        _cmd = cmdStr;
    }
    else {
        _cmd.clear();
    }
}

const char* SSHTermHandle::GetCmd() const {
    return _cmd.c_str();
}

void SSHTermHandle::SetPrompt(const std::string promptStr) {
    _prompt = promptStr;
    NppSSH_LogInfoAuto("【提示符更新】主动修复输入状态，新提示符：" + promptStr);
}

const std::string& SSHTermHandle::GetPrompt() const {
    return _prompt;
}

HWND SSHTermHandle_InitControlPanel(HWND hParent, int panelSeqId) {
    SSHTermHandle* _SSHTermHandle = new SSHTermHandle();
    NppSSH_LogInfoAuto("终端绑定的面板ID==" + std::to_string(panelSeqId));
    _SSHTermHandle->Set_panelSeqId(panelSeqId);
    return _SSHTermHandle->InitTerminalEditBox(hParent);
}


