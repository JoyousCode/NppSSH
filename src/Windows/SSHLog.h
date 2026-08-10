// SSHLog.h（调试 + 连接日志输出逻辑）
#pragma once

#include "SSHWindow.h"

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

// 初始化日志（插件启动调用）
void SSHLog_Init();