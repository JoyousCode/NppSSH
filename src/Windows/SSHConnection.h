//SSHConnection.h（SSH 连接核心逻辑声明）
#pragma once
#include <unordered_map>
#include <libssh2.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "SSHWindow.h"
#include "DockingFeature/DockingDlgInterface.h"
#include <tchar.h>
#include <mutex>
#include <thread>
#include <stdexcept>
#include <string>
#include <future>

// SSH连接全局状态封装
LIBSSH2_SESSION*& SSHConnection_GetSession();
SOCKET& SSHConnection_GetSocket();
bool& SSHConnection_GetConnectedState();
const char*& SSHConnection_GetHost();
int& SSHConnection_GetPort();
const char*& SSHConnection_GetUser();
const char*& SSHConnection_GetPass();

// SSH连接操作具体声明
bool SSHConnection_Connect(const char* host, int port, const char* user, const char* pass);
void SSHConnection_Disconnect();// 断开SSH连接
bool SSHConnection_IsConnected();// 判断是否连接
void SSHConnection_ResetState();// 重置连接状态

// 新增：根据面板索引断开对应连接（核心）
void SSHConnection_DisconnectByPanelIndex(int panelIndex);
void SSHConnection_BindPanelIndex(int panelIndex);