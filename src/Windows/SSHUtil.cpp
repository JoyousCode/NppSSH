// SSHUtil.cpp工具处理，具体实现
#include "SSHUtil.h"

// 日志专用：安全地把 std::wstring 转为 std::string （日志专用）（避免乱码和异常）
std::string WStringToLogStr(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &out[0], len, nullptr, nullptr);
    return out;
}
// 日志专用：指针转十六进制字符串（日志专用）
std::string PtrToHexStr(void* ptr) {
    char buf[32] = { 0 };
    sprintf_s(buf, "0x%p", ptr);
    return std::string(buf);
}
// 日志专用：数字转字符串
std::string IntToStr(int num) {
    return std::to_string(num);
}

// 编码转换工具（自动识别UTF8，解决Windows乱码）
std::wstring UTF8ToWstring(const std::string& str) {
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
    return res;
}
std::wstring GBKToWstring(const std::string& str) {
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
    // ======================【完整字符日志打印，解析所有转义符号】======================
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

// 辅助函数：转16进制字符串，方便日志查看
std::string IntToHexStr(DWORD val) {
    char buf[32];
    sprintf_s(buf, "%08X", val);
    return std::string(buf);
}
std::string WStringToUTF8(const std::wstring& wstr)
{
    if (wstr.empty()) return "";
    try
    {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        if (size_needed <= 0)
            return "";
        std::string strTo(size_needed, 0);
        int ret = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        if (ret <= 0)
            return "";
        return strTo;
    }
    catch (...)
    {
        NppSSH_LogErrorAuto("【工具函数】宽字符转换普通字符失败");
        return "";
    }

}

std::wstring HwndToWString(HWND hWnd)
{
    wchar_t buf[64]{};
    swprintf(buf, L"0x%p", hWnd);
    return std::wstring(buf);
}

std::string HwndToString(HWND hWnd)
{
    wchar_t buf[64]{};
    swprintf(buf, L"0x%p", hWnd);
    return WStringToUTF8(std::wstring(buf));
}

void CenterWindow(HWND hWndChild, HWND hWndParent)
{
    if (!hWndChild || !hWndParent) return;

    RECT rcChild, rcParent;
    GetWindowRect(hWndChild, &rcChild);
    GetWindowRect(hWndParent, &rcParent);

    int cx = (rcParent.right - rcParent.left) - (rcChild.right - rcChild.left);
    int cy = (rcParent.bottom - rcParent.top) - (rcChild.bottom - rcChild.top);

    SetWindowPos(
        hWndChild,
        NULL,
        rcParent.left + cx / 2,
        rcParent.top + cy / 2,
        0, 0,
        SWP_NOSIZE | SWP_NOZORDER
    );
}
std::wstring charToWString(const char* szSrc, UINT codepage)
{
    if (szSrc == nullptr || *szSrc == '\0')
        return std::wstring();

    // 第一步：获取需要宽字符缓冲区大小
    int nWideLen = MultiByteToWideChar(codepage, 0, szSrc, -1, nullptr, 0);
    if (nWideLen <= 0)
        return std::wstring();

    std::wstring wResult(nWideLen, L'\0');
    // 第二步：执行转换
    MultiByteToWideChar(codepage, 0, szSrc, -1, &wResult[0], nWideLen);
    return wResult;
}

std::string CheckHwndParentChildRelation(HWND hRoot, HWND hTarget)
{
    if (hRoot == nullptr || hTarget == nullptr)
        return "无效句柄";
    if (hRoot == hTarget)
    {
        char buf[256]{};
        sprintf(buf, "0x%p 与 0x%p 是同一个窗口", hRoot, hTarget);
        return std::string(buf);
    }

    // 向上遍历父窗口，看 hTarget 是否是 hRoot 的祖先
    int upCount = 0;
    HWND hCur = GetParent(hRoot);
    while (hCur != nullptr)
    {
        upCount++;
        if (hCur == hTarget)
        {
            char buf[512]{};
            sprintf(buf, "0x%p 查找%d次父级找到句柄 0x%p", hRoot, upCount, hTarget);
            return std::string(buf);
        }
        hCur = GetParent(hCur);
    }

    // 向下递归遍历所有子窗口，统计层级
    auto FindChildRecursive = [&](auto&& self, HWND parent, int level) -> int
        {
            HWND child = GetWindow(parent, GW_CHILD);
            while (child != nullptr)
            {
                if (child == hTarget)
                    return level;
                int subLevel = self(self, child, level + 1);
                if (subLevel != -1)
                    return subLevel;
                child = GetWindow(child, GW_HWNDNEXT);
            }
            return -1;
        };
    int childLevel = FindChildRecursive(FindChildRecursive, hRoot, 1);
    if (childLevel != -1)
    {
        char buf[512]{};
        sprintf(buf, "0x%p 查找%d次子级找到句柄 0x%p", hRoot, childLevel, hTarget);
        return std::string(buf);
    }

    // 既不是父祖先，也不是子后代
    char buf[512]{};
    sprintf(buf, "0x%p 和 0x%p 无父子层级关系", hRoot, hTarget);
    return std::string(buf);
}



// BLOB转Base64
bool CryptBlobToBase64(DATA_BLOB* pBlob, std::wstring& outBase64)
{
    if (!pBlob || pBlob->cbData == 0) return false;
    DWORD dwBase64Len = 0;
    if (!CryptBinaryToStringW(pBlob->pbData, pBlob->cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &dwBase64Len))
        return false;
    std::wstring buf(dwBase64Len, 0);
    if (!CryptBinaryToStringW(pBlob->pbData, pBlob->cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &buf[0], &dwBase64Len))
        return false;
    outBase64 = buf;
    return true;
}
// Base64转回BLOB
bool Base64ToCryptBlob(const std::wstring& base64Str, DATA_BLOB* pBlob)
{
    if (base64Str.empty()) return false;
    DWORD dwBinLen = 0;
    if (!CryptStringToBinaryW(base64Str.c_str(), (DWORD)base64Str.size(), CRYPT_STRING_BASE64, nullptr, &dwBinLen, nullptr, nullptr))
        return false;
    BYTE* pBin = new BYTE[dwBinLen];
    if (!CryptStringToBinaryW(base64Str.c_str(), (DWORD)base64Str.size(), CRYPT_STRING_BASE64, pBin, &dwBinLen, nullptr, nullptr))
    {
        delete[] pBin;
        return false;
    }
    pBlob->pbData = pBin;
    pBlob->cbData = dwBinLen;
    return true;
}
// 加密密码输出base64
bool SSH_EncryptPasswordToBase64(const std::wstring& plainPwd, std::wstring& outBase64)
{
    outBase64.clear();
    if (plainPwd.empty()) return true;
    DATA_BLOB inBlob{}, outBlob{};
    std::string u8pwd = WStringToUTF8(plainPwd);
    inBlob.pbData = (BYTE*)u8pwd.data();
    inBlob.cbData = (DWORD)u8pwd.size();
    if (!CryptProtectData(&inBlob, L"NppSSH_Pwd", nullptr, nullptr, nullptr, 0, &outBlob))
        return false;
    bool ok = CryptBlobToBase64(&outBlob, outBase64);
    LocalFree(outBlob.pbData);
    return ok;
}
// base64解密得到明文
bool SSH_DecryptPasswordFromBase64(const std::wstring& base64Str, std::wstring& outPlainPwd)
{
    outPlainPwd.clear();
    if (base64Str.empty()) return true;
    DATA_BLOB inBlob{}, outBlob{};
    if (!Base64ToCryptBlob(base64Str, &inBlob))
        return false;
    BOOL bRet = CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr, 0, &outBlob);
    delete[] inBlob.pbData;
    if (!bRet) return false;
    std::string u8((char*)outBlob.pbData, outBlob.cbData);
    outPlainPwd = UTF8ToWstring(u8);
    LocalFree(outBlob.pbData);
    return true;
}

