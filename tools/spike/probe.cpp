// M0 spike probe: measures FKeyIDE profile switching inside a plain Win32 Edit control.
//
// For each F-key press it injects (SendInput), it records how long until the window gets
// WM_INPUTLANGCHANGE and which HKL it reports, then types the JIS/US probe keys by scan code and
// reads back what the Edit control received. Run it with the TIP registered (tools/spike.ps1).
//   fkeyide_probe.exe [rounds]      (default 20 rounds of F18, F19, F20, F19)
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
    const int rounds = argc > 1 ? _wtoi(argv[1]) : 20;
    HWND frame = CreateWindowExW(0, L"STATIC", L"FKeyIDE probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 200, 200, 600, 200, nullptr, nullptr, nullptr, nullptr);
    g_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 10, 560, 30,
                             frame, nullptr, nullptr, nullptr);
    // WM_INPUTLANGCHANGE goes to the top-level window.
    SetWindowSubclass(frame, LangChangeProc, 1, 0);
    SetWindowSubclass(g_edit, LangChangeProc, 1, 0);

    // The TIP only sees keys while one of its profiles is active: start in FKeyIDE EN.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ITfInputProcessorProfileMgr* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&profiles));
    if (SUCCEEDED(hr)) {
        hr = profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, fkeyide::kLangIdEnglish,
                                       fkeyide::kClsidTextService, fkeyide::kProfileEnglish,
                                       nullptr, TF_IPPMF_FORPROCESS);
        profiles->Release();
    }
    std::wprintf(L"activate FKeyIDE EN: hr=%#lx hkl=%08IX\n", hr,
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
