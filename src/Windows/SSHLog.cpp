#include "SSHLog.h"

static std::queue<std::string> g_logQueue;
static CRITICAL_SECTION g_logCs; // 全局临界区，替代原来的局部静态cs
static HANDLE g_logEvent = NULL;
static HANDLE g_logThread = NULL;
static bool g_logActive = true;
// 初始化临界区（只执行一次）
void InitLogCs() {
    static bool inited = false;
    if (!inited) {
        InitializeCriticalSection(&g_logCs);
        inited = true;
    }
}

// 线程安全入队
void LogEnqueue(const std::string& s) {
    InitLogCs();
    EnterCriticalSection(&g_logCs);
    g_logQueue.push(s);
    LeaveCriticalSection(&g_logCs);
}

// 后台写入线程
DWORD WINAPI LogWriterThread(LPVOID) {
    InitLogCs();
    while (g_logActive) {
        WaitForSingleObject(g_logEvent, INFINITE);

        std::queue<std::string> localQueue;
        EnterCriticalSection(&g_logCs);
        while (!g_logQueue.empty()) {
            localQueue.push(g_logQueue.front());
            g_logQueue.pop();
        }
        LeaveCriticalSection(&g_logCs);

        std::wstring logPath = SSHLogs_GetPluginsConfigDir();
        logPath += L"\\" NPPSSH_LOG_FILE_NAME;

        HANDLE hFile = CreateFileW(
            logPath.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER sz;
            if (GetFileSizeEx(hFile, &sz) && sz.QuadPart == 0) {
                BYTE bom[] = { 0xEF, 0xBB, 0xBF };
                DWORD w;
                WriteFile(hFile, bom, 3, &w, NULL);
            }

            while (!localQueue.empty()) {
                std::string s = localQueue.front();
                localQueue.pop();

                DWORD w;
                WriteFile(hFile, s.c_str(), (DWORD)s.size(), &w, NULL);
            }

            CloseHandle(hFile);
        }
    }
    return 0;
}

// 日志初始化（插件启动调用）
void SSHLog_Init() {
    InitLogCs();
    g_logEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_logThread = CreateThread(NULL, 0, LogWriterThread, NULL, 0, NULL);
}

std::wstring SSHLogs_GetPluginsConfigDir() {
    TCHAR szConfigDir[MAX_PATH] = { 0 };
    SendMessage(g_nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)szConfigDir);

    if (_tcslen(szConfigDir) > 0 && PathIsDirectory(szConfigDir))
        return szConfigDir;

    TCHAR szNppPath[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, szNppPath, MAX_PATH);
    PathRemoveFileSpec(szNppPath);
    swprintf_s(szConfigDir, L"%s\\plugins\\config", szNppPath);
    CreateDirectory(szConfigDir, NULL);
    return szConfigDir;
}

std::string SSHLog_FormatTime() {
    std::time_t now = std::time(nullptr);
    std::tm localTime;
    localtime_s(&localTime, &now);
    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// 最终日志函数（无锁，仅入队）
void SSHLog_Write(LogLevel level, const std::string& event, const std::string& content) {
    std::string time = SSHLog_FormatTime();
    std::string lstr = "info";
    switch (level) {
    case LogLevel::LOG_ERROR: lstr = "error"; break;
    case LogLevel::LOG_DEBUG: lstr = "debug"; break;
    case LogLevel::LOG_WARN:  lstr = "warn"; break;
    }

    std::ostringstream ss;
    ss << time << " level: [" << lstr << "] ";
    ss << "event: [" << (event.empty() ? "unknown" : event) << "] ";
    ss << "msg ==> [ " << content << " ]\n";

    LogEnqueue(ss.str());
    SetEvent(g_logEvent);
}