// 判断片段是否全部为数字
static bool IsAllDigit(const std::wstring& s)
{
    if (s.empty())
        return false;
    for (wchar_t ch : s)
    {
        if (!iswdigit(static_cast<wint_t>(ch)))
            return false;
    }
    return true;
}

/// <summary>
/// 严格IPv4校验
/// 返回值：
/// true：标准合法IPv4
/// false：不是IPv4
/// out allNumberSeg：输出四段是否全部是数字
/// </summary>
bool IsValidIPv4(const std::wstring& s, bool& allNumberSeg)
{
    allNumberSeg = false;
    size_t dotCnt = std::count(s.begin(), s.end(), L'.');
    if (dotCnt != 3)
        return false;

    std::vector<std::wstring> segs;
    std::wstring seg;
    for (wchar_t ch : s)
    {
        if (ch == L'.')
        {
            segs.push_back(seg);
            seg.clear();
        }
        else
        {
            seg += ch;
        }
    }
    segs.push_back(seg);

    // 必须4段
    if (segs.size() != 4)
        return false;

    bool allDigit = true;
    for (const auto& item : segs)
    {
        if (!IsAllDigit(item))
        {
            allDigit = false;
            break;
        }
    }
    allNumberSeg = allDigit;

    if (!allDigit)
        return false;

    // 全部数字，校验每段0‑255
    for (const auto& item : segs)
    {
        unsigned long val = std::wcstoul(item.c_str(), nullptr, 10);
        if (val > 255)
            return false;
    }
    return true;
}

