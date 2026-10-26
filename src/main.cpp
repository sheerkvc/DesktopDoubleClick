#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <exdisp.h>
#include <servprov.h>
#include <oleacc.h>
#include "click_matcher.h"
#include "app_config.h"

// Native Win32 application: no .NET, Python, downloads, telemetry or polling timer.
namespace {
const wchar_t* kTitle = L"桌面双击隐藏";
const wchar_t* kClass = L"DesktopDoubleClick.Window.v1";
constexpr UINT kTray = WM_APP + 1, kClick = WM_APP + 2, kReset = WM_APP + 3;
constexpr UINT kPause = 101, kShow = 102, kHide = 103, kExit = 104, kSettings = 105;
HWND g_window = nullptr;
HANDLE g_hookReady = nullptr, g_hookThread = nullptr;
DWORD g_hookThreadId = 0, g_hookError = 0;
UINT g_taskbarCreated = 0;
HHOOK g_hook = nullptr;
bool g_paused = false;
ClickMatcher g_matcher;
NOTIFYICONDATAW g_tray = {};

template<class T> struct ComPtr {
    T* p = nullptr;
    ~ComPtr() { if (p) p->Release(); }
    T** out() { return &p; }
    T* operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
};

HRESULT DesktopView(IFolderView2** result) {
    *result = nullptr;
    ComPtr<IShellWindows> windows;
    HRESULT hr = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER,
                                  IID_IShellWindows, (void**)windows.out());
    if (FAILED(hr)) return hr;
    VARIANT location, empty;
    VariantInit(&location); VariantInit(&empty);
    location.vt = VT_I4; location.lVal = CSIDL_DESKTOP;
    long hwnd = 0;
    ComPtr<IDispatch> dispatch;
    hr = windows->FindWindowSW(&location, &empty, SWC_DESKTOP, &hwnd,
                               SWFO_NEEDDISPATCH, dispatch.out());
    if (FAILED(hr) || !dispatch) return FAILED(hr) ? hr : E_FAIL;
    ComPtr<IServiceProvider> provider;
    hr = dispatch->QueryInterface(IID_IServiceProvider, (void**)provider.out());
    if (FAILED(hr)) return hr;
    ComPtr<IShellBrowser> browser;
    hr = provider->QueryService(SID_STopLevelBrowser, IID_IShellBrowser,
                                 (void**)browser.out());
    if (FAILED(hr)) return hr;
    ComPtr<IShellView> view;
    hr = browser->QueryActiveShellView(view.out());
    if (FAILED(hr)) return hr;
    return view->QueryInterface(IID_IFolderView2, (void**)result);
}

// mode: -1 toggles; 0 shows; 1 hides. Reconnect on every operation so Explorer
// restarts and virtual desktop changes do not leave a stale COM view cached.
HRESULT SetIcons(int mode) {
    ComPtr<IFolderView2> view;
    HRESULT hr = DesktopView(view.out());
    if (FAILED(hr)) return hr;
    DWORD flags = 0;
    hr = view->GetCurrentFolderFlags(&flags);
    if (FAILED(hr)) return hr;
    bool hide = mode < 0 ? !(flags & FWF_NOICONS) : mode != 0;
    return view->SetCurrentFolderFlags(FWF_NOICONS, hide ? FWF_NOICONS : FWF_NONE);
}

void Notify(const wchar_t* message, DWORD kind = NIIF_INFO) {
    NOTIFYICONDATAW data = g_tray;
    data.uFlags = NIF_INFO;
    lstrcpynW(data.szInfoTitle, kTitle, ARRAYSIZE(data.szInfoTitle));
    lstrcpynW(data.szInfo, message, ARRAYSIZE(data.szInfo));
    data.dwInfoFlags = kind;
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void ChangeIcons(int mode) {
    if (FAILED(SetIcons(mode)))
        Notify(L"暂时无法连接 Windows 桌面。请等待资源管理器恢复后重试。", NIIF_WARNING);
}

void AddTray() {
    g_tray = {};
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_window; g_tray.uID = 1;
    g_tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    g_tray.uCallbackMessage = kTray;
    g_tray.hIcon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                   IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (!g_tray.hIcon) g_tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(g_tray.szTip, g_paused ? L"桌面双击隐藏（已暂停）" :
              L"桌面双击隐藏 · 单击打开设置，右键菜单", ARRAYSIZE(g_tray.szTip));
    if (Shell_NotifyIconW(NIM_ADD, &g_tray)) {
        g_tray.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &g_tray);
    }
}

bool ClassIs(HWND hwnd, const wchar_t* name) {
    wchar_t cls[80] = {};
    return GetClassNameW(hwnd, cls, ARRAYSIZE(cls)) && lstrcmpW(cls, name) == 0;
}

