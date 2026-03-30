// SSHSettings.h - INI配置文件操作封装
#pragma once
#define WIN32_LEAN_AND_MEAN  // 减少Windows头文件冗余
#include <winsock2.h>
#include <ws2tcpip.h>	// 补充IPv6相关定义（可选，libssh2可能需要）
#pragma comment(lib, "ws2_32.lib") 

#include <Windows.h>
#include <tchar.h>
#include <string>

// 配置文件名
#define NPP_SSH_INI_NAME _T("NppSSH.ini")
// INI中面板数量的键名
#define NPP_SSH_PANEL_COUNT_KEY _T("PanelCount")
// INI中默认节名
#define NPP_SSH_INI_SECTION _T("General")

// 获取NPP插件配置目录（动态适配用户/默认路径）
std::wstring SSHSettings_GetPluginsConfigDir();

// 获取NppSSH.ini完整路径
std::wstring SSHSettings_GetIniFilePath();

// 写入INI：保存面板数量
bool SSHSettings_SavePanelCountToIni(int count);

// 读取INI：加载面板数量
int SSHSettings_LoadPanelCountFromIni();

// 删除INI配置（插件卸载时）
void SSHSettings_DeleteIniConfig();