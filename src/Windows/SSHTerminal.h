// SSHTerminal.h模拟终端的头文件，只做声明
#pragma once
#define _WIN32_WINNT 0x0A00
#include "SSHWindow.h"
#include <shlwapi.h>
#include <algorithm>
#include <windows.h>
#include <consoleapi2.h>
#include <processenv.h>
#include <richedit.h>       // RichEdit 核心头文件
#include <commctrl.h>       // 可选（若需高级功能）
#include <sstream>
#include <iterator>
#include <unordered_map>
//#pragma comment(lib, "msftedit.lib")
// PTY 特性配置（适配不同终端类型的核心）
struct PTYFeatures {
    bool supportANSI;          // 是否支持ANSI转义序列（dumb不支持，其他支持）
    bool support256Color;      // 是否支持256色（仅xterm-256color支持）
    bool supportCursorMove;    // 是否支持光标移动（dumb不支持）
    bool supportBold;          // 是否支持加粗
    bool crlfToLf;             // 换行规则：CR+LF → LF（linux/xterm）
    bool lfToCrlf;             // 换行规则：LF → CR+LF（vt100）
    int defaultFGColor;        // 默认前景色
    int defaultBGColor;        // 默认背景色
};

// 全局PTY特性映射（扩展时只需加新类型的配置）
//{ "screen", { true, true, true, true, true, false, RGB(192,192,192), RGB(0,0,0) } },新增 PTY 类型：只需在 g_ptyFeatureMap 中添加新类型的特性配置，无需修改其他逻辑。
static std::unordered_map<std::string, PTYFeatures> g_ptyFeatureMap = {
    {"xterm-256color", {true, true, true, true, true, false, RGB(192,192,192), RGB(0,0,0)}},
    {"xterm",          {true, false, true, true, true, false, RGB(192,192,192), RGB(0,0,0)}},
    {"vt100",          {true, false, true, true, false, true, RGB(255,255,255), RGB(0,0,0)}},
    {"dumb",           {false, false, false, false, true, false, RGB(255,255,255), RGB(0,0,0)}},
    {"linux",          {true, false, true, true, true, false, RGB(192,192,192), RGB(0,0,0)}}
};
// ANSI 基础 16 色映射（标准终端颜色）
struct AnsiColor {
    const char* name;
    COLORREF rgb;
};
// ANSI 30~37 标准色，完全对齐Windows终端
const COLORREF ANSI_COLORS[16] = {
    RGB(0,0,0),        //30黑
    RGB(194,54,33),    //31红
    RGB(37,188,36),    //32绿
    RGB(173,173,39),   //33黄
    RGB(73,46,155),    //34蓝(目录)
    RGB(173,54,174),   //35紫
    RGB(54,187,188),   //36青
    RGB(209,209,209),  //37白
    //90~97高亮
    RGB(129,131,131),
    RGB(249,56,51),
    RGB(49,231,34),
    RGB(234,236,35),
    RGB(88,51,255),
    RGB(249,53,248),
    RGB(20,240,240),
    RGB(233,233,233)
};
class SSHTerminal {
public:
    SSHTerminal();
    ~SSHTerminal();
    // 子类化终端编辑框过程监听
    static LRESULT CALLBACK TerminalEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    // 初始化终端编辑框（迁移自initPanel的编辑框创建逻辑）
    HWND InitTerminalEditBox(HWND hParent);
    void disConnection();
    void resetSSHTerminal();
    void SizeSSHTerminal(HWND hParent);

    HWND Get_TerminalHandle() const;

    // 输出文本到输出框（迁移自AppendOutputText）
    void AppendOutputText(const std::string& text);

    // 检查光标位置是否合法（迁移自IsCursorInEditableArea）
    bool IsCursorInEditableArea() const;

    // 获取/设置回车要执行的命令（迁移自cmd）
    void SetCmd(const char* cmdStr);
    const char* GetCmd() const;

    // 获取/设置命令提示符（迁移自Prompt）
    void SetPrompt(const std::string promptStr);
    const std::string& GetPrompt() const;

    // 获取编辑框句柄
    int GetPanelId() const { return _panelId; }
    void SetPanelId(int panelId) { _panelId = panelId; }
    WNDPROC GetOldEditProc() const { return _oldEditProc; }
    void SetIsCommandRunning(bool isCommandRunning) {_isCommandRunning = isCommandRunning;}
    const bool GetIsCommandRunning() const {return _isCommandRunning;}
    HWND Get_hwndParent() const { return _hwndParent; }


    // 解析 ANSI 转义序列（核心：提取颜色并设置文本颜色）
    void SetPTYType(const std::string& ptyType);
    const PTYFeatures& GetPTYFeatures() const; // 获取当前PTY特性（对外提供只读访问）
    void ParseAnsiParseOnly(const std::wstring& seq, CHARFORMAT2W& outCf);//只解析参数、不操作控件、不 SetSel
    void StoreTerminalContent(wchar_t ch);//单字符入库函数
    void StoreTerminalContent(const std::string& str);//string字符入库函数
    std::wstring GetStoreContent()const { return _oldStoreContent; }//获取存储内容
    void SetStoreContent(const std::wstring wstr) { _oldStoreContent = std::move(wstr); }
    
private:
    HWND _hTerminal;
    HWND _hwndParent = nullptr;
    bool _initialized = false;
    std::wstring _oldStoreContent; // 只追加、永不删除

    HMODULE _hRichEditLib = nullptr;
    std::string _currentPTYType;    // 当前使用的PTY类型（如xterm-256color）
    PTYFeatures _currentPTYFeatures;// 当前PTY的特性配置

    int _panelId;
    std::string _cmd;             // 回车需要执行的命令（迁移自cmd）
    std::string _prompt;               // 命令提示符（迁移自Prompt）
    WNDPROC _oldEditProc = nullptr; // 传统子类化保存旧过程
    bool _isCommandRunning = false; // 标记后台命令是否正在执行
};

HWND SSHTerminal_InitTerminalEditBox(HWND hParent,int panelId);
void SSHTerminal_disconnectTerminalEditBox(int panelIndex);
void SSHTerminal_resetSSHTerminal(int panelIndex);
void SSHTerminal_SizeSSHTerminal(HWND hParent,int panelIndex);


void SSHTerminal_AppendOutput(int panelIndex, const std::string& text);
void SSHTerminal_PanelPrompt(int panelIndex, std::string prompt);
void SSHTerminal_SetIsCommandRunning(int panelIndex, bool isCommandRunning);
void SSHTerminal_SetEnglishType(int panelIndex);
void SSHTerminal_ClearOutputText(int panelIndex);
std::string SSHTerminal_getPanelPrompt(int panelIndex);

SSHTerminal* getSSHTerminal(int panelIndex);

// 工具函数声明（日志专用）
inline std::wstring GBKToWstring(const std::string& gbkStr);
inline std::string PtrToHexStr(void* ptr);
inline std::string IntToStr(int num);
