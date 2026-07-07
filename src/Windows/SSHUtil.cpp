// SSHUtil.cpp工具处理，具体实现
#include "SSHUtil.h"
static NppData s_nppData;
static HINSTANCE s_hInst;

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
    NppSSH_LogInfoAuto("自动识别UTF8，解决Windows乱码,转换成功");
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