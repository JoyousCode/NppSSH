//SSHWindow.cpp（仅分发调用，无具体逻辑）
#include "SSHWindow.h"

#include "SSHBasePanel.h"
#include "SSHPanel.h"
#include "SSHConEmu.h"
#include "SSHTerminal.h"

std::vector<SSHBasePanel*> g_SSHPanelVec;
static std::mutex g_SSHPanelMutex;


// 全局变量转发（实际定义在SSHPanel中）
NppData& g_nppData = SSHPanel_GetGlobalNppData();
HINSTANCE& g_hInst = SSHPanel_GetGlobalHInst();
int& iconSize = SSHPanel_iconSize();


/**************（工具函数）***************/
void SSH_PanelVecBySeqIdUpdate(int startIndex) {
    for (size_t i = startIndex; i < g_SSHPanelVec.size(); ++i) {
        g_SSHPanelVec[i]->Set_panelSeqId(i);
    }
}
void SSH_PanelVecClearAll()
{
    for (auto* p : g_SSHPanelVec)
    {
        if (!p)
            continue;
        delete p;
    }
    g_SSHPanelVec.clear();
}
/**************（全局处理）***************/
SSHBasePanel* SSH_PanelVecBySeqId(int panelSeqId)
{
    if (panelSeqId < 0 || panelSeqId >= (int)g_SSHPanelVec.size())
        return nullptr;
    return g_SSHPanelVec[panelSeqId];
}
void SSH_PanelVecBySeqIdRemove(int panelSeqId) {
    if (panelSeqId < 0 || panelSeqId >= (int)g_SSHPanelVec.size())
    {
        NppSSH_LogInfoAuto("【全局处理】无效序号:" + std::to_string(panelSeqId));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(g_SSHPanelMutex);
        // delete this释放内存已由窗口过程手动处理，这里不再处理
        g_SSHPanelVec.erase(g_SSHPanelVec.begin() + panelSeqId);
        // 更新后续元素的 panelSeqId
        SSH_PanelVecBySeqIdUpdate(panelSeqId);
    }
}
SSHPanel* SSH_PanelVecBySeqIdGetSSHPanel(int panelSeqId)
{
    SSHBasePanel* pBase = SSH_PanelVecBySeqId(panelSeqId);
    if (!pBase) return nullptr;;
    return dynamic_cast<SSHPanel*>(pBase);
}
int SSH_PanelVecSize() {
    return g_SSHPanelVec.size();
}
int SSH_PanelVecGetInvalidSeqId() {
    if (g_SSHPanelVec.empty()) {
        return 1; // 如果向量为空，第一个可用 ID 是 1
    }

    // 创建一个临时向量来存储所有的 ID
    std::vector<int> ids;
    for (const auto& panel : g_SSHPanelVec) {
        ids.push_back(panel->Get_panelrealId());
    }

    // 排序 ID 列表
    std::sort(ids.begin(), ids.end());

    // 检查是否存在缺失的 ID
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] != static_cast<int>(i + 1)) {
            return static_cast<int>(i + 1); // 返回缺失的 ID
        }
    }

    // 如果没有缺失的 ID，返回下一个最大 ID
    return ids.back() + 1;
}
bool SSH_PanelVecIsHasConnection() { // 检查活跃连接
    bool hasActiveConnection = false;
    for (auto* panel : g_SSHPanelVec) {
        if (panel && panel->Get_isConnected()) {
            NppSSH_LogInfoAuto("【检查活跃连接】");
            hasActiveConnection = true;
            break;
        }
    }
    return hasActiveConnection;
}
void SSH_HandAllFree() {
    // 检查活跃连接并直接断开
    //bool hasActiveConnection = SSH_PanelVecIsHasConnection();
    //if (hasActiveConnection) {
    //    for (auto* panel : g_SSHPanelVec) {
    //        if (!panel) continue;
    //        SSHPanel* panel =  dynamic_cast<SSHPanel*>(panel);
    //        if (panel != nullptr) panel->disconnectSSH();
    //        SSHConEmu* conEmuPanel = dynamic_cast<SSHConEmu*>(panel);
    //        if (conEmuPanel != nullptr) {  }//待处理如果是ConEmu断开连接
    //    }
    //}
    SSHConnection_ClearAllSSHConnections();//已在该方法中直接断开连接后清空所有内容
    SSHTerminal_ClearAllSSHTerminal();
    SSH_PanelVecClearAll();
}


