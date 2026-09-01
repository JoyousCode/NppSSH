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