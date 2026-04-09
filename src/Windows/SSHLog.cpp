// SSHLog.cpp（输出调试和SSH连接日志具体实现）
#include "SSHLog.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

// 获取NPP插件配置目录（通过NPP原生消息）
std::wstring SSHLogs_GetPluginsConfigDir() {
    std::wstring configDir;
    TCHAR szConfigDir[MAX_PATH] = { 0 };

    // 调用NPP原生消息获取插件配置目录
    SendMessage(g_nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)szConfigDir);

    // 验证路径有效性
    if (_tcslen(szConfigDir) > 0 && PathIsDirectory(szConfigDir)) {
        configDir = szConfigDir;
    }
    else {
        // 降级方案：使用NPP安装目录下的plugins/config
        TCHAR szNppPath[MAX_PATH] = { 0 };
        GetModuleFileName(NULL, szNppPath, MAX_PATH);
        PathRemoveFileSpec(szNppPath);
        _stprintf_s(szConfigDir, MAX_PATH, _T("%s\\plugins\\config"), szNppPath);
        configDir = szConfigDir;

        // 确保目录存在
        if (!PathIsDirectory(szConfigDir)) {
            CreateDirectory(szConfigDir, NULL);
        }
    }

    return configDir;
}

// 格式化当前时间（精确到秒）：YYYY-MM-DD HH:MM:SS
std::string SSHLog_FormatTime() {
    std::time_t now = std::time(nullptr);
    std::tm localTime;
    localtime_s(&localTime, &now); // 线程安全版本
    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// 核心日志写入实现（终极版：无行首空格 + 无弃用警告 + 无空行）
void SSHLog_Write(LogLevel level, const std::string& event, const std::string& content) {
    // 格式化时间（精确到秒）
    std::string timeStr = SSHLog_FormatTime();

    // 日志级别
    std::string levelStr;
    if (level == LogLevel::LOG_INFO) {
        levelStr = "info";
    }
    else if (level == LogLevel::LOG_ERROR) {
        levelStr = "error";
    }
    else if (level == LogLevel::LOG_DEBUG) {
        levelStr = "debug";
    }
    else {
        levelStr = "warn";
    }

    // 严格控制空格，绝对无首尾空格
    std::ostringstream logStream;
    logStream << timeStr << " level: [" << levelStr << "] ";
    if (!event.empty()) {
        logStream << "event: [" << event << "] ";
    }
    else {
        logStream << "event: [unknown] ";
    }
    logStream << "msg ==> [ " << content << " ]\n"; // 仅末尾加\n，无任何多余空格

    std::string logStr = logStream.str();

    // 用Windows API直接写UTF-8文件，无编码转换陷阱
    std::wstring logPath = SSHLogs_GetPluginsConfigDir();
    logPath += L"\\" + std::wstring(NPPSSH_LOG_FILE_NAME);

    // 打开文件：用CreateFileW直接操作，完全绕开wofstream的编码问题
    HANDLE hFile = CreateFileW(
        logPath.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        // 首次写入时添加UTF-8 BOM，确保记事本正确识别编码
        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(hFile, &fileSize) && fileSize.QuadPart == 0) {
            const BYTE utf8Bom[] = { 0xEF, 0xBB, 0xBF };
            DWORD written;
            WriteFile(hFile, utf8Bom, 3, &written, NULL);
        }

        // 直接写入UTF-8窄字符日志，无任何宽字符转换，彻底消除空格
        DWORD bytesWritten;
        WriteFile(hFile, logStr.c_str(), (DWORD)logStr.size(), &bytesWritten, NULL);
        CloseHandle(hFile);
    }
    else {
        MessageBoxW(NULL, L"日志文件写入失败！", L"NppSSH 日志错误", MB_OK | MB_ICONERROR);
    }
}