/**************（实际定义在SSHSettings中）***************/
std::wstring SSH_SettingsGetPluginsDir() {//获取插件所在文件夹绝对路径(_T("%s\\plugins"))
    return SSHSettings_GetPluginsDir();
}
void SSH_SettingsSavePanelCount(int count) {
    SSHSettings_SavePanelCount(count); // INI操作转发
}
int SSH_SettingsLoadPanelCount() {
    return SSHSettings_LoadPanelCount();
}
void SSH_SettingsDeleteFile() {
    SSHSettings_DeleteFile();
}
bool SSH_SettingsSavePanelType(int newPanelrealId, PanelType type) {
    return SSHSettings_SavePanelType(newPanelrealId, type);
}
// 读取INI：加载指定面板的类型
//PanelType SSH_LoadPanelTypeFromIni(int panelId) {
//    return SSHSettings_LoadPanelTypeFromIni(panelId);
//}
void SSH_SettingsInitRecreatePanels() {
    SSHSettings_InitRecreatePanels();
}
void SSH_SettingsByRealIdRemove(int panelRealId) {
    SSHSettings_ByRealIdRemove(panelRealId);
}
// 获取全部面板ID与类型有序集合
//std::vector<PanelIdTypeItem> SSH_GetAllPanelIdTypeList() {
//    return SSHSettings_GetAllPanelIdTypeList();
//}
void SSH_SettingsSaveConfigTmpFile(const std::wstring& ExceFile, const std::wstring& ExceComd) {
    SSHSettings_SaveConfigTmpFile(ExceFile, ExceComd);
}
std::wstring SSH_SettingsGetConfigFileExistPath(const std::wstring& ExceFile) {
    return SSHSettings_GetConfigFileExistPath(ExceFile);
}
void SSH_SettingsDeleteConfigFile(const std::wstring& ExceFile) {
    SSHSettings_DeleteConfigFile(ExceFile);
}


/**************（实际定义在SSHPanel中）***************/
void SSH_PanelInitRecreateTerminalPanel(int panelSeqId, int panelrealId) {//panelSeqId索引从0开始，panelrealId面板默认标题从1开始
    //int panelSeqId = g_SSHPanelSeqIdMap.size();panelSeqId++;
    NppSSH_LogInfoAuto("面板索引="+ std::to_string(panelSeqId) +"面板标题id="+ std::to_string(panelrealId));
    std::lock_guard<std::mutex> lock(g_SSHPanelMutex);
    if (panelSeqId < 0) { panelSeqId = 0;panelrealId = 1; }
    SSHPanel* pPanel = new SSHPanel(panelSeqId, panelrealId);
    SSHPanel_InitRecreatePanel(pPanel);
    g_SSHPanelVec.push_back(pPanel);
}
void SSH_PanelInitRecreateConEmuPanel(int panelSeqId, int panelrealId) {//panelSeqId索引从0开始，panelrealId面板默认标题从1开始
    //int panelSeqId = g_SSHPanelSeqIdMap.size();panelSeqId++;
    NppSSH_LogInfoAuto("面板索引=" + std::to_string(panelSeqId) + "面板标题id=" + std::to_string(panelrealId));
    std::lock_guard<std::mutex> lock(g_SSHPanelMutex);
    if (panelSeqId < 0) { panelSeqId = 0;panelrealId = 1; }
    SSHConEmu* pPanel = new SSHConEmu(panelSeqId, panelrealId);
    SSHConEmu_InitRecreatePanel(pPanel);
    g_SSHPanelVec.push_back(pPanel);
}
//HWND SSH_PanelGetLoginPanelHwnd(int panelSeqId) {//暂未使用
//    SSHPanel* pPanel = g_SSHPanelVec[panelSeqId];
//    return pPanel->getLoginPanel();        //获得登录面板句柄
//}
HWND SSH_PanelGetPanelHwnd(int panelSeqId) {
    return SSHPanel_GetPanelHwnd(panelSeqId);
}