// Strictly limit gestures to Explorer's desktop, never an Explorer folder,
// taskbar, application window or desktop icon / icon label.
bool BlankDesktop(POINT point, HWND* surface) {
    HWND hit = WindowFromPoint(point);
    if (!hit) return false;
    HWND root = GetAncestor(hit, GA_ROOT);
    if (!ClassIs(root, L"Progman") && !ClassIs(root, L"WorkerW")) return false;
    DWORD shellPid = 0, hitPid = 0;
    GetWindowThreadProcessId(GetShellWindow(), &shellPid);
    GetWindowThreadProcessId(root, &hitPid);
    if (!shellPid || hitPid != shellPid) return false;
    *surface = root;
    if (ClassIs(hit, L"SysListView32")) {
        ComPtr<IAccessible> accessible;
        if (FAILED(AccessibleObjectFromWindow(hit, OBJID_CLIENT, IID_IAccessible,
                                              (void**)accessible.out()))) return false;
        VARIANT child;
        VariantInit(&child);
        HRESULT hr = accessible->accHitTest(point.x, point.y, &child);
        const bool blank = SUCCEEDED(hr) && child.vt == VT_I4 && child.lVal == CHILDID_SELF;
        VariantClear(&child);
        return blank;
    }
    return hit == root || ClassIs(hit, L"SHELLDLL_DefView");
}

struct Click {
    POINT down, up;
    DWORD started, ended;
    bool dragged, modified;
};
bool g_pressed = false;
Click g_pending = {};

bool ModifierDown() {
    return (GetAsyncKeyState(VK_SHIFT) | GetAsyncKeyState(VK_CONTROL) |
            GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_LWIN) |
            GetAsyncKeyState(VK_RWIN)) & 0x8000;
}

void TrackDrag(POINT p) {
    if (abs(p.x - g_pending.down.x) > GetSystemMetrics(SM_CXDRAG) / 2 ||
        abs(p.y - g_pending.down.y) > GetSystemMetrics(SM_CYDRAG) / 2)
        g_pending.dragged = true;
}

// This runs on a dedicated message-loop thread. Never call COM, accessibility,
// disk, UI dialogs or desktop shell operations inside the low-level hook.
LRESULT CALLBACK MouseHook(int code, WPARAM message, LPARAM param) {
    if (code == HC_ACTION) {
        const auto* mouse = reinterpret_cast<const MSLLHOOKSTRUCT*>(param);
        if (!(mouse->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED))) {
            if (message == WM_LBUTTONDOWN) {
                g_pending = {mouse->pt, mouse->pt, mouse->time, mouse->time,
                             false, ModifierDown()};
                g_pressed = true;
            } else if (message == WM_MOUSEMOVE && g_pressed) {
                TrackDrag(mouse->pt);
            } else if (message == WM_LBUTTONUP && g_pressed) {
                TrackDrag(mouse->pt);
                g_pending.up = mouse->pt; g_pending.ended = mouse->time;
                g_pending.modified = g_pending.modified || ModifierDown();
                g_pressed = false;
                Click* click = (Click*)HeapAlloc(GetProcessHeap(), 0, sizeof(Click));
                if (click) {
                    *click = g_pending;
                    if (!PostMessageW(g_window, kClick, 0, (LPARAM)click))
                        HeapFree(GetProcessHeap(), 0, click);
                }
            } else if (message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN ||
                       message == WM_XBUTTONDOWN || message == WM_MOUSEWHEEL ||
                       message == WM_MOUSEHWHEEL) {
                g_pressed = false;
                PostMessageW(g_window, kReset, 0, 0);
            }
        }
    }
    return CallNextHookEx(g_hook, code, message, param);
}

DWORD WINAPI HookThread(void*) {
    MSG message;
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    g_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseHook, GetModuleHandleW(nullptr), 0);
    if (!g_hook) g_hookError = GetLastError();
    SetEvent(g_hookReady);
    if (!g_hook) return 1;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    UnhookWindowsHookEx(g_hook); g_hook = nullptr;
    return 0;
}

void HandleClick(const Click& click) {
    if (g_paused) { g_matcher.reset(); return; }
    HWND a = nullptr, b = nullptr;
    const unsigned interval = g_config.interval ? g_config.interval : GetDoubleClickTime();
    const bool eligible = !click.dragged && !click.modified &&
        DWORD(click.ended - click.started) <= interval &&
        BlankDesktop(click.down, &a) && BlankDesktop(click.up, &b) && a == b;
    if (g_matcher.accept(click.down.x, click.down.y, click.started,
                         reinterpret_cast<std::uintptr_t>(a), eligible, interval,
                         GetSystemMetrics(SM_CXDOUBLECLK), GetSystemMetrics(SM_CYDOUBLECLK)))
        ChangeIcons(-1);
}

#include "settings_ui.h"

