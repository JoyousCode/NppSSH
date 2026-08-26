// resource.h
#ifndef RESOURCE_H
#define RESOURCE_H

// 对话框ID
#define IDD_SSH_PANEL 1001

// 控件ID
#define IDC_OUTPUT_EDIT 1002	// 输出框
#define IDC_BTN_CONNECT_SSH 1010  // 连接SSH按钮
#define IDC_BTN_DISCONNECT_SSH 1011  // 断开SSH按钮（新增）
#define IDI_ICON_CONNECT 1012       // 连接图标ID（需与RC文件中一致）
#define IDI_ICON_DISCONNECT 1013    // 断开图标ID（需与RC文件中一致）
#define IDI_ICON_NPPSSH  1014

//登录对话框控件ID
#define IDD_SSH_LOGIN  1600
#define IDD_SSH_Putty_LOGIN  1601


#define IDC_HOST 1020
#define IDC_PORT 1021
#define IDC_USER 1022
#define IDC_PASS 1023
#define IDC_BTN_CONNECT 1024
#define IDC_BTN_TEST  1025  // 新增测试按钮ID
#define IDC_BTN_EYE 1026	// 眼睛按钮控件ID
#define IDI_EYE_HIDE 1027	//隐藏密码图标
#define IDI_EYE_SHOW 1028	//显示密码图标
#define IDC_DIRECTOR 1029	// 初始远程目录输入框
#define IDC_BTN_SUBMIT 1030	// 确认登录按钮
#define IDC_CHK_SAVE_HIST 1031	// 保存历史记录复选框


//登录面板控件ID
#define IDI_ICON_PUTTY  1040
#define IDC_BTN_CONNECT_PUTTY 1041  // 连接SSH按钮
#define IDC_STATIC_PUTTY_TIP     1042
#define IDC_EDIT_PUTTY_PATH      1043
#define IDC_BTN_BROWSE_PUTTY     1044
#define IDI_ICON_SELECT  1045
#define IDI_ICON_CLOSE  1046
#define IDC_BTN_CLOSE_SSH 1047
#define IDI_ICON_WINTOP  1048
#define IDI_ICON_CLOSEWINTOP  1049 
#define IDC_BTN_PUTTYTOP_SSH 1050



#endif // RESOURCE_H

