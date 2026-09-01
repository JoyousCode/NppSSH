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
