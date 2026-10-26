#pragma once

void UpdateTrayTip() {
    g_tray.uFlags = NIF_TIP;
    lstrcpynW(g_tray.szTip, g_paused ? L"桌面双击隐藏（已暂停）· 单击打开设置" :
              L"桌面双击隐藏 · 单击打开设置，右键菜单", ARRAYSIZE(g_tray.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
}
void RefreshMemory(HWND dlg) {
    PROCESS_MEMORY_COUNTERS_EX info = {}; info.cb = sizeof(info);
    wchar_t text[240] = {};
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&info, sizeof(info))) {
        StringCchPrintfW(text, ARRAYSIZE(text),
            L"当前驻留：%.2f MiB    私有提交：%.2f MiB\n5 MiB 驻留限制：%s",
            info.WorkingSetSize / 1048576.0, info.PrivateUsage / 1048576.0,
            g_limitActive ? L"已启用" : (g_limitError ? L"启用失败" : L"未启用"));
    } else StringCchCopyW(text, ARRAYSIZE(text), L"无法读取内存数据");
    SetDlgItemTextW(dlg, IDC_MEMORY, text);
}
void FillSettings(HWND dlg, const Config& c, bool autostart) {
    CheckDlgButton(dlg, IDC_ENABLED, c.enabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_AUTOSTART, autostart ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_RESTORE, c.restore ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_NOTIFY, c.notify ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_STARTSETTINGS, c.startSettings ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_LIMITMEM, c.limitMemory ? BST_CHECKED : BST_UNCHECKED);
    const DWORD values[] = {0, 200, 350, 500, 700};
    for (unsigned i = 0; i < ARRAYSIZE(values); ++i)
        if (values[i] == c.interval) SendDlgItemMessageW(dlg, IDC_SPEED, CB_SETCURSEL, i, 0);
}
bool RestartAsAdmin(HWND dlg) {
    wchar_t path[32768] = {}, args[80] = {};
    ExePath(path);
    StringCchPrintfW(args, ARRAYSIZE(args), L"--restart %lu", GetCurrentProcessId());
    SHELLEXECUTEINFOW info = {}; info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.hwnd = dlg; info.lpVerb = L"runas"; info.lpFile = path;
    info.lpParameters = args; info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        DWORD error = GetLastError();
        if (error != ERROR_CANCELLED) ShowSystemError(dlg, L"无法启动管理员模式。", error);
        return false;
    }
    if (info.hProcess) CloseHandle(info.hProcess);
    g_restarting = true;
    PostMessageW(g_window, WM_CLOSE, 0, 0);
    return true;
}
INT_PTR CALLBACK SettingsProc(HWND dlg, UINT message, WPARAM wparam, LPARAM) {
    switch (message) {
    case WM_INITDIALOG: {
        HICON icon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                      IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
        SendMessageW(dlg, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        const wchar_t* options[] = {L"跟随 Windows 系统设置", L"200 毫秒（较快）",
            L"350 毫秒", L"500 毫秒", L"700 毫秒（较宽松）"};
        for (auto option : options) SendDlgItemMessageW(dlg, IDC_SPEED, CB_ADDSTRING, 0, (LPARAM)option);
        Config c = g_config; c.enabled = !g_paused;
        FillSettings(dlg, c, AutoStartEnabled());
        bool admin = Elevated();
        SetDlgItemTextW(dlg, IDC_ADMINSTATE, admin ? L"当前权限：管理员（仅本次）" : L"当前权限：普通用户");
        EnableWindow(GetDlgItem(dlg, IDC_ADMIN), !admin);
        SetDlgItemTextW(dlg, IDC_STATUS, L"修改后点击“保存”生效；关闭窗口后继续在托盘运行。");
        RefreshMemory(dlg);
        // Center on the monitor where the pointer is, including negative coordinates.
        POINT p; GetCursorPos(&p);
        MONITORINFO mi = {}; mi.cbSize = sizeof(mi);
        RECT r; GetWindowRect(dlg, &r);
        if (GetMonitorInfoW(MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST), &mi))
            SetWindowPos(dlg, nullptr, mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - (r.right-r.left))/2,
                mi.rcWork.top + ((mi.rcWork.bottom-mi.rcWork.top) - (r.bottom-r.top))/2,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case IDOK: {
            Config c = g_config;
            c.enabled = IsDlgButtonChecked(dlg, IDC_ENABLED) == BST_CHECKED;
            c.restore = IsDlgButtonChecked(dlg, IDC_RESTORE) == BST_CHECKED;
            c.notify = IsDlgButtonChecked(dlg, IDC_NOTIFY) == BST_CHECKED;
            c.startSettings = IsDlgButtonChecked(dlg, IDC_STARTSETTINGS) == BST_CHECKED;
            c.limitMemory = IsDlgButtonChecked(dlg, IDC_LIMITMEM) == BST_CHECKED;
            const DWORD values[] = {0, 200, 350, 500, 700};
            LRESULT index = SendDlgItemMessageW(dlg, IDC_SPEED, CB_GETCURSEL, 0, 0);
            c.interval = index >= 0 && index < 5 ? values[index] : 0;
            bool autostart = IsDlgButtonChecked(dlg, IDC_AUTOSTART) == BST_CHECKED;
            bool oldAuto = AutoStartEnabled();
            bool oldLimit = g_config.limitMemory != 0;
            if (!ApplyMemoryLimit(c.limitMemory != 0)) {
                ShowSystemError(dlg, L"内存限制修改失败，设置未保存。", g_limitError);
                RefreshMemory(dlg); return TRUE;
            }
            LSTATUS r = SetAutoStart(autostart);
            if (r != ERROR_SUCCESS) {
                ApplyMemoryLimit(oldLimit);
                ShowSystemError(dlg, L"开机自启修改失败，设置未保存。", r); return TRUE;
            }
            r = WriteConfig(c);
            if (r != ERROR_SUCCESS) {
                LSTATUS rollback = SetAutoStart(oldAuto);
                ApplyMemoryLimit(oldLimit);
                ShowSystemError(dlg, L"设置保存失败。", r);
                if (rollback != ERROR_SUCCESS)
                    ShowSystemError(dlg, L"开机自启回退失败，请重新检查此选项。", rollback);
                return TRUE;
            }
            g_config = c; g_haveConfig = true;
            g_paused = !c.enabled; g_matcher.reset(); UpdateTrayTip();
            SetDlgItemTextW(dlg, IDC_STATUS, L"设置已保存。");
            RefreshMemory(dlg); return TRUE;
        }
        case IDCANCEL: DestroyWindow(dlg); return TRUE;
        case IDC_REFRESH: RefreshMemory(dlg); return TRUE;
        case IDC_DEFAULTS:
            FillSettings(dlg, Defaults(), false);
            SetDlgItemTextW(dlg, IDC_STATUS, L"已填入默认值；点击“保存”后生效。"); return TRUE;
        case IDC_ADMIN:
            if (MessageBoxW(dlg, L"将以管理员权限重启程序。\n请先保存已修改的设置；此次提升不改变开机自启权限。\n是否继续？",
                            kTitle, MB_OKCANCEL | MB_ICONINFORMATION) == IDOK) RestartAsAdmin(dlg);
            return TRUE;
        case IDC_FOLDER: {
            wchar_t path[32768] = {}; ExePath(path);
            wchar_t* slash = wcsrchr(path, L'\\'); if (slash) *slash = 0;
            ShellExecuteW(dlg, L"open", path, nullptr, nullptr, SW_SHOWNORMAL); return TRUE;
        }
        } break;
    case WM_CLOSE: DestroyWindow(dlg); return TRUE;
    case WM_DESTROY: g_settings = nullptr; CoFreeUnusedLibraries(); return TRUE;
    }
    return FALSE;
}
void OpenSettings() {
    if (!g_settings) g_settings = CreateDialogParamW(GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, SettingsProc, 0);
    if (g_settings) { ShowWindow(g_settings, SW_SHOWNORMAL); SetForegroundWindow(g_settings); }
    else ShowSystemError(g_window, L"无法打开设置窗口。", GetLastError());
}
