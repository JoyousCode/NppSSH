#pragma once
#include "SSHWindow.h"
#include "json.hpp"

// 单条SSH登录历史记录
typedef struct tagSSHLoginHistoryItem
{
    wchar_t szHost[256];
    wchar_t szPort[32];
    wchar_t szUser[256];
    wchar_t szPassEncBase64[1024]; // CryptProtectData加密后base64字符串，不再存明文
    wchar_t szDir[256];
} SSHLoginHistoryItem;

// ========= JSON历史记录操作 =========
// 获取历史JSON完整路径 NppSSHLoginData.json
std::wstring SSHLogin_GetHistoryJsonPath();
// 加载全部登录历史
std::vector<SSHLoginHistoryItem> SSHLogin_LoadHistoryJson();
// 保存一条记录，host相同直接覆盖，不存在新增
void SSHLogin_SaveHistoryJson(const SSHLoginHistoryItem& item);
// 删除单条历史记录，按结构体删除一条数据
void SSHLogin_DeleteHistoryByItem(const SSHLoginHistoryItem* pItem);

void SSHLoginModal_WindowsModal(SSHLoginModal* pOut);

INT_PTR CALLBACK SSH_LoginDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool isEmptyInputToSSHLoginModal(SSHLoginModal* loginPanel, const char* host, const char* port, const char* user, const char* pass, const char* director);



