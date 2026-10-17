// M0 spike probe: measures FKeyIME profile switching inside a plain Win32 Edit control.
//
// For each F-key press it injects (SendInput), it records how long until the window gets
// WM_INPUTLANGCHANGE and which HKL it reports, then types the JIS/US probe keys by scan code and
// reads back what the Edit control received. Run it with the TIP registered (tools/spike.ps1).
//   fkeyime_probe.exe [rounds]      (default 20 rounds of F18, F19, F20, F19)
//   fkeyime_probe.exe --handoff [rounds]
//       FKeyIME API + AutoHotkey hand-off (docs/api.md): F18-F20 must reach FKeyIME while it's
//       active, the API must report its mode, and with a Microsoft IME active the keys must go to
//       AutoHotkey instead. Restores the session's input method at the end.
#include <windows.h>
#include <commctrl.h>
#include <msctf.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "guids.h"

namespace {

HWND g_edit = nullptr;
TF_INPUTPROCESSORPROFILE g_prior = {};  // the session's profile before the probe changed it
long long g_langchange_us = 0;
HKL g_langchange_hkl = nullptr;

long long NowUs() {
    static const long long f = [] {
        LARGE_INTEGER v;
        QueryPerformanceFrequency(&v);
        return v.QuadPart;
    }();
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart / f * 1000000 + t.QuadPart % f * 1000000 / f;
}

LRESULT CALLBACK LangChangeProc(HWND w, UINT m, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (m == WM_INPUTLANGCHANGE) {
        g_langchange_us = NowUs();
        g_langchange_hkl = reinterpret_cast<HKL>(lp);
    }
    return DefSubclassProc(w, m, wp, lp);
}

void Pump(int ms) {
    const long long until = NowUs() + ms * 1000LL;
    MSG msg;
    while (NowUs() < until) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(1);
    }
}

void SendKey(WORD vk, WORD scan, bool shift = false, bool by_scan = false) {
    std::vector<INPUT> in;
    auto add = [&](WORD v, WORD s, DWORD flags) {
        INPUT i = {INPUT_KEYBOARD};
        i.ki.wVk = by_scan ? 0 : v;
        i.ki.wScan = s;
        i.ki.dwFlags = flags | (by_scan ? KEYEVENTF_SCANCODE : 0);
        in.push_back(i);
    };
    if (shift) add(VK_LSHIFT, 0x2a, 0);
    add(vk, scan, 0);
    add(vk, scan, KEYEVENTF_KEYUP);
    if (shift) add(VK_LSHIFT, 0x2a, KEYEVENTF_KEYUP);
    SendInput(static_cast<UINT>(in.size()), in.data(), sizeof(INPUT));
}

std::wstring ProbeSymbols() {
    SetWindowTextW(g_edit, L"");
    struct P {
        WORD scan;
        bool shift;
    };
    // Shift+2, ろ, Shift+ろ, ¥, @, [, :  (JIS: " \ _ ¥ @ [ :   US: @ - - - [ ] ')
    for (P p : {P{0x03, true}, P{0x73, false}, P{0x73, true}, P{0x7d, false}, P{0x1a, false},
                P{0x1b, false}, P{0x28, false}}) {
        SendKey(0, p.scan, p.shift, true);
        SendKey(0, 0x39, false, true);  // space as a separator
        Pump(15);
    }
    Pump(50);
    wchar_t text[128] = L"";
    GetWindowTextW(g_edit, text, 128);
    return text;
}

TF_INPUTPROCESSORPROFILE ActiveProfile() {
    TF_INPUTPROCESSORPROFILE p = {};
    ITfInputProcessorProfileMgr* profiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&profiles)))) {
        profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &p);
        profiles->Release();
    }
    return p;
}

HRESULT Activate(const TF_INPUTPROCESSORPROFILE& p, DWORD flags) {
    ITfInputProcessorProfileMgr* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&profiles));
    if (SUCCEEDED(hr)) {
        hr = profiles->ActivateProfile(p.dwProfileType, p.langid, p.clsid, p.guidProfile, p.hkl,
                                       flags);
        profiles->Release();
    }
    return hr;
}

