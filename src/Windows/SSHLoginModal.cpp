#include "SSHLoginModal.h"

using json = nlohmann::json;

// 获取JSON完整路径
std::wstring SSHLogin_GetHistoryJsonPath()
{
    std::wstring configDir = SSH_SettingsGetPluginsConfigDir();
    if (configDir.empty()) return L"";
    return configDir + L"\\NppSSHLoginData.json";
}
// 加载JSON历史
std::vector<SSHLoginHistoryItem> SSHLogin_LoadHistoryJson()
{
    std::vector<SSHLoginHistoryItem> ret;
    std::wstring jsonPath = SSHLogin_GetHistoryJsonPath();
    HANDLE hFile = CreateFileW(jsonPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return ret;
    DWORD dwRead = 0;
    DWORD dwFileSize = GetFileSize(hFile, nullptr);
    std::vector<char> buf(dwFileSize + 1, 0);
    ReadFile(hFile, buf.data(), dwFileSize, &dwRead, nullptr);
    CloseHandle(hFile);
    try
    {
        json jroot = json::parse(buf.data());
        for (auto& jitem : jroot)
        {
            SSHLoginHistoryItem it{};
            std::wstring h = UTF8ToWstring(jitem["host"].get<std::string>());
            std::wstring p = UTF8ToWstring(jitem["port"].get<std::string>());
            std::wstring u = UTF8ToWstring(jitem["user"].get<std::string>());
            std::wstring pe = UTF8ToWstring(jitem["pass_enc"].get<std::string>());
            std::wstring d = UTF8ToWstring(jitem["dir"].get<std::string>());

            wcsncpy_s(it.szHost, _countof(it.szHost), h.c_str(), _TRUNCATE);
            wcsncpy_s(it.szPort, _countof(it.szPort), p.c_str(), _TRUNCATE);
            wcsncpy_s(it.szUser, _countof(it.szUser), u.c_str(), _TRUNCATE);
            wcsncpy_s(it.szPassEncBase64, _countof(it.szPassEncBase64), pe.c_str(), _TRUNCATE);
            wcsncpy_s(it.szDir, _countof(it.szDir), d.c_str(), _TRUNCATE);
            ret.push_back(it);
        }
    }
    catch (...)
    {
        // json解析失败直接返回空
    }
    return ret;
}
// 保存历史，host相同覆盖
void SSHLogin_SaveHistoryJson(const SSHLoginHistoryItem& item)
{
    if (item.szHost[0] == 0) return;
    std::vector<SSHLoginHistoryItem> list = SSHLogin_LoadHistoryJson();
    bool found = false;
    for (auto& it : list)
    {
        if (wcscmp(it.szHost, item.szHost) == 0)
        {
            it = item;
            found = true;
            break;
        }
    }
    if (!found)
    {
        list.push_back(item);
    }
    // 组装json
    json jarr = json::array();
    for (auto& it : list)
    {
        json jo;
        jo["host"] = WStringToUTF8(std::wstring(it.szHost));
        jo["port"] = WStringToUTF8(std::wstring(it.szPort));
        jo["user"] = WStringToUTF8(std::wstring(it.szUser));
        jo["pass_enc"] = WStringToUTF8(std::wstring(it.szPassEncBase64));
        jo["dir"] = WStringToUTF8(std::wstring(it.szDir));
        jarr.push_back(jo);
    }
    std::string jsonText = jarr.dump(4);
    std::wstring jsonPath = SSHLogin_GetHistoryJsonPath();
    // 先写tmp文件，防止崩溃损坏正式文件
    std::wstring tmpPath = jsonPath + L".tmp";
    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD dwWrite = 0;
    WriteFile(hFile, jsonText.data(), (DWORD)jsonText.size(), &dwWrite, nullptr);
    CloseHandle(hFile);
    // 替换正式文件
    MoveFileExW(tmpPath.c_str(), jsonPath.c_str(), MOVEFILE_REPLACE_EXISTING);
}
//按结构体删除一条数据
void SSHLogin_DeleteHistoryByItem(const SSHLoginHistoryItem* pItem)
{
    if (pItem == nullptr) return;
    std::vector<SSHLoginHistoryItem> list = SSHLogin_LoadHistoryJson();
    bool bChanged = false;
    for (auto it = list.begin(); it != list.end(); )
    {
        // 完整比对全部字段，避免host重复误删多条
        bool same = (wcscmp(it->szHost, pItem->szHost) == 0)
            && (wcscmp(it->szPort, pItem->szPort) == 0)
            && (wcscmp(it->szUser, pItem->szUser) == 0)
            && (wcscmp(it->szDir, pItem->szDir) == 0)
            && (wcscmp(it->szPassEncBase64, pItem->szPassEncBase64) == 0);
        if (same)
        {
            it = list.erase(it);
            bChanged = true;
        }
        else
        {
            ++it;
        }
    }
    if (!bChanged) return;

    json jarr = json::array();
    for (auto& it : list)
    {
        json jo;
        jo["host"] = WStringToUTF8(std::wstring(it.szHost));
        jo["port"] = WStringToUTF8(std::wstring(it.szPort));
        jo["user"] = WStringToUTF8(std::wstring(it.szUser));
        jo["pass_enc"] = WStringToUTF8(std::wstring(it.szPassEncBase64));
        jo["dir"] = WStringToUTF8(std::wstring(it.szDir));
        jarr.push_back(jo);
    }
    std::string jsonText = jarr.dump(4);
    std::wstring jsonPath = SSHLogin_GetHistoryJsonPath();
    std::wstring tmpPath = jsonPath + L".tmp";
    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD dwWrite = 0;
    WriteFile(hFile, jsonText.data(), (DWORD)jsonText.size(), &dwWrite, nullptr);
    CloseHandle(hFile);
    MoveFileExW(tmpPath.c_str(), jsonPath.c_str(), MOVEFILE_REPLACE_EXISTING);
}


void SSHLoginModal_WindowsModal(SSHLoginModal* pOut)
{
    if (!pOut) return;
    ZeroMemory(pOut, sizeof(SSHLoginModal));

    // 将结构体指针作为DialogBoxParam的lParam传入
    INT_PTR nDlgRet = DialogBoxParamW(
        g_hInst,
        MAKEINTRESOURCE(IDD_SSH_Putty_LOGIN),
        g_nppData._nppHandle,
        SSH_LoginDlgProc,
        (LPARAM)pOut   
    );

    // DialogBoxParam返回，模态框已经关闭
    if (nDlgRet == IDOK)
    {
        pOut->bOk = TRUE;
    }
    else
    {
        pOut->bOk = FALSE;
    }
}

static LRESULT CALLBACK ComboDropList_SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    HWND hCombo = reinterpret_cast<HWND>(dwRefData);

    if (uMsg == WM_LBUTTONDOWN)
    {
        POINT ptClient;
        ptClient.x = LOWORD(lParam);
        ptClient.y = HIWORD(lParam);
        POINT ptScreen = ptClient;
        ClientToScreen(hWnd, &ptScreen);

        int itemCount = (int)SendMessageW(hWnd, LB_GETCOUNT, 0, 0);
        int hitItem = -1;
        bool bHitDelete = false;

        char logBuf[256]{};
        sprintf_s(logBuf, "DropList WM_LBUTTONDOWN ptScreen(%d,%d), totalItem=%d", ptScreen.x, ptScreen.y, itemCount);
        NppSSH_LogInfoAuto(logBuf);

        // 遍历全部行，找到鼠标落在哪个item屏幕矩形
        for (int i = 0; i < itemCount; i++)
        {
            RECT rcItemClient{};
            LRESULT lr = SendMessageW(hWnd, LB_GETITEMRECT, i, (LPARAM)&rcItemClient);
            if (lr == LB_ERR) continue;

            RECT rcScreen = rcItemClient;
            MapWindowPoints(hWnd, nullptr, (LPPOINT)&rcScreen, 2);

            char rectLog[512]{};
            sprintf_s(rectLog, "DropList i=%d rcScreen L=%d R=%d T=%d B=%d", i, rcScreen.left, rcScreen.right, rcScreen.top, rcScreen.bottom);
            NppSSH_LogInfoAuto(rectLog);

            if (PtInRect(&rcScreen, ptScreen))
            {
                hitItem = i;
                // 判断是否命中该行最右侧36px删除按钮
                if (ptScreen.x >= (rcScreen.right - 36))
                {
                    bHitDelete = true;
                }
                break;
            }
        }

        if (hitItem != -1)
        {
            char hitLog[256]{};
            sprintf_s(hitLog, "DropList hitItem=%d bHitDelete=%d", hitItem, bHitDelete);
            NppSSH_LogInfoAuto(hitLog);
        }

        // 命中删除按钮：直接Post删除消息，return 0吃掉鼠标消息，不调用DefSubclassProc，彻底屏蔽原生选中行为
        if (bHitDelete && hitItem != -1)
        {
            HWND hDlg = (HWND)GetPropW(hCombo, L"_DLG_HWND");
            if (hDlg)
            {
                NppSSH_LogInfoAuto("DropList：命中删除按钮，Post WM_DELETE_COMBO_ITEM");
                PostMessageW(hDlg, WM_DELETE_COMBO_ITEM, (WPARAM)hCombo, (LPARAM)hitItem);
            }
            SendMessageW(hWnd, LB_SETCURSEL, (WPARAM)-1, 0);
            return 0; // 吃掉WM_LBUTTONDOWN，不再往下走，不会触发CBN_SELCHANGE
        }

        // 没有命中删除，走原生逻辑
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

INT_PTR CALLBACK SSH_LoginDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

    SSHLoginModal* pPanel = nullptr;
    // 统一从窗口属性读取面板指针，替代每次遍历全局vector
    wchar_t buf[128]{};
    swprintf_s(buf, _countof(buf), L"SSHLoginModal-%p", hWnd);
    pPanel = (SSHLoginModal*)GetPropW(hWnd, buf);
    switch (uMsg)
    {
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        HBRUSH hBgBrush = (HBRUSH)GetPropW(hWnd, L"_DlgBgBrush");
        FillRect(hdc, &rcClient, hBgBrush);
        return TRUE;
    }
    // 只处理静态文本、复选框文字背景（LTEXT、复选框文字）
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkColor(hdcStatic, RGB(239, 244, 249));
        HBRUSH hBgBrush = (HBRUSH)GetPropW(hWnd, L"_DlgBgBrush");
        return (LRESULT)hBgBrush;
    }
    case WM_CLEAR_SUPPRESS:
    {
        RemovePropW(hWnd, L"SuppressSelChange");
        NppSSH_LogInfoAuto("WM_CLEAR_SUPPRESS：清除SuppressSelChange标记");
        return TRUE;
    }
    case WM_MEASUREITEM:
    {
        // 设置下拉列表每一行高度 36像素
        LPMEASUREITEMSTRUCT pMis = (LPMEASUREITEMSTRUCT)lParam;
        if (pMis->CtlID == IDC_HOST)
        {
            pMis->itemHeight = 36;
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT pDis = (LPDRAWITEMSTRUCT)lParam;
        if (pDis->CtlID != IDC_HOST) break;
        // itemID非法直接返回，空列表防止崩溃
        if (pDis->itemID == (UINT)-1 || pDis->itemID >= (UINT)SendMessageW(pDis->hwndItem, CB_GETCOUNT, 0, 0))
        {
            return TRUE;
        }

        RECT rcText = pDis->rcItem;
        RECT rcDelBtn = pDis->rcItem;
        // ✅关键：删除按钮画在rcItem内部，不向外溢出！
        rcDelBtn.left = rcDelBtn.right - 36;
        // rcDelBtn.right += 1; // 删除这一行！禁止向外扩张
        rcText.right = rcDelBtn.left - 4;

        // 背景绘制
        if (pDis->itemState & ODS_SELECTED)
        {
            SetBkColor(pDis->hDC, GetSysColor(COLOR_HIGHLIGHT));
            SetTextColor(pDis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
        }
        else
        {
            SetBkColor(pDis->hDC, GetSysColor(COLOR_WINDOW));
            SetTextColor(pDis->hDC, GetSysColor(COLOR_WINDOWTEXT));
        }
        ExtTextOutW(pDis->hDC, 0, 0, ETO_OPAQUE, &pDis->rcItem, nullptr, 0, nullptr);

        SSHLoginHistoryItem* pItem = (SSHLoginHistoryItem*)SendMessageW(pDis->hwndItem, CB_GETITEMDATA, pDis->itemID, 0);
        if (pItem != nullptr)
        {
            DrawTextW(pDis->hDC, pItem->szHost, -1, &rcText, DT_VCENTER | DT_SINGLELINE | DT_LEFT);
        }

        HPEN hPenOld = (HPEN)SelectObject(pDis->hDC, GetStockObject(BLACK_PEN));
        int oldBkMode = SetBkMode(pDis->hDC, TRANSPARENT);
        MoveToEx(pDis->hDC, rcDelBtn.left + 4, rcDelBtn.top + 4, nullptr);
        LineTo(pDis->hDC, rcDelBtn.right - 4, rcDelBtn.bottom - 4);
        MoveToEx(pDis->hDC, rcDelBtn.right - 4, rcDelBtn.top + 4, nullptr);
        LineTo(pDis->hDC, rcDelBtn.left + 4, rcDelBtn.bottom - 4);

        SelectObject(pDis->hDC, hPenOld);
        SetBkMode(pDis->hDC, oldBkMode);
        return TRUE;
    }

    case WM_INITDIALOG:
    {
        HBRUSH hBgBrush = CreateSolidBrush(RGB(239, 244, 249));
        SetPropW(hWnd, L"_DlgBgBrush", (HANDLE)hBgBrush);
        // lParam是DialogBoxParam传入的this，存入窗口属性
        pPanel = (SSHLoginModal*)lParam;
        wchar_t buf[128]{};
        swprintf_s(buf, _countof(buf), L"SSHLoginModal-%p", hWnd);
        SetPropW(hWnd, buf, (HANDLE)pPanel);

        // 居中在 Notepad++ 主窗口
        CenterWindow(hWnd, g_nppData._nppHandle);
        SetForegroundWindow(hWnd);

        // 初始化默认值
        SetDlgItemTextA(hWnd, IDC_HOST, "");
        SetDlgItemTextA(hWnd, IDC_PORT, "22");
        SetDlgItemTextA(hWnd, IDC_USER, "");
        SetDlgItemTextA(hWnd, IDC_PASS, "");
        SetDlgItemTextA(hWnd, IDC_DIRECTOR, "");
        // 复选框默认不勾选
        SendDlgItemMessageW(hWnd, IDC_CHK_SAVE_HIST, BM_SETCHECK, BST_UNCHECKED, 0);

        // 密码框样式：默认隐藏密码
        HWND hPassEdit = GetDlgItem(hWnd, IDC_PASS);
        SendDlgItemMessage(hWnd, IDC_PASS, EM_SETPASSWORDCHAR, L'•', 0);

        HWND hComboHost = GetDlgItem(hWnd, IDC_HOST);
        COMBOBOXINFO cbi{ 0 };
        cbi.cbSize = sizeof(COMBOBOXINFO);
        GetComboBoxInfo(hComboHost, &cbi);
        if (cbi.hwndList != nullptr)
        {
            if (cbi.hwndItem)
            {
                SendMessageW(cbi.hwndItem, EM_SETCUEBANNER, 0, (LPARAM)L"请输入SSH主机IP/域名");
                PostMessageW(hWnd, WM_SET_EDIT_CURSOR_END, (WPARAM)cbi.hwndItem, 0);// 投递消息，等初始化全部完成后再设置光标
            }
            // 子类挂载给下拉列表 hwndList
            SetPropW(hComboHost, L"_DLG_HWND", (HANDLE)hWnd);
            SetWindowSubclass(cbi.hwndList, ComboDropList_SubclassProc, 1, (DWORD_PTR)hComboHost);
        }

        SendDlgItemMessageW(hWnd, IDC_PORT, EM_SETCUEBANNER, 0, (LPARAM)L"请输入SSH端口");
        SendDlgItemMessageW(hWnd, IDC_USER, EM_SETCUEBANNER, 0, (LPARAM)L"请输入登录用户");
        SendDlgItemMessageW(hWnd, IDC_PASS, EM_SETCUEBANNER, 0, (LPARAM)L"请输入登录密码");
        SendDlgItemMessageW(hWnd, IDC_DIRECTOR, EM_SETCUEBANNER, 0, (LPARAM)L"请输入初始远程目录，默认为空");


        // 加载历史记录填充COMBOBOX
        std::vector<SSHLoginHistoryItem> historyList = SSHLogin_LoadHistoryJson();
        for (const auto& item : historyList)
        {
            int idx = (int)SendMessageW(hComboHost, CB_ADDSTRING, 0, (LPARAM)item.szHost);
            // 把完整item拷贝存入ItemData，注意：不能直接存栈对象指针！存入拷贝到堆
            SSHLoginHistoryItem* pCopy = new SSHLoginHistoryItem();
            *pCopy = item;
            SendMessageW(hComboHost, CB_SETITEMDATA, idx, (LPARAM)pCopy);
        }

        // 加载默认闭眼图标
        HWND hEyeBtn = GetDlgItem(hWnd, IDC_BTN_EYE);
        HICON hEyeHide = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCE(IDI_EYE_HIDE), IMAGE_ICON, 28, 28, LR_DEFAULTCOLOR);
        HICON hEyeShow = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCE(IDI_EYE_SHOW), IMAGE_ICON, 28, 28, LR_DEFAULTCOLOR);
        SendMessageW(hEyeBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hEyeHide);
        // 保存图标句柄到窗口属性，后续切换使用
        SetPropW(hWnd, L"hEyeHide", (HANDLE)hEyeHide);
        SetPropW(hWnd, L"hEyeShow", (HANDLE)hEyeShow);
        SetPropW(hWnd, L"isPasswordHide", (HANDLE)true);// 标记当前密码是否隐藏（默认true隐藏）

        return TRUE;
    }
    case WM_DELETE_COMBO_ITEM:
    {
        HWND hCombo = (HWND)wParam;
        int nIdx = (int)lParam;
        NppSSH_LogInfoAuto("收到WM_DELETE_COMBO_ITEM");
        int totalItems = (int)SendMessageW(hCombo, CB_GETCOUNT, 0, 0);
        // 索引越界直接返回，防止删除最后一条时的非法索引
        if (nIdx < 0 || nIdx >= totalItems)
        {
            SendMessageW(hCombo, CB_SHOWDROPDOWN, FALSE, 0);
            PostMessageW(hWnd, WM_CLEAR_SUPPRESS, 0, 0);
            return TRUE;
        }

        SSHLoginHistoryItem* pHis = (SSHLoginHistoryItem*)SendMessageW(hCombo, CB_GETITEMDATA, nIdx, 0);
        if (pHis != nullptr)
        {
            std::string hostStr = WStringToUTF8(std::wstring(pHis->szHost));
            char delLog[256]{};
            sprintf_s(delLog, "对话框执行删除历史 host=%s", hostStr.c_str());
            NppSSH_LogInfoAuto(delLog);

            SSHLogin_DeleteHistoryByItem(pHis);
            delete pHis;
            SendMessageW(hCombo, CB_SETITEMDATA, nIdx, (LPARAM)0);
            SendMessageW(hCombo, CB_DELETESTRING, (WPARAM)nIdx, 0);

            // 先同步收起下拉，让控件记录条目变更
            SendMessageW(hCombo, CB_SHOWDROPDOWN, FALSE, 0);
            // 异步重新展开，此时ComboBox会使用新条目数量计算下拉窗口高度，消除空白
            PostMessageW(hCombo, CB_SHOWDROPDOWN, TRUE, 0);
        }
        PostMessageW(hWnd, WM_CLEAR_SUPPRESS, 0, 0);
        return TRUE;
    }
    case WM_SET_EDIT_CURSOR_END:
    {
        HWND hEdit = (HWND)wParam;
        int len = (int)SendMessageW(hEdit, WM_GETTEXTLENGTH, 0, 0);
        SendMessageW(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        return TRUE;
    }
    case WM_COMMAND:
    {
        WORD wNotifyCode = HIWORD(wParam);
        WORD wId = LOWORD(wParam);
        // ========== COMBOBOX 即将展开下拉 CBN_DROPDOWN ==========
        if (wId == IDC_HOST && wNotifyCode == CBN_DROPDOWN)
        {
            // 什么都不做！禁止重建下拉列表，保护编辑框光标/选中状态
            return FALSE;
        }
        if (wId == IDCANCEL)////取消连接，无论面板什么状态直接断开
        {
            // 取消连接时重置状态
            NppSSH_LogInfoAuto("用户取消连接");
            EndDialog(hWnd, IDCANCEL); // 右上角关闭
        }
        // ========= COMBOBOX下拉选择变更 CBN_SELCHANGE =========
        else if (wId == IDC_HOST && wNotifyCode == CBN_SELCHANGE)
        {
            // 如果是点击删除按钮触发，直接丢弃通知，重置标记
            HANDLE hSuppress = GetPropW(hWnd, L"SuppressSelChange");
            if (hSuppress != nullptr)
            {
                RemovePropW(hWnd, L"SuppressSelChange");
                NppSSH_LogInfoAuto("CBN_SELCHANGE：被删除按钮抑制，跳过");
                return TRUE;
            }
            HWND hComboHost = GetDlgItem(hWnd, IDC_HOST);
            int selIdx = (int)SendMessageW(hComboHost, CB_GETCURSEL, 0, 0);
            if (selIdx == CB_ERR)
            {
                NppSSH_LogInfoAuto("CBN_SELCHANGE: selIdx == CB_ERR，直接返回");
                return TRUE;
            }

            SSHLoginHistoryItem* pHis = (SSHLoginHistoryItem*)SendMessageW(hComboHost, CB_GETITEMDATA, selIdx, 0);
            if (pHis)
            {
                SetDlgItemTextW(hWnd, IDC_PORT, pHis->szPort);
                SetDlgItemTextW(hWnd, IDC_USER, pHis->szUser);
                SetDlgItemTextW(hWnd, IDC_DIRECTOR, pHis->szDir);
                std::wstring plainPwd;
                SSH_DecryptPasswordFromBase64(pHis->szPassEncBase64, plainPwd);
                SetDlgItemTextW(hWnd, IDC_PASS, plainPwd.c_str());
                NppSSH_LogInfoAuto("用户选择历史记录，自动填充密码框");
                // 异步投递，等控件原生全选逻辑执行完毕再设置光标
                COMBOBOXINFO cbiSel{};
                cbiSel.cbSize = sizeof(COMBOBOXINFO);
                GetComboBoxInfo(hComboHost, &cbiSel);
                if (cbiSel.hwndItem)
                {
                    PostMessageW(hWnd, WM_SET_EDIT_CURSOR_END, (WPARAM)cbiSel.hwndItem, 0);
                }
            }
            
        }
        // ========== 眼睛按钮点击处理 ==========
        else if (wId == IDC_BTN_EYE)
        {
            HWND hPassEdit = GetDlgItem(hWnd, IDC_PASS);
            HWND hEyeBtn = GetDlgItem(hWnd, IDC_BTN_EYE);
            // 读取当前状态
            BOOL bHide = (BOOL)GetPropW(hWnd, L"isPasswordHide");
            HICON hHide = (HICON)GetPropW(hWnd, L"hEyeHide");
            HICON hShow = (HICON)GetPropW(hWnd, L"hEyeShow");

            if (bHide)
            {
                // 当前隐藏 → 切换明文，取消掩码
                SendDlgItemMessage(hWnd, IDC_PASS, EM_SETPASSWORDCHAR, 0, 0);
                SendMessageW(hEyeBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hShow);
                SetPropW(hWnd, L"isPasswordHide", (HANDLE)0);
            }
            else
            {
                // 当前明文 → 切换掩码隐藏
                SendDlgItemMessage(hWnd, IDC_PASS, EM_SETPASSWORDCHAR, L'•', 0);
                SendMessageW(hEyeBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hHide);
                SetPropW(hWnd, L"isPasswordHide", (HANDLE)1);
            }
            // 强制重绘密码输入框
            InvalidateRect(hPassEdit, nullptr, TRUE);
            UpdateWindow(hPassEdit);

            InvalidateRect(hEyeBtn, nullptr, TRUE);
            return TRUE;
        }
        else {
            if (LOWORD(wParam) == IDC_BTN_SUBMIT)//确认登录按钮
            {
                NppSSH_LogInfoAuto("用户点击确认登录按钮，开始调用");
                wchar_t szHost[256] = { 0 };
                wchar_t szPort[32] = { 0 };
                wchar_t szUser[256] = { 0 };
                wchar_t szPass[256] = { 0 };
                wchar_t szDir[256] = { 0 };
                GetDlgItemTextW(hWnd, IDC_HOST, szHost, _countof(szHost));
                GetDlgItemTextW(hWnd, IDC_PORT, szPort, _countof(szPort));
                GetDlgItemTextW(hWnd, IDC_USER, szUser, _countof(szUser));
                GetDlgItemTextW(hWnd, IDC_PASS, szPass, _countof(szPass));
                GetDlgItemTextW(hWnd, IDC_DIRECTOR, szDir, _countof(szDir));
                
                std::string hostU8 = WStringToUTF8(szHost);
                std::string portU8 = WStringToUTF8(szPort);
                std::string userU8 = WStringToUTF8(szUser);
                std::string passU8 = WStringToUTF8(szPass);
                std::string dirU8 = WStringToUTF8(szDir);
                
                bool bValid = isEmptyInputToSSHLoginModal(pPanel,
                    hostU8.c_str(), portU8.c_str(), userU8.c_str(), passU8.c_str(), dirU8.c_str());
                if(bValid){
                    // 校验成功，判断复选框是否勾选
                    UINT chkState = (UINT)SendDlgItemMessageW(hWnd, IDC_CHK_SAVE_HIST, BM_GETCHECK, 0, 0);
                    if (chkState == BST_CHECKED)
                    {
                        SSHLoginHistoryItem item{};
                        wcsncpy_s(item.szHost, _countof(item.szHost), szHost, _TRUNCATE);
                        wcsncpy_s(item.szPort, _countof(item.szPort), szPort, _TRUNCATE);
                        wcsncpy_s(item.szUser, _countof(item.szUser), szUser, _TRUNCATE);
                        wcsncpy_s(item.szDir, _countof(item.szDir), szDir, _TRUNCATE);
                        std::wstring encBase64;
                        SSH_EncryptPasswordToBase64(szPass, encBase64);
                        wcsncpy_s(item.szPassEncBase64, _countof(item.szPassEncBase64), encBase64.c_str(), _TRUNCATE);
                        SSHLogin_SaveHistoryJson(item);

                    }
                    EndDialog(hWnd, IDOK);
                }
            }
            else if (LOWORD(wParam) == IDC_BTN_CONNECT) {
                EndDialog(hWnd, IDCANCEL);
                NppSSH_LogErrorAuto("点击取消按钮");
            }
        }
        return TRUE;
    }
    //对话框销毁后的所有操作
    case WM_DESTROY:
        wchar_t buf[128]{};
        swprintf_s(buf, _countof(buf), L"SSHLoginModal-%p", hWnd);
        RemovePropW(hWnd, buf);
        RemovePropW(hWnd, L"SuppressSelChange");

        // 释放背景画刷资源
        HBRUSH hBgBrush = (HBRUSH)GetPropW(hWnd, L"_DlgBgBrush");
        if (hBgBrush != nullptr)
        {
            DeleteObject(hBgBrush);
            RemovePropW(hWnd, L"_DlgBgBrush");
        }

        // 释放COMBOBOX堆上分配的ItemData内存，防止内存泄漏
        HWND hComboHost = GetDlgItem(hWnd, IDC_HOST);
        COMBOBOXINFO cbi{ 0 };
        cbi.cbSize = sizeof(COMBOBOXINFO);
        GetComboBoxInfo(hComboHost, &cbi);
        if (cbi.hwndList != nullptr)
        {
            RemoveWindowSubclass(cbi.hwndList, ComboDropList_SubclassProc, 1);
        }
        // 清理combo上挂的对话框句柄属性
        RemovePropW(hComboHost, L"_DLG_HWND");
        
        int count = (int)SendMessageW(hComboHost, CB_GETCOUNT, 0, 0);
        for (int i = 0;i < count;i++)
        {
            SSHLoginHistoryItem* p = (SSHLoginHistoryItem*)SendMessageW(hComboHost, CB_GETITEMDATA, i, 0);
            if (p) delete p;
        }

        if (pPanel)
        {
            // 释放眼睛图标资源
            HICON hHide = (HICON)GetPropW(hWnd, L"hEyeHide");
            HICON hShow = (HICON)GetPropW(hWnd, L"hEyeShow");
            if (hHide) DestroyIcon(hHide);
            if (hShow) DestroyIcon(hShow);
            RemovePropW(hWnd, L"hEyeHide");
            RemovePropW(hWnd, L"hEyeShow");
            RemovePropW(hWnd, L"isPasswordHide");
        }

        NppSSH_LogInfoAuto("登录对话框销毁");
        return TRUE;
    }

    return FALSE;
}
bool isEmptyInputToSSHLoginModal(SSHLoginModal* loginPanel,const char* host, const char* port, const char* user, const char* pass, const char* director) {

    std::wstring SSH_HOST = UTF8ToWstring(host);
    std::wstring SSH_PORT = UTF8ToWstring(port);
    std::wstring SSH_USER = UTF8ToWstring(user);
    std::wstring SSH_PASS = UTF8ToWstring(pass);
    std::wstring SSH_INITCD = UTF8ToWstring(director);

    if (SSH_HOST.empty()) {
        ::MessageBoxW(NULL, L"主机不能为空！", L"NppSSH 提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：主机不能为空");
        return false;
    }
    // 主机不能包含空格
    if (SSH_HOST.find(L' ') != std::wstring::npos)
    {
        ::MessageBoxW(NULL, L"主机不能包含空格！", L"NppSSH 提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：主机包含空格");
        return false;
    }
    // 禁止带协议头 ssh:// http:// https://
    if ((SSH_HOST.substr(0, 7) == L"ssh://") ||
        (SSH_HOST.substr(0, 7) == L"http://") ||
        (SSH_HOST.substr(0, 8) == L"https://"))
    {
        ::MessageBoxW(NULL, L"主机请勿填写协议头，仅填写域名或IP地址", L"NppSSH 提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：主机携带协议头");
        return false;
    }
    // 合法地址格式校验
    bool hostOk = false;
    bool allNumberSeg = false;
    bool isIpv4 = IsValidIPv4(SSH_HOST, allNumberSeg);

    if (isIpv4)
    {
        hostOk = true;
    }
    else if (allNumberSeg)
    {
        // 3个点+四段全数字，但是数值越界，属于IP格式错误，不降级域名
        hostOk = false;
    }
    else if (IsValidIPv6(SSH_HOST))
    {
        hostOk = true;
    }
    else
    {
        hostOk = IsHostNameLoose(SSH_HOST);
    }

    if (!hostOk)
    {
        ::MessageBoxW(NULL, L"主机地址格式非法，请输入合法IP或者域名", L"NppSSH 提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto(std::string("启动PuTTY失败：主机格式非法,host=") + std::string(host ? host : ""));
        return false;
    }

    wcsncpy_s(loginPanel->szHost, _countof(loginPanel->szHost), SSH_HOST.c_str(), _TRUNCATE);

    if (SSH_PORT.empty()) {
        ::MessageBoxW(NULL, L"端口不能为空！", L"NppSSH 提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：端口不能为空");
        return false;
    }
    // 端口必须全部是数字
    bool allDigit = true;
    for (wchar_t ch : SSH_PORT)
    {
        if (!iswdigit(static_cast<wint_t>(ch)))
        {
            allDigit = false;
            break;
        }
    }
    if (!allDigit)
    {
        ::MessageBoxW(NULL, L"端口必须为纯数字！", L"NppSSH 提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：端口非数字");
        return false;
    }
    // 转数字校验范围 1‑65535
    unsigned long portNum = std::wcstoul(SSH_PORT.c_str(), nullptr, 10);
    if (portNum < 1 || portNum > 65535)
    {
        ::MessageBoxW(NULL, L"端口范围必须在 1 ~ 65535 之间", L"NppSSH 提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto(std::string("启动PuTTY失败：端口越界,port=") + std::to_string(portNum));
        return false;
    }
    wcsncpy_s(loginPanel->szPort, _countof(loginPanel->szPort), SSH_PORT.c_str(), _TRUNCATE);

    if (SSH_USER.empty()) {
        ::MessageBoxW(NULL, L"用户名不能为空！", L"NppSSH 提示", MB_OK | MB_ICONERROR);
        NppSSH_LogErrorAuto("启动PuTTY失败：用户名不能为空");
        return false;
    }
    wcsncpy_s(loginPanel->szUser, _countof(loginPanel->szUser), SSH_USER.c_str(), _TRUNCATE);

    wcsncpy_s(loginPanel->szPass, _countof(loginPanel->szPass), SSH_PASS.c_str(), _TRUNCATE);
    wcsncpy_s(loginPanel->szDir, _countof(loginPanel->szDir), SSH_INITCD.c_str(), _TRUNCATE);
    return true;
}