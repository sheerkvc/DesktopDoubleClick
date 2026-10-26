#pragma once
#include <psapi.h>
#include <strsafe.h>
#include "resource.h"

const wchar_t* kConfigKey = L"Software\\DesktopDoubleClick";
const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* kRunValue = L"DesktopDoubleClick";
struct Config {
    DWORD version, enabled, restore, notify, startSettings, limitMemory, interval;
};
Config Defaults() { return {2, 1, 1, 0, 1, 1, 0}; }
Config g_config = Defaults();
bool g_haveConfig = false, g_restarting = false, g_limitActive = false;
DWORD g_limitError = 0;
HWND g_settings = nullptr;

void ExePath(wchar_t (&path)[32768]) {
    GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
}
void ReadConfig() {
    DWORD size = sizeof(Config), type = 0;
    Config c = {};
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kConfigKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return;
    LSTATUS r = RegQueryValueExW(key, L"SettingsV2", nullptr, &type, (BYTE*)&c, &size);
    RegCloseKey(key);
    if (r == ERROR_SUCCESS && type == REG_BINARY && size == sizeof(c) && c.version == 2 &&
        c.enabled <= 1 && c.restore <= 1 && c.notify <= 1 && c.startSettings <= 1 &&
        c.limitMemory <= 1 && (c.interval == 0 || c.interval == 200 || c.interval == 350 ||
                             c.interval == 500 || c.interval == 700)) {
        g_config = c; g_haveConfig = true;
    }
}
LSTATUS WriteConfig(const Config& c) {
    HKEY key = nullptr;
    LSTATUS r = RegCreateKeyExW(HKEY_CURRENT_USER, kConfigKey, 0, nullptr, 0,
                               KEY_SET_VALUE, nullptr, &key, nullptr);
    if (r != ERROR_SUCCESS) return r;
    r = RegSetValueExW(key, L"SettingsV2", 0, REG_BINARY, (const BYTE*)&c, sizeof(c));
    RegCloseKey(key); return r;
}
bool AutoStartEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    DWORD size = 0, type = 0;
    LSTATUS r = RegQueryValueExW(key, kRunValue, nullptr, &type, nullptr, &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && type == REG_SZ && size > sizeof(wchar_t);
}
LSTATUS SetAutoStart(bool enabled) {
    HKEY key = nullptr;
    LSTATUS r = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                               KEY_SET_VALUE, nullptr, &key, nullptr);
    if (r != ERROR_SUCCESS) return r;
    if (enabled) {
        wchar_t path[32768] = {}, command[32784] = {};
        ExePath(path);
        StringCchPrintfW(command, ARRAYSIZE(command), L"\"%s\" --background", path);
        if (lstrlenW(command) > 260) r = ERROR_FILENAME_EXCED_RANGE;
        else r = RegSetValueExW(key, kRunValue, 0, REG_SZ, (const BYTE*)command,
                               (lstrlenW(command) + 1) * sizeof(wchar_t));
    } else {
        r = RegDeleteValueW(key, kRunValue);
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;
    }
    RegCloseKey(key); return r;
}
bool Elevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION e = {}; DWORD size = 0;
    BOOL ok = GetTokenInformation(token, TokenElevation, &e, sizeof(e), &size);
    CloseHandle(token); return ok && e.TokenIsElevated;
}
bool ApplyMemoryLimit(bool enable) {
    // A real Windows working-set limit, never a timer that repeatedly empties it.
    DWORD flags = QUOTA_LIMITS_HARDWS_MIN_DISABLE |
        (enable ? QUOTA_LIMITS_HARDWS_MAX_ENABLE : QUOTA_LIMITS_HARDWS_MAX_DISABLE);
    BOOL ok = SetProcessWorkingSetSizeEx(GetCurrentProcess(), 64 * 1024,
                                         enable ? 5 * 1024 * 1024 : 64 * 1024 * 1024, flags);
    g_limitError = ok ? 0 : GetLastError();
    if (ok) g_limitActive = enable;
    return ok != FALSE;
}
void ShowSystemError(HWND owner, const wchar_t* action, DWORD error) {
    wchar_t detail[256] = {}, message[512] = {};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                    error, 0, detail, ARRAYSIZE(detail), nullptr);
    StringCchPrintfW(message, ARRAYSIZE(message), L"%s\n错误 %lu：%s", action, error, detail);
    MessageBoxW(owner, message, L"桌面双击隐藏", MB_OK | MB_ICONWARNING);
}