int Handoff(int rounds) {
    // Load the installed DLL the way other programs do: through the CLSID (docs/api.md).
    wchar_t path[MAX_PATH] = L"";
    DWORD size = sizeof(path);
    RegGetValueW(HKEY_LOCAL_MACHINE,
                 L"SOFTWARE\\Classes\\CLSID\\{66749D2A-1770-43D3-A269-A051B5BC1AAA}\\InprocServer32",
                 nullptr, RRF_RT_REG_SZ, nullptr, path, &size);
    HMODULE dll = LoadLibraryW(path);
    using QueryFn = int(WINAPI*)(HWND);
    using VersionFn = int(WINAPI*)();
    auto query = dll ? reinterpret_cast<QueryFn>(GetProcAddress(dll, "FKeyImeQueryWindow")) : nullptr;
    auto version =
        dll ? reinterpret_cast<VersionFn>(GetProcAddress(dll, "FKeyImeApiVersion")) : nullptr;
    if (!query || !version) {
        std::wprintf(L"API not found in [%s]\n", path);
        return 1;
    }
    std::wprintf(L"API v%d from %s\n", version(), path);

    struct Step {
        WORD vk, scan;
        int mode;
        const GUID* profile;
    };
    const Step fkeys[] = {{VK_F18, 0x69, 1, &fkeyime::kProfileKorean},
                          {VK_F19, 0x6a, 0, &fkeyime::kProfileEnglish},
                          {VK_F20, 0x6b, 2, &fkeyime::kProfileJapanese},
                          {VK_F16, 0x67, 2, &fkeyime::kProfileJapanese}};

    // 1. FKeyIME active: every key must reach FKeyIME, and the API must follow.
    int ok_profile = 0, ok_api = 0, taken_by_other = 0, total = 0;
    for (int r = 0; r < rounds; ++r) {
        for (const Step& s : fkeys) {
            SendKey(s.vk, s.scan);
            Pump(250);
            const TF_INPUTPROCESSORPROFILE p = ActiveProfile();
            const bool ours = IsEqualGUID(p.clsid, fkeyime::kClsidTextService) != FALSE;
            ok_profile += ours && IsEqualGUID(p.guidProfile, *s.profile);
            taken_by_other += !ours;
            ok_api += query(g_edit) == s.mode;
            ++total;
        }
    }
    std::wprintf(L"FKeyIME active: %d presses, right FKeyIME profile %d, API right %d, "
                 L"switched to another IME %d\n", total, ok_profile, ok_api, taken_by_other);

    // 2. Microsoft Korean IME active: the API must say "not active".
    TF_INPUTPROCESSORPROFILE ms = {};
    ms.dwProfileType = TF_PROFILETYPE_INPUTPROCESSOR;
    ms.langid = 0x0412;
    ms.clsid = {0xa028ae76, 0x01b1, 0x46c2, {0x99, 0xc4, 0xac, 0xd9, 0x85, 0x8a, 0xe0, 0x2f}};
    ms.guidProfile = {0xb5fe1f02, 0xd5f2, 0x4445, {0x9c, 0x03, 0xc5, 0x68, 0xf2, 0x3c, 0x99, 0xa1}};
    HRESULT hr = Activate(ms, TF_IPPMF_FORPROCESS);
    Pump(300);
    std::wprintf(L"Microsoft Korean IME active (hr=%#lx): API says %d (expected -1)\n", hr,
                 query(g_edit));

    // 3. Still a Microsoft IME: AutoHotkey must handle the keys (language changes, not FKeyIME).
    struct AhkStep {
        WORD vk, scan;
        LANGID lang;
    };
    const AhkStep ahk_keys[] = {{VK_F20, 0x6b, 0x0411}, {VK_F18, 0x69, 0x0412}};
    int ahk_ok = 0, ahk_total = 0;
    for (int r = 0; r < rounds; ++r) {
        for (const AhkStep& k : ahk_keys) {
            SendKey(k.vk, k.scan);
            Pump(400);
            auto right = [&] {
                const TF_INPUTPROCESSORPROFILE p = ActiveProfile();
                return !IsEqualGUID(p.clsid, fkeyime::kClsidTextService) &&
                       LOWORD(reinterpret_cast<ULONG_PTR>(GetKeyboardLayout(0))) == k.lang;
            };
            if (right()) {
                ++ahk_ok;
            } else {
                // Late (right after another 600 ms) or wrong (still not the Microsoft IME).
                const TF_INPUTPROCESSORPROFILE p = ActiveProfile();
                const LANGID now = LOWORD(reinterpret_cast<ULONG_PTR>(GetKeyboardLayout(0)));
                Pump(600);
                std::wprintf(L"  round %d F%d: at 400 ms lang=%04x fkeyime=%d -> %s\n", r,
                             k.vk - VK_F17 + 17, now,
                             IsEqualGUID(p.clsid, fkeyime::kClsidTextService) != FALSE,
                             right() ? L"right at 1 s (late)" : L"still wrong at 1 s");
            }
            ++ahk_total;
        }
    }
    std::wprintf(L"Microsoft IME active: %d presses, handled by AutoHotkey %d\n", ahk_total,
                 ahk_ok);

    hr = Activate(g_prior, TF_IPPMF_FORSESSION);
    std::wprintf(L"restored the session's input method (hr=%#lx)\n", hr);
    FreeLibrary(dll);
    return ok_profile == total && ok_api == total && ahk_ok == ahk_total ? 0 : 2;
}

}  // namespace

