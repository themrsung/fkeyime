#include "user_setup.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace fkeyime::settings {
namespace {

// "<LANGID>:{CLSID}{PROFILE}" for FKeyIME EN, KO, JA (src/tip/guids.h; permanent).
constexpr wchar_t kEnglish[] =
    L"0409:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{8F61A2C0-75BC-4042-B739-CC7D63B542E5}";
constexpr wchar_t kAll[] =
    L"0409:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{8F61A2C0-75BC-4042-B739-CC7D63B542E5};"
    L"0412:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{28C201BF-390E-482F-8259-CD26E05844A9};"
    L"0411:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{1AC13F3E-07AC-4B03-A606-967BD48BFD99}";

// input.dll, documented: https://learn.microsoft.com/windows/win32/tsf/installlayoutortip
constexpr DWORD kIlotUninstall = 0x00000001;
constexpr DWORD kIlotDefProfile = 0x00000002;
using InstallLayoutOrTipFn = BOOL(WINAPI*)(LPCWSTR, DWORD);

constexpr wchar_t kUserProfile[] = L"Control Panel\\International\\User Profile";

void Log(const wchar_t* format, ...) {
    wchar_t path[MAX_PATH];
    if (!ExpandEnvironmentStringsW(L"%ProgramData%\\fkeyime\\setup.log", path, MAX_PATH)) return;
    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t line[512];
    int n = swprintf_s(line, L"%04u-%02u-%02u %02u:%02u:%02u ", now.wYear, now.wMonth, now.wDay,
                       now.wHour, now.wMinute, now.wSecond);
    va_list args;
    va_start(args, format);
    const int m = vswprintf_s(line + n, 512 - n - 2, format, args);
    va_end(args);
    if (n < 0 || m < 0) return;
    n += m;
    line[n++] = L'\r';
    line[n++] = L'\n';
    char utf8[1536];
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, line, n, utf8, sizeof(utf8), nullptr, nullptr);
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(file, utf8, static_cast<DWORD>(bytes), &written, nullptr);
    CloseHandle(file);
}

int Call(const wchar_t* tips, DWORD flags) {
    HMODULE input = LoadLibraryExW(L"input.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!input) return static_cast<int>(GetLastError());
    auto install = reinterpret_cast<InstallLayoutOrTipFn>(GetProcAddress(input, "InstallLayoutOrTip"));
    int error = ERROR_PROC_NOT_FOUND;
    if (install) error = install(tips, flags) ? 0 : static_cast<int>(GetLastError() ? GetLastError() : 1);
    FreeLibrary(input);
    return error;
}

std::wstring Override() {
    wchar_t value[256] = L"";
    DWORD size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, kUserProfile, L"InputMethodOverride", RRF_RT_REG_SZ, nullptr,
                 value, &size);
    return value;
}

}  // namespace

int UserInstall() {
    int error = Call(kAll, 0);
    Log(L"user-install: add the 3 profiles to the language list: %d", error);
    if (error) return error;
    // The default input method (Settings: "Override for default input method"). Not enough on
    // its own to start each session in FKeyIME (docs/issues/sign-in-starts-on-microsoft-ime.md).
    error = Call(kEnglish, kIlotDefProfile);
    // ILOT_DEFPROFILE returns success but leaves the override empty (measured, Windows Sandbox
    // 24H2): write the value Set-WinDefaultInputMethodOverride writes.
    if (Override() != kEnglish) {
        const LSTATUS status = RegSetKeyValueW(HKEY_CURRENT_USER, kUserProfile,
                                               L"InputMethodOverride", REG_SZ, kEnglish,
                                               sizeof(kEnglish));
        Log(L"user-install: write InputMethodOverride: %ld", status);
        if (status != ERROR_SUCCESS) error = static_cast<int>(status);
    }
    Log(L"user-install: FKeyIME EN as the default input method: %d (override now %s)", error,
        Override().c_str());
    return error;
}

int UserUninstall() {
    // Clear the override only if it's ours, then take the profiles out of the list. The
    // Microsoft IMEs were never removed by this step, so nothing needs to come back.
    if (Override() == kEnglish) {
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kUserProfile,
                                                  L"InputMethodOverride");
        Log(L"user-uninstall: clear the default input method override: %ld", status);
    }
    const int error = Call(kAll, kIlotUninstall);
    Log(L"user-uninstall: remove the 3 profiles from the language list: %d", error);
    return error;
}

}  // namespace fkeyime::settings
