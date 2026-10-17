#include "system_details.h"

#include <windows.h>
#include <tlhelp32.h>
#include <combaseapi.h>
#include <winternl.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

#include "guids.h"
#include "version.h"

namespace fkeyime::settings {
namespace {

std::wstring ReadString(HKEY root, const wchar_t* key, const wchar_t* name, DWORD flags = 0) {
    DWORD size = 0;
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ | flags, nullptr, nullptr, &size) !=
            ERROR_SUCCESS ||
        size < sizeof(wchar_t)) {
        return {};
    }
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ | flags, nullptr, value.data(), &size) !=
        ERROR_SUCCESS) {
        return {};
    }
    value.resize(wcslen(value.c_str()));
    return value;
}

DWORD ReadDword(HKEY root, const wchar_t* key, const wchar_t* name, DWORD fallback) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    return RegGetValueW(root, key, name, RRF_RT_REG_DWORD, nullptr, &value, &size) ==
                   ERROR_SUCCESS
               ? value
               : fallback;
}

// Every value of a key as "name = value" lines (DWORDs and strings), or "(none)".
std::wstring ListValues(HKEY root, const wchar_t* path) {
    HKEY key;
    if (RegOpenKeyExW(root, path, 0, KEY_READ, &key) != ERROR_SUCCESS) return L"  (none)\r\n";
    std::wstring out;
    for (DWORD i = 0;; ++i) {
        wchar_t name[256];
        DWORD name_size = 256;
        DWORD type = 0;
        BYTE data[1024];
        DWORD data_size = sizeof(data);
        if (RegEnumValueW(key, i, name, &name_size, nullptr, &type, data, &data_size) !=
            ERROR_SUCCESS) {
            break;
        }
        out += L"  ";
        out += name;
        out += L" = ";
        if (type == REG_DWORD) {
            out += std::to_wstring(*reinterpret_cast<DWORD*>(data));
        } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
            out.append(reinterpret_cast<wchar_t*>(data), wcsnlen(reinterpret_cast<wchar_t*>(data),
                                                                   data_size / sizeof(wchar_t)));
        } else {
            out += L"(type " + std::to_wstring(type) + L")";
        }
        out += L"\r\n";
    }
    RegCloseKey(key);
    return out.empty() ? L"  (none)\r\n" : out;
}

std::wstring Windows() {
    constexpr wchar_t kKey[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    const std::wstring build = ReadString(HKEY_LOCAL_MACHINE, kKey, L"CurrentBuild");
    std::wstring product = ReadString(HKEY_LOCAL_MACHINE, kKey, L"ProductName");
    // ProductName still says "Windows 10" on Windows 11; build 22000 and later is 11.
    if (_wtoi(build.c_str()) >= 22000 && product.rfind(L"Windows 10", 0) == 0) {
        product.replace(8, 2, L"11");
    }
    std::wstring out = product + L" " + ReadString(HKEY_LOCAL_MACHINE, kKey, L"DisplayVersion") +
                       L", build " + build + L"." +
                       std::to_wstring(ReadDword(HKEY_LOCAL_MACHINE, kKey, L"UBR", 0));
    USHORT process = 0;
    USHORT native = 0;
    if (IsWow64Process2(GetCurrentProcess(), &process, &native)) {
        out += native == IMAGE_FILE_MACHINE_ARM64 ? L", ARM64"
               : native == IMAGE_FILE_MACHINE_AMD64 ? L", x64"
                                                    : L", x86";
    }
    wchar_t locale[LOCALE_NAME_MAX_LENGTH] = L"";
    GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH);
    return out + L", locale " + locale;
}

std::wstring ClsidPath() {
    wchar_t clsid[39];
    StringFromGUID2(kClsidTextService, clsid, 39);
    return std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + clsid + L"\\InprocServer32";
}

// The user's language list with its input methods, in order, and the default override.
std::wstring Languages() {
    constexpr wchar_t kProfile[] = L"Control Panel\\International\\User Profile";
    std::wstring out;
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kProfile, L"Languages", RRF_RT_REG_MULTI_SZ, nullptr,
                     nullptr, &size) == ERROR_SUCCESS) {
        std::vector<wchar_t> list(size / sizeof(wchar_t) + 1);
        RegGetValueW(HKEY_CURRENT_USER, kProfile, L"Languages", RRF_RT_REG_MULTI_SZ, nullptr,
                     list.data(), &size);
        for (const wchar_t* lang = list.data(); *lang; lang += wcslen(lang) + 1) {
            out += L"  ";
            out += lang;
            out += L":";
            const std::wstring sub = std::wstring(kProfile) + L"\\" + lang;
            HKEY key;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, sub.c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS) {
                // Input methods are the values named "<LANGID>:<CLSID><PROFILE>" or
                // "<LANGID>:<KLID>", with their order as the data.
                std::vector<std::pair<DWORD, std::wstring>> tips;
                for (DWORD i = 0;; ++i) {
                    wchar_t name[256];
                    DWORD name_size = 256;
                    BYTE data[512];  // other values (CachedLanguageName, ...) are strings
                    DWORD data_size = sizeof(data);
                    DWORD type = 0;
                    if (RegEnumValueW(key, i, name, &name_size, nullptr, &type, data,
                                      &data_size) != ERROR_SUCCESS) {
                        break;
                    }
                    if (type == REG_DWORD && wcschr(name, L':')) {
                        tips.emplace_back(*reinterpret_cast<DWORD*>(data), name);
                    }
                }
                RegCloseKey(key);
                std::sort(tips.begin(), tips.end());
                for (const auto& [order, tip] : tips) out += L" " + tip;
            }
            out += L"\r\n";
        }
    }
    const std::wstring override_tip = ReadString(HKEY_CURRENT_USER, kProfile,
                                                 L"InputMethodOverride");
    out += L"  default input method override: " +
           (override_tip.empty() ? std::wstring(L"(none)") : override_tip) + L"\r\n";
    return out;
}

