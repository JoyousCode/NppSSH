// resource.h
#ifndef RESOURCE_H
#define RESOURCE_H

// 对话框ID
#define IDD_SSH_PANEL 1001

// 控件ID
#define IDC_OUTPUT_EDIT 1002
#define IDC_BTN_CONNECT_SSH 1010  // 连接SSH按钮
#define IDC_BTN_DISCONNECT_SSH 1011  // 断开SSH按钮（新增）
#define IDI_ICON_CONNECT 1012       // 连接图标ID（需与RC文件中一致）
#define IDI_ICON_DISCONNECT 1013    // 断开图标ID（需与RC文件中一致）
#define IDI_ICON_NPPSSH  1014

//登录控件ID
#define IDD_SSH_LOGIN  1600
#define IDC_HOST 1020
#define IDC_PORT 1021
#define IDC_USER 1022
#define IDC_PASS 1023
#define IDC_BTN_CONNECT 1024
#define IDC_BTN_TEST  1025  // 新增测试按钮ID


#endif // RESOURCE_H

