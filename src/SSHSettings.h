// SSHSettings.h - INI配置文件操作封装
#pragma once
#define WIN32_LEAN_AND_MEAN  // 减少Windows头文件冗余
#include <winsock2.h>
#include <ws2tcpip.h>	// 补充IPv6相关定义（可选，libssh2可能需要）
#pragma comment(lib, "ws2_32.lib") 

#include <Windows.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

// 配置文件名
#define NPP_SSH_INI_NAME _T("NppSSH.ini")
// INI中面板数量的键名
#define NPP_SSH_PANEL_COUNT_KEY _T("PanelCount")
// INI中默认节名
#define NPP_SSH_INI_SECTION _T("General")
#define NPP_SSH_INI_SECTIONTYPE _T("GeneralPanelType")
// 面板类型的键名前缀（拼接面板ID，如PanelType_1）
#define NPP_SSH_PANEL_TYPE_KEY_PREFIX _T("PanelType_")
enum class PanelType {
    SSHTermPanel = 1,        // SSHTermPanel面板类型
    SSHAppPanel = 2,       // SSHAppPanel面板类型
};
// 存储面板ID与对应类型的结构体
struct PanelIdTypeItem
{
    int panelSeqId;
    int panelrealId;
    PanelType type;
};
// 获取NPP插件配置目录（动态适配用户/默认路径）
std::wstring SSHSettings_GetPluginsConfigDir();
std::wstring SSHSettings_GetPluginsDir();

// 获取NppSSH.ini完整路径
std::wstring SSHSettings_GetIniFilePath();

// 写入INI：保存面板数量
bool SSHSettings_SavePanelCount(int count);

// 读取INI：加载面板数量
int SSHSettings_LoadPanelCount();

// 删除INI配置（插件卸载时）
void SSHSettings_DeleteFile();



// 写入INI：保存指定面板的类型
bool SSHSettings_SavePanelType(int panelId, PanelType type);

// 读取INI：加载指定面板的类型
PanelType SSHSettings_LoadPanelTypeFromIni(int panelId);

// 启动重建面板,根据不同的type创建不同的面板
void SSHSettings_InitRecreatePanels();

// 删除指定面板ID对应的类型配置项，不存在直接返回不报错
void SSHSettings_ByRealIdRemove(int panelRealId);

// 获取全部面板ID与类型有序集合
std::vector<PanelIdTypeItem> SSHSettings_GetAllPanelLineList();

// 保存配置目录临时文件
void SSHSettings_SaveConfigTmpFile(const std::wstring& ExceFile, const std::wstring& ExceComd);
// 查询配置目录文件，存在返回绝对路径，不存在返回空
std::wstring SSHSettings_GetConfigFileExistPath(const std::wstring& ExceFile);
// 直接删除配置目录指定文件（无判空、无返回值）
void SSHSettings_DeleteConfigFile(const std::wstring& ExceFile);

