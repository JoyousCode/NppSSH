/*
 * SSHWindow：仅保留分发逻辑，所有具体实现（面板、注册表、SSH 连接）均转发到 SSHPanel 和 SSHConnection，不包含任何业务逻辑；
 * SSHPanel：承载面板创建、注册表操作、面板消息处理等所有面板相关逻辑，依赖 SSHConnection 处理连接状态；
 * SSHConnection：承载 SSH 连接、断开、状态管理等所有连接相关逻辑，独立无依赖（仅依赖系统库）；
 * SSHLog：自定义日志输出（输出文件的路径：插件配置目录下面\\plugins\\config\\NppSSH.log）
 * 调用链路：NppPluginSSH/PluginDefinition → SSHClient → SSHWindow（分发） → SSHPanel/SSHConnection/SSHLog（具体实现）。
*/

#pragma once
#include "Windows/SSHWindow.h"
#include "Windows/SSHPanel.h"
#include <Windows.h>

// 点击插件--》下拉创建面板触发，仅保留对外暴露的创建面板函数（核心逻辑已由SSHWindow转发）
void CreateNppSSHTerminal();