std::wstring FileVersion(const std::wstring& path) {
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (!size) return L"?";
    std::vector<BYTE> data(size);
    VS_FIXEDFILEINFO* info = nullptr;
    UINT length = 0;
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()) ||
        !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length)) {
        return L"?";
    }
    return std::to_wstring(HIWORD(info->dwFileVersionMS)) + L"." +
           std::to_wstring(LOWORD(info->dwFileVersionMS)) + L"." +
           std::to_wstring(HIWORD(info->dwFileVersionLS)) + L"." +
           std::to_wstring(LOWORD(info->dwFileVersionLS));
}

// A process's command line (same user, PROCESS_QUERY_LIMITED_INFORMATION is enough).
std::wstring CommandLineOf(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return L"(no access)";
    using Query = LONG(NTAPI*)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
    static const auto query = reinterpret_cast<Query>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess"));
    std::wstring out = L"(unknown)";
    constexpr auto kCommandLine = static_cast<PROCESSINFOCLASS>(60);  // ProcessCommandLineInformation
    ULONG size = 0;
    if (query) {
        query(process, kCommandLine, nullptr, 0, &size);
        if (size) {
            std::vector<BYTE> buffer(size);
            if (query(process, kCommandLine, buffer.data(), size, &size) >= 0) {
                const auto* text = reinterpret_cast<UNICODE_STRING*>(buffer.data());
                out.assign(text->Buffer, text->Length / sizeof(wchar_t));
            }
        }
    }
    CloseHandle(process);
    return out;
}

std::wstring AutoHotkey() {
    std::wstring out;
    bool found = false;
    for (REGSAM view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) {
        for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
            HKEY key;
            if (RegOpenKeyExW(root, L"SOFTWARE\\AutoHotkey", 0, KEY_READ | view, &key) !=
                ERROR_SUCCESS) {
                continue;
            }
            RegCloseKey(key);
            const DWORD flags = view == KEY_WOW64_32KEY ? RRF_SUBKEY_WOW6432KEY
                                                        : RRF_SUBKEY_WOW6464KEY;
            const std::wstring version =
                ReadString(root, L"SOFTWARE\\AutoHotkey", L"Version", flags);
            const std::wstring dir = ReadString(root, L"SOFTWARE\\AutoHotkey", L"InstallDir", flags);
            out += L"  installed: " + (version.empty() ? std::wstring(L"?") : version) + L" in " +
                   (dir.empty() ? std::wstring(L"?") : dir) + L"\r\n";
            found = true;
            break;
        }
        if (found) break;
    }
    if (!found) out += L"  installed: no (no SOFTWARE\\AutoHotkey key)\r\n";

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry = {sizeof(entry)};
    int running = 0;
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
        if (_wcsnicmp(entry.szExeFile, L"AutoHotkey", 10) != 0) continue;
        ++running;
        std::wstring exe;
        if (HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID)) {
            wchar_t path[MAX_PATH];
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(p, 0, path, &size)) exe = path;
            CloseHandle(p);
        }
        out += L"  running: pid " + std::to_wstring(entry.th32ProcessID) + L", " +
               entry.szExeFile + L" " + (exe.empty() ? L"?" : FileVersion(exe)) + L": " +
               CommandLineOf(entry.th32ProcessID) + L"\r\n";
    }
    CloseHandle(snapshot);
    if (!running) out += L"  running: none\r\n";
    return out;
}

}  // namespace

std::wstring SystemDetails(bool include_autohotkey) {
    std::wstring out = L"FKeyIME " FKEYIME_VERSION L" (" FKEYIME_COMMIT L")\r\n";
    out += L"Windows: " + Windows() + L"\r\n";
    out += L"FKeyIME DLL (x64): " +
           ReadString(HKEY_LOCAL_MACHINE, ClsidPath().c_str(), nullptr, RRF_SUBKEY_WOW6464KEY) +
           L"\r\n";
    out += L"FKeyIME DLL (x86): " +
           ReadString(HKEY_LOCAL_MACHINE, ClsidPath().c_str(), nullptr, RRF_SUBKEY_WOW6432KEY) +
           L"\r\n";
    out += L"Options (HKLM\\Software\\fkeyime):\r\n" +
           ListValues(HKEY_LOCAL_MACHINE, L"Software\\fkeyime");
    out += L"Options (HKCU\\Software\\fkeyime):\r\n" +
           ListValues(HKEY_CURRENT_USER, L"Software\\fkeyime");
    out += L"Language list:\r\n" + Languages();
    if (include_autohotkey) {
        out += L"AutoHotkey:\r\n" + AutoHotkey();
    } else {
        out += L"AutoHotkey: not included (the user said no)\r\n";
    }
    return out;
}

}  // namespace fkeyime::settings