void TrayMenu() {
    g_matcher.reset();
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, kSettings, L"设置…");
    AppendMenuW(menu, MF_STRING | (g_paused ? MF_CHECKED : 0), kPause,
                g_paused ? L"恢复双击功能" : L"暂停双击功能");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kShow, L"显示桌面图标");
    AppendMenuW(menu, MF_STRING, kHide, L"隐藏桌面图标");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kExit, g_config.restore ? L"退出并显示图标" : L"退出（保留图标状态）");
    POINT p; GetCursorPos(&p);
    SetForegroundWindow(g_window);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                  p.x, p.y, 0, g_window, nullptr);
    DestroyMenu(menu);
    PostMessageW(g_window, WM_NULL, 0, 0);
    if (command) SendMessageW(g_window, WM_COMMAND, command, 0);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == g_taskbarCreated && g_taskbarCreated) { AddTray(); return 0; }
    switch (message) {
    case kClick: {
        Click* click = reinterpret_cast<Click*>(lparam);
        if (click) { HandleClick(*click); HeapFree(GetProcessHeap(), 0, click); }
        return 0;
    }
    case kReset: g_matcher.reset(); return 0;
    case kTray:
        if (LOWORD(lparam) == WM_CONTEXTMENU) TrayMenu();
        else if (LOWORD(lparam) == NIN_SELECT || LOWORD(lparam) == NIN_KEYSELECT) OpenSettings();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case kSettings: OpenSettings(); break;
        case kPause:
            g_paused = !g_paused; g_matcher.reset();
            UpdateTrayTip();
            break;
        case kShow: ChangeIcons(0); break;
        case kHide: ChangeIcons(1); break;
        case kExit: SendMessageW(window, WM_CLOSE, 0, 0); break;
        }
        return 0;
    case WM_CLOSE:
        if (g_config.restore && !g_restarting) SetIcons(0);
        if (g_settings) DestroyWindow(g_settings);
        DestroyWindow(window); return 0;
    case WM_QUERYENDSESSION: return TRUE;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_tray);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    // A successful runas launch hands over only after the original process exits.
    if (wcsncmp(commandLine, L"--restart ", 10) == 0) {
        DWORD pid = wcstoul(commandLine + 10, nullptr, 10);
        HANDLE previous = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
        if (previous) {
            DWORD result = WaitForSingleObject(previous, 10000);
            CloseHandle(previous);
            if (result != WAIT_OBJECT_0) return 2;
        }
    }
    ReadConfig();
    ApplyMemoryLimit(g_config.limitMemory != 0);
    g_paused = !g_config.enabled;
    HANDLE singleton = CreateMutexW(nullptr, FALSE, L"Local\\DesktopDoubleClick.Singleton.v1");
    if (!singleton) {
        ShowSystemError(nullptr, L"无法创建程序单实例标记。若已在管理员模式运行，请从托盘打开设置。", GetLastError());
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(kClass, kTitle);
        if (existing && wcscmp(commandLine, L"--background") != 0) {
            DWORD pid = 0; GetWindowThreadProcessId(existing, &pid);
            AllowSetForegroundWindow(pid);
            PostMessageW(existing, WM_COMMAND, kSettings, 0);
        }
        else if (!existing) MessageBoxW(nullptr, L"程序已经运行。若仍在使用旧版，请先从托盘退出旧版。", kTitle,
                         MB_OK | MB_ICONINFORMATION);
        CloseHandle(singleton); return 0;
    }
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) { CloseHandle(singleton); return 1; }
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW cls = {};
    cls.lpfnWndProc = WindowProc; cls.hInstance = instance; cls.lpszClassName = kClass;
    if (!RegisterClassW(&cls)) { CoUninitialize(); CloseHandle(singleton); return 1; }
    g_window = CreateWindowExW(WS_EX_TOOLWINDOW, kClass, kTitle, WS_POPUP,
                               0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!g_window) { CoUninitialize(); CloseHandle(singleton); return 1; }
    AddTray();
    g_hookReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_hookReady)
        g_hookThread = CreateThread(nullptr, 64 * 1024, HookThread, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, &g_hookThreadId);
    const bool ready = g_hookThread && WaitForSingleObject(g_hookReady, 5000) == WAIT_OBJECT_0;
    if (!ready || g_hookError) {
        MessageBoxW(nullptr, L"无法启动鼠标监听。请退出后重新运行。", kTitle, MB_OK | MB_ICONERROR);
        DestroyWindow(g_window);
    } else {
        UpdateTrayTip();
        if (g_config.notify) Notify(L"已启动：双击桌面空白处隐藏／显示图标。单击托盘图标打开设置。");
        if (g_limitError) Notify(L"5 MiB 驻留内存限制启用失败，请打开设置查看。", NIIF_WARNING);
        if (wcscmp(commandLine, L"--background") != 0 && (!g_haveConfig || g_config.startSettings)) OpenSettings();
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            if (!g_settings || !IsDialogMessageW(g_settings, &message)) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
        }
    }
    if (g_hookThread) {
        PostThreadMessageW(g_hookThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(g_hookThread, 3000);
        CloseHandle(g_hookThread);
    }
    MSG pending;
    while (PeekMessageW(&pending, nullptr, kClick, kClick, PM_REMOVE))
        HeapFree(GetProcessHeap(), 0, reinterpret_cast<Click*>(pending.lParam));
    if (g_hookReady) CloseHandle(g_hookReady);
    CoUninitialize(); CloseHandle(singleton);
    return 0;
}