// --send: type F18 Shift+2 ろ, F19 Shift+2 ろ, F20 Shift+2 ろ, F17, F19 into the foreground
// window, for checking key delivery in other apps (the TIP log shows what arrived).
int SendToForeground() {
    wchar_t title[256] = L"";
    GetWindowTextW(GetForegroundWindow(), title, 256);
    std::wprintf(L"sending to foreground window \"%s\"\n", title);
    const WORD fkeys[][2] = {{VK_F18, 0x69}, {VK_F19, 0x6a}, {VK_F20, 0x6b}};
    for (const auto& k : fkeys) {
        SendKey(k[0], k[1]);
        Pump(200);
        SendKey(0, 0x03, true, true);
        SendKey(0, 0x73, false, true);
        SendKey(0, 0x39, false, true);
        Pump(200);
    }
    SendKey(VK_F17, 0x68);
    Pump(200);
    SendKey(VK_F19, 0x6a);
    Pump(200);
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    if (argc > 1 && !wcscmp(argv[1], L"--send")) return SendToForeground();
    const bool handoff = argc > 1 && !wcscmp(argv[1], L"--handoff");
    const int rounds =
        handoff ? (argc > 2 ? _wtoi(argv[2]) : 20) : (argc > 1 ? _wtoi(argv[1]) : 20);
    HWND frame = CreateWindowExW(0, L"STATIC", L"FKeyIME probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 200, 200, 600, 200, nullptr, nullptr, nullptr, nullptr);
    g_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 10, 560, 30,
                             frame, nullptr, nullptr, nullptr);
    // WM_INPUTLANGCHANGE goes to the top-level window.
    SetWindowSubclass(frame, LangChangeProc, 1, 0);
    SetWindowSubclass(g_edit, LangChangeProc, 1, 0);

    // The TIP only sees keys while one of its profiles is active: start in FKeyIME EN.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    g_prior = ActiveProfile();
    ITfInputProcessorProfileMgr* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&profiles));
    if (SUCCEEDED(hr)) {
        hr = profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, fkeyime::kLangIdEnglish,
                                       fkeyime::kClsidTextService, fkeyime::kProfileEnglish,
                                       nullptr, TF_IPPMF_FORPROCESS);
        profiles->Release();
    }
    std::wprintf(L"activate FKeyIME EN: hr=%#lx hkl=%08IX\n", hr,
                 reinterpret_cast<ULONG_PTR>(GetKeyboardLayout(0)));

    // A process started from a console doesn't always get the foreground. Ask for it.
    SetForegroundWindow(frame);
    SetFocus(g_edit);
    Pump(500);
    if (GetForegroundWindow() != frame) {
        std::wprintf(L"probe window is not in the foreground; click it within 5 s\n");
        for (int i = 0; i < 50 && GetForegroundWindow() != frame; ++i) Pump(100);
        SetFocus(g_edit);
    }
    std::wprintf(L"foreground=%d\n", GetForegroundWindow() == frame);
    if (handoff) {
        const int result = Handoff(rounds);
        DestroyWindow(frame);
        return result;
    }

    struct Step {
        const wchar_t* name;
        WORD vk, scan;
    };
    const Step steps[] = {{L"F18", VK_F18, 0x69}, {L"F19", VK_F19, 0x6a},
                          {L"F20", VK_F20, 0x6b}, {L"F19", VK_F19, 0x6a}};

    // Symbols per mode once, then latency rounds.
    for (const Step& s : steps) {
        SendKey(s.vk, s.scan);
        Pump(300);
        std::wprintf(L"symbols after %s: hkl=%08IX [%s]\n", s.name,
                     reinterpret_cast<ULONG_PTR>(GetKeyboardLayout(0)), ProbeSymbols().c_str());
    }

    std::vector<double> latencies[4];
    int missing[4] = {};
    for (int r = 0; r < rounds; ++r) {
        for (int i = 0; i < 4; ++i) {
            g_langchange_us = 0;
            const long long start = NowUs();
            SendKey(steps[i].vk, steps[i].scan);
            Pump(250);
            if (g_langchange_us) {
                latencies[i].push_back((g_langchange_us - start) / 1000.0);
            } else {
                ++missing[i];
            }
        }
    }
    for (int i = 0; i < 4; ++i) {
        auto& v = latencies[i];
        std::sort(v.begin(), v.end());
        if (v.empty()) {
            std::wprintf(L"%s (step %d): no WM_INPUTLANGCHANGE in %d presses\n", steps[i].name, i,
                         missing[i]);
            continue;
        }
        std::wprintf(L"%s (step %d): n=%zu missing=%d  min %.1f  median %.1f  p95 %.1f  max %.1f ms\n",
                     steps[i].name, i, v.size(), missing[i], v.front(), v[v.size() / 2],
                     v[std::min(v.size() - 1, v.size() * 95 / 100)], v.back());
    }
    // Text that leaked into the Edit control (F-keys must be eaten, so this should be empty).
    SetWindowTextW(g_edit, L"");
    for (const Step& s : steps) SendKey(s.vk, s.scan);
    Pump(300);
    wchar_t text[64] = L"";
    GetWindowTextW(g_edit, text, 64);
    std::wprintf(L"edit text after F-keys only: [%s]\n", text);
    DestroyWindow(frame);
    return 0;
}
