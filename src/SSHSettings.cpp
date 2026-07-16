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
// 获取NPP插件目录（通过NPP原生消息）
std::wstring SSHSettings_GetPluginsDir() {
    std::wstring configDir;
    TCHAR szConfigDir[MAX_PATH] = { 0 };

    // 调用NPP原生消息获取插件配置目录
    SendMessage(g_nppData._nppHandle, NPPM_GETPLUGINHOMEPATH, MAX_PATH, (LPARAM)szConfigDir);

    // 验证路径有效性
    if (_tcslen(szConfigDir) > 0 && PathIsDirectory(szConfigDir)) {
        configDir = szConfigDir;
    }
    else {
        // 降级方案：使用NPP安装目录下的plugins
        TCHAR szNppPath[MAX_PATH] = { 0 };
        GetModuleFileName(NULL, szNppPath, MAX_PATH);
        PathRemoveFileSpec(szNppPath);
        _stprintf_s(szConfigDir, MAX_PATH, _T("%s\\plugins"), szNppPath);
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
bool SSHSettings_SavePanelCount(int count) {
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
int SSHSettings_LoadPanelCount() {
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
void SSHSettings_DeleteFile() {
    std::wstring iniPath = SSHSettings_GetIniFilePath();
    if (PathFileExists(iniPath.c_str())) {
        DeleteFile(iniPath.c_str());
    }
}

///////////////////////////////////////////////////////////////////////// 新增
// 写入INI：保存指定面板的类型
bool SSHSettings_SavePanelType(int newPanelrealId, PanelType type) {
    std::wstring iniPath = SSHSettings_GetIniFilePath();

    // 拼接面板类型的键名（如PanelType_1）
    TCHAR typeKey[64] = { 0 };
    int retPrint = _stprintf_s(typeKey, _countof(typeKey), _T("%s%d"), NPP_SSH_PANEL_TYPE_KEY_PREFIX, newPanelrealId);
    if (retPrint < 0) return false; // 缓冲区溢出防护

    // 转换类型枚举为字符串
    TCHAR typeStr[16] = { 0 };
    _stprintf_s(typeStr, _countof(typeStr), _T("%d"), static_cast<int>(type));

    return WritePrivateProfileString(
        NPP_SSH_INI_SECTIONTYPE,
        typeKey,
        typeStr,
        iniPath.c_str()
    ) != 0;
}

// 读取INI：加载指定面板的类型
PanelType SSHSettings_LoadPanelTypeFromIni(int panelId) {
    std::wstring iniPath = SSHSettings_GetIniFilePath();
    // 拼接面板类型的键名
    TCHAR typeKey[64];
    _stprintf_s(typeKey, _countof(typeKey), _T("%s%d"), NPP_SSH_PANEL_TYPE_KEY_PREFIX, panelId);

    // 读取类型值（默认返回TerminalPanel类型）
    int typeVal = GetPrivateProfileInt(
        NPP_SSH_INI_SECTIONTYPE,
        typeKey,
        static_cast<int>(PanelType::SSHTermPanel),  // 默认值
        iniPath.c_str()
    );

    // 校验枚举值有效性，防止非法值
    switch (typeVal) {
    case static_cast<int>(PanelType::SSHTermPanel):
        return PanelType::SSHTermPanel;
    case static_cast<int>(PanelType::SSHAppPanel):
        return PanelType::SSHAppPanel;
    default:
        return PanelType::SSHTermPanel; // 非法值默认返回SSH类型
    }
}

// 获取全部面板ID与类型有序集合
std::vector<PanelIdTypeItem> SSHSettings_GetAllPanelLineList()
{
    std::vector<PanelIdTypeItem> result;
    int totalCount = SSHSettings_LoadPanelCount();
    if (totalCount <= 0)
    {
        NppSSH_LogInfoAuto("【SSHSettings_GetAllPanelLineList】面板数量≤0，返回空");
        return result;
    }

    std::wstring iniPath = SSHSettings_GetIniFilePath();
    const DWORD bufSize = 4096;
    TCHAR sectionBuf[bufSize] = { 0 };
    DWORD readLen = GetPrivateProfileSection(
        NPP_SSH_INI_SECTIONTYPE,
        sectionBuf,
        bufSize,
        iniPath.c_str()
    );

    if (readLen == 0)
    {
        NppSSH_LogInfoAuto("【SSHSettings_GetAllPanelLineList】GeneralPanelType无配置");
        return result;
    }

    TCHAR* pLine = sectionBuf;
    int currentLineSeq = 0; // INI第一行=0，第二行=0...从0开始计数
    while (*pLine != _T('\0'))
    {
        TCHAR* pEqual = _tcschr(pLine, _T('='));
        if (pEqual == nullptr)
        {
            pLine += _tcslen(pLine) + 1;
            currentLineSeq++;
            continue;
        }

        // 提取 PanelType_数字
        size_t keyLen = pEqual - pLine;
        size_t underscorePos = 0;
        for (; underscorePos < keyLen; underscorePos++)
        {
            if (pLine[underscorePos] == _T('_'))
                break;
        }
        if (underscorePos >= keyLen)
        {
            pLine += _tcslen(pLine) + 1;
            currentLineSeq++;
            continue;
        }

        int panelId = _ttoi(pLine + underscorePos + 1);
        int typeVal = _ttoi(pEqual + 1);
        PanelType curType = PanelType::SSHTermPanel;
        switch (typeVal)
        {
        case static_cast<int>(PanelType::SSHTermPanel):
            curType = PanelType::SSHTermPanel;
            break;
        case static_cast<int>(PanelType::SSHAppPanel):
            curType = PanelType::SSHAppPanel;
            break;
        default:
            curType = PanelType::SSHTermPanel;
            break;
        }

        // 存入，保留当前行序号，允许重复panelId
        PanelIdTypeItem item{};
        item.panelSeqId = currentLineSeq;
        item.panelrealId = panelId;
        item.type = curType;
        result.push_back(item);

        pLine += _tcslen(pLine) + 1;
        currentLineSeq++;
    }

    NppSSH_LogInfoAuto("【SSHSettings_GetAllPanelLineList】解析原始总行数：" + std::to_string(result.size()));
    return result;
}


// 启动重建面板,根据不同的type创建不同的面板
void SSHSettings_InitRecreatePanels() {
    int panelCount = SSHSettings_LoadPanelCount(); // 从INI加载
    if (panelCount <= 0) return;
    std::vector<PanelIdTypeItem> panelItemList = SSHSettings_GetAllPanelLineList();
    PanelType panelType = PanelType::SSHTermPanel;
    // 按注册表记录的数量重建面板，ID延续自注册表
    for (int seqIndex = 0; seqIndex <= (panelCount-1); seqIndex++) {
        NppSSH_LogInfoAuto("【SSHSettings_InitRecreatePanels】开始创建第" + std::to_string(seqIndex) + "个面板");
        int realPanelId = seqIndex;
        // 查找当前行号对应的配置
        for (const auto& item : panelItemList)
        {
            if (item.panelSeqId == seqIndex)
            {
                realPanelId = item.panelrealId;
                panelType = item.type;
                break;
            }
        }
        NppSSH_LogInfoAuto(
            "【重建面板】行序号seqIndex=" + std::to_string(seqIndex)
            + " 真实panelId=" + std::to_string(realPanelId)
            + " 类型=" + std::to_string(static_cast<int>(panelType))
        );
        switch (panelType) {
        case PanelType::SSHTermPanel:
            SSH_PanelInitRecreateSSHTermPanel(seqIndex, realPanelId);
            break;
        case PanelType::SSHAppPanel:
            SSH_PanelInitRecreateSSHAppPanel(seqIndex, realPanelId);//暂时用默认的
            break;
        default:
            SSH_PanelInitRecreateSSHTermPanel(seqIndex, realPanelId);
            //SSH_TerminalPanelIdOnNppStart(seqIndex, realPanelId);
        }
    }
}

// 删除单个面板ID对应的类型配置项，不存在无操作、不报错
void SSHSettings_ByRealIdRemove(int panelRealId) {
    std::wstring iniPath = SSHSettings_GetIniFilePath();
    if (iniPath.empty())
        return;

    // 拼接对应PanelType键名
    TCHAR typeKey[64] = { 0 };
    _stprintf_s(typeKey, _countof(typeKey), _T("%s%d"), NPP_SSH_PANEL_TYPE_KEY_PREFIX, panelRealId);

    // WritePrivateProfileString 第三个参数传NULL = 删除该key整行
    // 若key本身不存在，API不会报错，直接静默返回
    WritePrivateProfileString(
        NPP_SSH_INI_SECTIONTYPE,
        typeKey,
        NULL,
        iniPath.c_str()
    );
}


// 添加/覆盖文件方法：传入文件名+文件内容，保存到插件Config目录
void SSHSettings_SaveConfigTmpFile(const std::wstring& ExceFile, const std::wstring& ExceComd)
{
    // 获取插件配置根目录
    std::wstring configDir = SSHSettings_GetPluginsConfigDir();
    if (configDir.empty())
        return;

    // 拼接文件完整绝对路径
    std::wstring fullFilePath = configDir + L"\\" + ExceFile;

    // 创建文件并写入内容，覆盖已有文件
    HANDLE hFile = CreateFileW(
        fullFilePath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS, // 始终创建/覆盖文件
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
        return;

    // 宽字符转UTF8字节流写入文件
    std::string fileContent = WStringToUTF8(ExceComd);
    DWORD writeBytes = 0;
    WriteFile(hFile, fileContent.c_str(), (DWORD)fileContent.length(), &writeBytes, NULL);

    CloseHandle(hFile);
}

// 2、查询文件是否存在：存在返回完整绝对路径，不存在返回空wstring
std::wstring SSHSettings_GetConfigFileExistPath(const std::wstring& ExceFile)
{
    std::wstring configDir = SSHSettings_GetPluginsConfigDir();
    if (configDir.empty())
        return L"";

    // 拼接完整文件路径
    std::wstring fullFilePath = configDir + L"\\" + ExceFile;

    // 判断文件是否真实存在
    if (PathFileExistsW(fullFilePath.c_str()) && !PathIsDirectoryW(fullFilePath.c_str()))
    {
        return fullFilePath;
    }

    // 文件不存在返回空
    return L"";
}

// 3、根据文件名直接删除文件：无需判空、无返回值、无论是否存在直接执行删除
void SSHSettings_DeleteConfigFile(const std::wstring& ExceFile)
{
    std::wstring configDir = SSHSettings_GetPluginsConfigDir();
    if (configDir.empty())
        return;

    std::wstring fullFilePath = configDir + L"\\" + ExceFile;
    // 直接调用删除API，文件不存在也不会报错
    DeleteFileW(fullFilePath.c_str());
}