bool IsValidIPv6(const std::wstring& s)
{
    if (s.find(L':') == std::wstring::npos)
        return false;

    for (wchar_t ch : s)
    {
        if (!((ch >= L'0' && ch <= L'9') ||
            (ch >= L'a' && ch <= L'f') ||
            (ch >= L'A' && ch <= L'F') ||
            ch == L':' || ch == L'.'))
        {
            return false;
        }
    }
    return true;
}

// 松散主机名校验：仅拦截连续点
bool IsHostNameLoose(const std::wstring& s)
{
    if (s.find(L"..") != std::wstring::npos)
        return false;
    return true;
}

bool IsRealPuttyGuiExe(const std::wstring& exePath)
{
    DWORD dwVerInfoSize = ::GetFileVersionInfoSizeW(exePath.c_str(), nullptr);
    if (dwVerInfoSize == 0)
    {
        NppSSH_LogErrorAuto("IsRealPuttyGuiExe：GetFileVersionInfoSize 返回0，无版本资源，直接放行");
        return true;
    }

    std::vector<BYTE> verBuf(dwVerInfoSize);
    if (!::GetFileVersionInfoW(exePath.c_str(), 0, dwVerInfoSize, verBuf.data()))
    {
        NppSSH_LogErrorAuto("IsRealPuttyGuiExe：GetFileVersionInfoW调用失败，直接放行");
        return true;
    }

    LPVOID pLangBlock = nullptr;
    UINT langBlockLen = 0;
    if (!VerQueryValueW(verBuf.data(), L"\\VarFileInfo\\Translation", &pLangBlock, &langBlockLen))
    {
        NppSSH_LogErrorAuto("IsRealPuttyGuiExe：VerQueryValueW 获取Translation语言列表失败，直接放行");
        return true;
    }

    std::wstring originalName;
    const WORD* pLangList = reinterpret_cast<const WORD*>(pLangBlock);
    for (UINT i = 0; i < langBlockLen / sizeof(WORD); i += 2)
    {
        WORD wLang = pLangList[i];
        WORD wCodePage = pLangList[i + 1];
        wchar_t szQueryPath[256] = { 0 };
        swprintf_s(szQueryPath, L"\\StringFileInfo\\%04X%04X\\OriginalFilename", wLang, wCodePage);

        LPVOID pStrVal = nullptr;
        UINT strLen = 0;
        if (VerQueryValueW(verBuf.data(), szQueryPath, &pStrVal, &strLen))
        {
            originalName = reinterpret_cast<LPCWSTR>(pStrVal);
            // 全部拼接为一个std::string，单参数传入日志
            std::string logMsg = "IsRealPuttyGuiExe：语言块:";
            logMsg += WStringToLogStr(std::wstring(szQueryPath));
            logMsg += " 读取OriginalFilename=";
            logMsg += WStringToLogStr(originalName);
            NppSSH_LogInfoAuto(logMsg);
            break;
        }
        {
            std::string logMsg = "IsRealPuttyGuiExe：语言块:";
            logMsg += WStringToLogStr(std::wstring(szQueryPath));
            logMsg += " 读取OriginalFilename失败";
            NppSSH_LogErrorAuto(logMsg);
        }
    }

    if (originalName.empty())
    {
        NppSSH_LogErrorAuto("IsRealPuttyGuiExe：所有语言块均读取OriginalFilename失败，直接放行");
        return true;
    }

    // 转为小写副本，用于子串判断，忽略大小写
    std::wstring lowerName = originalName;
    for (auto& ch : lowerName)
    {
        ch = towlower(ch);
    }

    // puttygen 密钥生成工具，直接拒绝
    if (lowerName.find(L"puttygen") != std::wstring::npos)
    {
        NppSSH_LogErrorAuto("IsRealPuttyGuiExe：检测为puttygen，拒绝选择");
        return false;
    }
    // puttytel 终端工具，直接拒绝
    if (lowerName.find(L"puttytel") != std::wstring::npos)
    {
        NppSSH_LogErrorAuto("IsRealPuttyGuiExe：检测为puttytel，拒绝选择");
        return false;
    }

    // 读到字段，包含putty子串直接放行
    if (lowerName.find(L"putty") != std::wstring::npos)
    {
        return true;
    }

    // 读到字段，但是不包含putty，拦截
    NppSSH_LogErrorAuto("IsRealPuttyGuiExe：OriginalFilename不含putty，判定为其他第三方exe，拒绝");
    return false;
}

// 输入法中英文切换（真正安全、无循环）
// bForceEnglish: true=强制英文(修复时用) | false=手动切换(Shift用)
void imm_chineseType(HWND hEdit)
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


// 清理ANSI转义序列（解决乱码核心）
std::wstring CleanAnsiEscapeSequences(const std::wstring& input) {
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

std::string CleanAnsiEscapeSequences(const std::string& input) {
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