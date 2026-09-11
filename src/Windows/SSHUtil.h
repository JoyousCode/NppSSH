// SSHUtil.h工具处理的头文件，只做声明
#pragma once
#include "SSHWindow.h"
#include <wincrypt.h>
#include <atlbase.h>


std::string WStringToLogStr(const std::wstring& wstr);
std::string PtrToHexStr(void* ptr);
std::string IntToStr(int num);

void DeBugOutPutText(const std::wstring& text);
void DeBugOutPutText(const std::string& text);

std::wstring UTF8ToWstring(const std::string& str);
std::wstring GBKToWstring(const std::string& str);
std::string IntToHexStr(DWORD val);
std::string WStringToUTF8(const std::wstring& wstr);
std::wstring HwndToWString(HWND hWnd);
std::string HwndToString(HWND hWnd);

void CenterWindow(HWND hWndChild, HWND hWndParent);
std::wstring charToWString(const char* szSrc, UINT codepage = CP_UTF8);
std::string CheckHwndParentChildRelation(HWND hRoot, HWND hTarget);//测试专用，查看句柄之间的关系

// ========== 密码加密辅助函数（Windows CryptProtectData） ==========
// 加密明文密码，输出base64宽字符串
bool SSH_EncryptPasswordToBase64(const std::wstring& plainPwd, std::wstring& outBase64);
// base64密文解密得到明文密码
bool SSH_DecryptPasswordFromBase64(const std::wstring& base64Str, std::wstring& outPlainPwd);

//登录地址合法校验
bool IsValidIPv4(const std::wstring& s, bool& allNumberSeg);
bool IsValidIPv6(const std::wstring& s);
bool IsHostNameLoose(const std::wstring& s);
bool IsRealPuttyGuiExe(const std::wstring& exePath);

// 输入法中英文切换（真正安全、无循环）
void imm_chineseType(HWND hEdit);

std::wstring CleanAnsiEscapeSequences(const std::wstring& input);
std::string CleanAnsiEscapeSequences(const std::string& input);

void SSHProgress_Init();
/**
 * @brief 【统一工具函数】带消息泵的等待函数，单函数支持两种模式
 * 示例：BOOL bGotHandle = WaitWithMsgLoop(&hResultWnd, 3000);
 *		像提示框一样仅仅等3000毫秒，WaitWithMsgLoop(nullptr, 3000);
 * @param phWnd 可选：待等待窗口句柄指针。传nullptr则为纯延时等待，不检测窗口
 * @param dwTimeoutMs 最大等待毫秒数
 * @return BOOL
 *         - phWnd != nullptr：TRUE=超时前获取有效窗口；FALSE=超时未获取
 *         - phWnd == nullptr：固定返回TRUE，等待满时长结束
 * @note 等待期间持续驱动主线程消息循环，UI不会冻结，替代Sleep
 */
BOOL WaitWithMsgLoop(HWND* phWnd, DWORD dwTimeoutMs);
INT_PTR CALLBACK SshWaitDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
HWND CreateSshWaitDialog(HWND hParent, HWND hLoginPanel, bool bTestMsg);
void UpdateSshWaitProgress(HWND hDlg, int nPercent, LPCWSTR szDetailText, LPCWSTR szMainTitle);
void CloseSshWaitDialog(HWND& hDlg);