// SSHSettings.cpp - INI配置文件操作实现
#include "SSHSettings.h"
#include "Windows/SSHWindow.h" // 用于获取NppData全局变量
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

// 获取NPP插件配置目录（通过NPP原生消息）
std::wstring SSHSettings_GetPluginsConfigDir() {
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

// 获取NppSSH.ini完整路径
std::wstring SSHSettings_GetIniFilePath() {
    std::wstring configDir = SSHSettings_GetPluginsConfigDir();
    return configDir + _T("\\") + NPP_SSH_INI_NAME;
}

// 写入INI：保存面板数量
bool SSHSettings_SavePanelCountToIni(int count) {
    std::wstring iniPath = SSHSettings_GetIniFilePath();
    TCHAR countStr[16];
    wsprintf(countStr, _T("%d"), count);  // 将整数转为字符串
    return WritePrivateProfileString(
        NPP_SSH_INI_SECTION,
        NPP_SSH_PANEL_COUNT_KEY,
        countStr,
        iniPath.c_str()
    ) != 0;
}

// 读取INI：加载面板数量
int SSHSettings_LoadPanelCountFromIni() {
    std::wstring iniPath = SSHSettings_GetIniFilePath();
    // 读取失败返回0
    return GetPrivateProfileInt(
        NPP_SSH_INI_SECTION,
        NPP_SSH_PANEL_COUNT_KEY,
        0,
        iniPath.c_str()
    );
}

// 删除INI配置（插件卸载时）
void SSHSettings_DeleteIniConfig() {
    std::wstring iniPath = SSHSettings_GetIniFilePath();
    if (PathFileExists(iniPath.c_str())) {
        DeleteFile(iniPath.c_str());
    }
}