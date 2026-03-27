//SSHConnection.h（SSH 连接核心逻辑声明）
#pragma once
#include <libssh2.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

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
void SSHConnection_Disconnect();
bool SSHConnection_IsConnected();
void SSHConnection_ResetState();