/**************（实际定义在SSHConnection中）***************/
bool SSH_ConnectionHandle(int panelSeqId,const char* host, int port, const char* user, const char* pass) {
    return SSHConnection_Handle(panelSeqId,host, port, user, pass);   // SSH连接操作转发
}
void SSH_ConnectionOnDisconn(int panelSeqId) {
    SSHConnection_OnDisconn(panelSeqId);
}
bool SSH_ConnectionIsConn(int panelSeqId) {
    return SSHConnection_IsConn(panelSeqId);
}
void SSH_ConnectionResetConn(int panelSeqId) {
    SSHConnection_ResetConn(panelSeqId);
}
bool SSH_ConnectionExecuteCommand(int panelSeqId, const std::string& cmd) {
    return SSHConnection_ExecuteCommand(panelSeqId, cmd);   // 命令执行转发
}
std::string SSH_ConnectionPanelPrompt(int panelSeqId) {
    return SSHConnection_PanelPrompt(panelSeqId);
}
void SSH_ConnectionPtySize(int panelSeqId, int cols, int rows) {
    SSHConnection_PtySize(panelSeqId, cols, rows);
}


/**************（实际定义在SSHLog中）***************/
void NppSSH_LogDebug(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_DEBUG, event, content);
}
void NppSSH_LogInfo(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_INFO, event, content);
}
void NppSSH_LogWarn(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_WARN, event, content);
}
void NppSSH_LogError(const std::string& event, const std::string& content) {
    SSHLog_Write(LogLevel::LOG_ERROR, event, content);  
}
///// ===================== 日志调用正确测试示例=====================
//// 1. 自动获取当前函数名作为 event（最常用）
//NppSSH_LogInfoAuto("==============测试日志使用开始==========");
//NppSSH_LogInfoAuto("SSH连接成功，Socket与会话已创建");

//// 2. 手动指定 event 名称
//NppSSH_LogInfo("SSH_Handshake", "SSH协议握手完成，服务器响应正常");

//// 3. event 传空字符串（触发兜底 unknown）
//NppSSH_LogInfo("", "用户密码认证通过，登录成功");

//// 4. 错误级别日志（测试）
//NppSSH_LogError("SSH_Connect_Test", "测试错误日志：连接流程正常结束");

//// 5. 调试级别日志
//NppSSH_LogDebug("SSH_Session", "libssh2会话已初始化，阻塞模式开启");

//// 6. 警告级别日志
//NppSSH_LogWarn("SSH_KeepAlive", "测试警告：连接成功，心跳未启动");//支持“\n”换行，例如：心跳\n未启动

//// 7. 输出服务器远程信息（你要的握手/返回内容）
//std::string serverInfo = "服务器主机：" + std::string(host) + " 端口：" + std::to_string(port) + " 用户：" + std::string(user);
//NppSSH_LogInfo("SSH_ServerInfo", serverInfo);

//// 8. event 传空字符串 + 错误级别（兜底测试）
//NppSSH_LogError("", "连接状态已标记为已连接");
//NppSSH_LogInfoAuto("==============测试日志使用结束==========");


/**************（实际定义在SSHTerminal中）***************/
HWND SSH_TerminalInitControlPanel(HWND hParent, int panelSeqId) {
    return SSHTerminal_InitControlPanel(hParent, panelSeqId);
}
void SSH_TerminalDisconnectHandle(int panelSeqId) {// 未用
    SSHTerminal_DisconnectHandle(panelSeqId);
}
void SSH_TerminalAppendTextHandle(int panelSeqId, const std::string& text) {
    SSHTerminal_AppendTextHandle(panelSeqId, text);
}
void SSH_TerminalSetPanelPrompt(int panelSeqId, const std::string prompt) {
    SSHTerminal_SetPanelPrompt(panelSeqId, prompt);
}
void SSH_TerminalSetCommandRunning(int panelSeqId, bool isCommandRunning) {
    SSHTerminal_SetCommandRunning(panelSeqId, isCommandRunning);
}
void SSH_TerminalSetEnglishType(int panelSeqId) {
    SSHTerminal_SetEnglishType(panelSeqId);
}
void SSH_TerminalExecuteClear(int panelSeqId) {
    SSHTerminal_ExecuteClear(panelSeqId);
}
std::string SSH_TerminalPanelPrompt(int panelSeqId) {
    return SSHTerminal_PanelPrompt(panelSeqId);
}
void SSH_TerminalBySeqIdRemove(int panelSeqId) {
    SSHTerminal_BySeqIdRemove(panelSeqId);
}
void SSH_TerminalBySeqIdReset(int panelSeqId) {
    SSHTerminal_BySeqIdReset(panelSeqId);
}
void SSH_TerminalResize(HWND hParent, int panelSeqId) {
    SSHTerminal_Resize(hParent, panelSeqId);
}