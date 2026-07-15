// SSHUtil.h工具处理的头文件，只做声明
#pragma once
#include "SSHWindow.h"
#include <shlwapi.h>
#include <algorithm>
#include <windows.h>
#include <processthreadsapi.h> // 进程/线程API
#include <processenv.h>
#include <thread>
#include <sstream>
#include <iterator>
#include <unordered_map>


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