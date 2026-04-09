// SSHLog.h（调试 + 连接日志输出逻辑）
#pragma once
#include <libssh2.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tchar.h>

#include <locale>
#include <string>
#include <fstream> 
#include <codecvt>

#include "SSHWindow.h"
#include "DockingFeature/DockingDlgInterface.h"

//#include <shlwapi.h>
//#include <algorithm>
//#include <windowsx.h>
//#pragma comment(lib, "shlwapi.lib")
// 日志文件名常量
#define NPPSSH_LOG_FILE_NAME L"NppSSH.log"

// 日志级别枚举
enum class LogLevel {
    LOG_INFO,
    LOG_ERROR,
    LOG_DEBUG,
    LOG_WARN
};


// 获取NPP插件配置目录（动态适配用户/默认路径）
std::wstring SSHLogs_GetPluginsConfigDir();

// 核心日志输出函数（内部调用，处理时间拼接、文件写入）
void SSHLog_Write(LogLevel level, const std::string& event, const std::string& content);

// 简化封装：自动获取调用函数名（用于默认事件名）
#define SSHLog_WriteAuto(level, content) SSHLog_Write(level, __FUNCTION__, content)