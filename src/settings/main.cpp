// FKeyIME settings window (docs/spec.md, section 7, item 8): Report an issue, Copy system
// details. Opened from Windows' input method settings (the TIP's ITfFnConfigure) and the Start
// menu. Options themselves are set by the installer for now.
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <utility>

#include "system_details.h"
#include "version.h"

namespace {

constexpr int kReport = 1;
constexpr int kCopy = 2;
constexpr wchar_t kClass[] = L"FKeyIMESettings";

HFONT g_font = nullptr;
HWND g_status = nullptr;

int Scale(HWND window, int value) { return MulDiv(value, GetDpiForWindow(window), 96); }

bool CopyToClipboard(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    bool ok = false;
    if (memory) {
        memcpy(GlobalLock(memory), text.c_str(), bytes);
        GlobalUnlock(memory);
        ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
        if (!ok) GlobalFree(memory);
    }
    CloseClipboard();
    return ok;
}

void CopyDetails(HWND window) {
    const int answer = MessageBoxW(
        window,
        L"Also include AutoHotkey details?\n\n"
        L"That's whether AutoHotkey is installed and its version, and the scripts running now "
        L"(their file paths, which can contain your user name).\n\n"
        L"Nothing is sent anywhere: the details only go to the clipboard, for you to paste into "
        L"an issue.",
        L"FKeyIME: copy system details", MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (answer == IDCANCEL) return;
    const std::wstring details = fkeyime::settings::SystemDetails(answer == IDYES);
    SetWindowTextW(g_status, CopyToClipboard(window, details)
                                 ? L"Copied. Paste it into the issue."
                                 : L"Couldn't open the clipboard. Try again.");
}

HWND Add(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, int id, int x, int y,
         int w, int h) {
    HWND child = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, Scale(parent, x),
                                 Scale(parent, y), Scale(parent, w), Scale(parent, h), parent,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr,
                                 nullptr);
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    return child;
}

void CreateFont(HWND window) {
    NONCLIENTMETRICSW metrics = {sizeof(metrics)};
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0,
                               GetDpiForWindow(window));
    if (g_font) DeleteObject(g_font);
    g_font = CreateFontIndirectW(&metrics.lfMessageFont);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            CreateFont(window);
            Add(window, L"STATIC",
                L"FKeyIME " FKEYIME_VERSION L"\n\nFound a problem? Report it on GitHub. The "
                L"developer will try to fix it if possible.",
                SS_LEFT, 0, 16, 12, 328, 64);
            Add(window, L"BUTTON", L"Report an issue", BS_DEFPUSHBUTTON | WS_TABSTOP, kReport,
                16, 84, 200, 30);
            Add(window, L"BUTTON", L"Copy system details", BS_PUSHBUTTON | WS_TABSTOP, kCopy, 16,
                122, 200, 30);
            g_status = Add(window, L"STATIC", L"", SS_LEFT, 0, 16, 160, 328, 20);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wparam) == kReport) {
                ShellExecuteW(window, L"open", FKEYIME_ISSUES_URL, nullptr, nullptr,
                              SW_SHOWNORMAL);
            } else if (LOWORD(wparam) == kCopy) {
                CopyDetails(window);
            }
            return 0;
        case WM_CTLCOLORSTATIC:
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int show) {
    // Testing: "--details <file>" / "--details-ahk <file>" writes the text instead of copying.
    for (const auto& [flag, ahk] : {std::pair{L"--details-ahk ", true}, {L"--details ", false}}) {
        if (wcsncmp(command_line, flag, wcslen(flag)) != 0) continue;
        const std::wstring text = fkeyime::settings::SystemDetails(ahk);
        HANDLE file = CreateFileW(command_line + wcslen(flag), GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return 1;
        DWORD written;
        WriteFile(file, "\xEF\xBB\xBF", 3, &written, nullptr);  // UTF-8 BOM
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr,
                                              nullptr);
        std::string utf8(bytes, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, utf8.data(), bytes, nullptr, nullptr);
        WriteFile(file, utf8.data(), bytes - 1, &written, nullptr);
        CloseHandle(file);
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    // One window: a second start brings the first one forward.
    if (HWND existing = FindWindowW(kClass, nullptr)) {
        SetForegroundWindow(existing);
        return 0;
    }
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = kClass;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    const UINT dpi = GetDpiForSystem();
    RECT rect = {0, 0, MulDiv(360, dpi, 96), MulDiv(192, dpi, 96)};
    constexpr DWORD kStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRectExForDpi(&rect, kStyle, FALSE, 0, dpi);
    HWND window = CreateWindowExW(0, kClass, L"FKeyIME settings", kStyle, CW_USEDEFAULT,
                                  CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
                                  nullptr, nullptr, instance, nullptr);
    ShowWindow(window, show);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(window, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
