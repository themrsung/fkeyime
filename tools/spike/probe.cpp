// M0 spike probe: measures FKeyIME profile switching inside a plain Win32 Edit control.
//
// For each F-key press it injects (SendInput), it records how long until the window gets
// WM_INPUTLANGCHANGE and which HKL it reports, then types the JIS/US probe keys by scan code and
// reads back what the Edit control received. Run it with the TIP registered (tools/spike.ps1).
//   fkeyime_probe.exe [rounds]      (default 20 rounds of F18, F19, F20, F19)
//   fkeyime_probe.exe --typing [EN|KO|JA]    (--typing-rich: in a TSF-aware RichEdit)
//       Types key sequences by scan code in each mode and checks the text that lands: JIS
//       English, Korean 2-Set, Japanese kana with F6-F8 (docs/spec.md, M1-M3).
//   fkeyime_probe.exe --hanja | --hanja-rich | --hanja-fg
//       Microsoft Korean IME Hanja through the AutoHotkey path (F18, 한, F17, then picking with a
//       digit, Enter or Down+Enter), in the probe's control or the foreground window (--hanja-fg).
//   fkeyime_probe.exe --active | --active-loop [s] | --passive [s] [ms] | --session-ko | --session-ms
//       The session's input method, as a fresh thread / a long-lived windowless thread sees it;
//       a plain window that never picks one (optionally starting on Microsoft's Korean IME);
//       switch the whole session to FKeyIME KO or to the Microsoft Korean IME.
//   fkeyime_probe.exe --type-fg <tokens>
//       Types a --typing token sequence into the foreground window and prints its title.
//   fkeyime_probe.exe --keylog [seconds]
//       Records key codes (no text) with a low-level hook, then the foreground control's text.
//   fkeyime_probe.exe --handoff [rounds]
//       FKeyIME API + AutoHotkey hand-off (docs/api.md): F18-F20 must reach FKeyIME while it's
//       active, the API must report its mode, with a Microsoft IME chosen the keys must go to
//       AutoHotkey, and a window stuck on Microsoft's IME while the session is on FKeyIME must be
//       taken over by FKeyIME on F18. Restores the session's input method at the end.
#include <windows.h>
#include <commctrl.h>
#include <msctf.h>
#include <richedit.h>

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

void SendKey(WORD vk, WORD scan, bool shift = false, bool by_scan = false,
             bool extended = false, bool alt = false) {
    std::vector<INPUT> in;
    auto add = [&](WORD v, WORD s, DWORD flags) {
        INPUT i = {INPUT_KEYBOARD};
        i.ki.wVk = by_scan ? 0 : v;
        i.ki.wScan = s;
        i.ki.dwFlags = flags | (by_scan ? KEYEVENTF_SCANCODE : 0);
        if (extended && v == vk) i.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        in.push_back(i);
    };
    if (alt) add(VK_LMENU, 0x38, 0);
    if (shift) add(VK_LSHIFT, 0x2a, 0);
    add(vk, scan, 0);
    add(vk, scan, KEYEVENTF_KEYUP);
    if (shift) add(VK_LSHIFT, 0x2a, KEYEVENTF_KEYUP);
    if (alt) add(VK_LMENU, 0x38, KEYEVENTF_KEYUP);
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

std::wstring Escaped(const std::wstring& s);

// --hanja: the Microsoft Korean IME's Hanja conversion through the AutoHotkey path (F18 for 한,
// F17 for 한자), picking the candidate with a digit and with Enter. Prints what lands.
int Hanja() {
    TF_INPUTPROCESSORPROFILE ms = {};
    ms.dwProfileType = TF_PROFILETYPE_INPUTPROCESSOR;
    ms.langid = 0x0412;
    ms.clsid = {0xa028ae76, 0x01b1, 0x46c2, {0x99, 0xc4, 0xac, 0xd9, 0x85, 0x8a, 0xe0, 0x2f}};
    ms.guidProfile = {0xb5fe1f02, 0xd5f2, 0x4445, {0x9c, 0x03, 0xc5, 0x68, 0xf2, 0x3c, 0x99, 0xa1}};
    std::wprintf(L"Microsoft Korean IME: hr=%#lx\n", Activate(ms, TF_IPPMF_FORPROCESS));
    Pump(300);
    struct Pick {
        const wchar_t* name;
        WORD vk, scan;
        bool down_first;
    };
    for (const Pick& pick : {Pick{L"digit 1", 0, 0x02, false}, Pick{L"Enter", VK_RETURN, 0x1c, false},
                             Pick{L"Down, Enter", VK_RETURN, 0x1c, true},
                             Pick{L"Down, digit 2", 0, 0x03, true}}) {
        SetWindowTextW(g_edit, L"");
        Pump(100);
        SendKey(VK_F18, 0x69);  // AutoHotkey: 한 mode
        Pump(400);
        for (WORD scan : {WORD{0x22}, WORD{0x25}, WORD{0x1f}}) {  // ㅎ ㅏ ㄴ = 한
            SendKey(0, scan, false, true);
            Pump(60);
        }
        SendKey(VK_F17, 0x68);  // AutoHotkey: VK_HANJA
        Pump(600);
        if (pick.down_first) {
            SendKey(VK_DOWN, 0x50, false, false, true);
            Pump(300);
        }
        SendKey(pick.vk, pick.scan, false, pick.vk == 0);
        Pump(400);
        wchar_t text[64] = L"";
        GetWindowTextW(g_edit, text, 64);
        std::wprintf(L"pick with %s: [%s]\n", pick.name, Escaped(text).c_str());
    }
    std::wprintf(L"restored the session's input method (hr=%#lx)\n", Activate(g_prior, TF_IPPMF_FORSESSION));
    return 0;
}

// The text of the foreground window's edit control (Windows 11 Notepad: RichEditD2DPT).
std::wstring ForegroundText() {
    struct Find {
        HWND found = nullptr;
    } find;
    EnumChildWindows(GetForegroundWindow(), [](HWND w, LPARAM p) -> BOOL {
        wchar_t cls[64] = L"";
        GetClassNameW(w, cls, 64);
        if ((!wcscmp(cls, L"RichEditD2DPT") || !wcscmp(cls, L"Edit")) && IsWindowVisible(w)) {
            reinterpret_cast<Find*>(p)->found = w;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&find));
    if (!find.found) return L"(no edit control)";
    wchar_t text[256] = L"";
    SendMessageTimeoutW(find.found, WM_GETTEXT, 256, reinterpret_cast<LPARAM>(text), SMTO_ABORTIFHUNG,
                        1000, nullptr);
    return text;
}

// --hanja-fg: the same Hanja keys as --hanja, typed into the foreground window (e.g. Notepad),
// clearing it with Ctrl+A, Delete before each try. Reads back through WM_GETTEXT.
int HanjaForeground() {
    wchar_t title[256] = L"";
    GetWindowTextW(GetForegroundWindow(), title, 256);
    std::wprintf(L"foreground window \"%s\"\n", title);
    struct Pick {
        const wchar_t* name;
        WORD vk, scan;
        bool down_first;
    };
    for (int round = 0; round < 2; ++round) {
        for (const Pick& pick : {Pick{L"Down, Enter", VK_RETURN, 0x1c, true},
                                 Pick{L"Enter", VK_RETURN, 0x1c, false},
                                 Pick{L"digit 1", 0, 0x02, false}}) {
            INPUT in[4] = {};
            for (INPUT& i : in) i.type = INPUT_KEYBOARD;
            in[0].ki.wVk = VK_CONTROL;
            in[1].ki.wVk = 'A';
            in[2].ki.wVk = 'A';
            in[2].ki.dwFlags = KEYEVENTF_KEYUP;
            in[3].ki.wVk = VK_CONTROL;
            in[3].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(4, in, sizeof(INPUT));
            Pump(150);
            SendKey(VK_DELETE, 0x53, false, false, true);
            Pump(200);
            SendKey(VK_F18, 0x69);  // AutoHotkey: Microsoft Korean IME, 한 mode
            Pump(500);
            for (WORD scan : {WORD{0x22}, WORD{0x25}, WORD{0x1f}}) {  // 한
                SendKey(0, scan, false, true);
                Pump(80);
            }
            SendKey(VK_F17, 0x68);  // AutoHotkey: VK_HANJA
            Pump(700);
            if (pick.down_first) {
                SendKey(VK_DOWN, 0x50, false, false, true);
                Pump(300);
            }
            SendKey(pick.vk, pick.scan, false, pick.vk == 0);
            Pump(500);
            std::wprintf(L"round %d, pick with %s: [%s]\n", round + 1, pick.name,
                         Escaped(ForegroundText()).c_str());
        }
    }
    return 0;
}

// --keylog <seconds>: records every key event (codes only, no text) with a low-level hook, for
// diagnosing physical-keyboard issues. Then prints the foreground edit control's text.
struct KeyRecord {
    DWORD time, vk, scan, flags;
    bool up;
};
std::vector<KeyRecord>* g_keylog = nullptr;

LRESULT CALLBACK KeyLogHook(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && g_keylog) {
        const auto* k = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
        g_keylog->push_back({k->time, k->vkCode, k->scanCode, k->flags,
                             wp == WM_KEYUP || wp == WM_SYSKEYUP});
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

int KeyLog(int seconds) {
    std::vector<KeyRecord> records;
    g_keylog = &records;
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyLogHook, GetModuleHandleW(nullptr), 0);
    std::wprintf(L"recording keys for %d s (hook=%d)\n", seconds, hook != nullptr);
    Pump(seconds * 1000);
    UnhookWindowsHookEx(hook);
    g_keylog = nullptr;
    const DWORD t0 = records.empty() ? 0 : records.front().time;
    for (const KeyRecord& r : records) {
        std::wprintf(L"%6lu ms %-4s vk=%02lX sc=%03lX%s%s\n", r.time - t0, r.up ? L"up" : L"down",
                     r.vk, r.scan, (r.flags & LLKHF_EXTENDED) ? L" ext" : L"",
                     (r.flags & LLKHF_INJECTED) ? L" INJECTED" : L"");
    }
    wchar_t title[256] = L"";
    GetWindowTextW(GetForegroundWindow(), title, 256);
    std::wprintf(L"foreground \"%s\" text: [%s]\n", title, Escaped(ForegroundText()).c_str());
    return 0;
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
    // Chosen for the whole session, as Win+Space does: then AutoHotkey switches as in v1.5.2.
    HRESULT hr = Activate(ms, TF_IPPMF_FORSESSION | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
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

    // 4. FKeyIME chosen for the session, but this window still on Microsoft's IME (stuck): F18
    //    must bring FKeyIME into the window (AutoHotkey moves it and sends the key again), and
    //    Korean must then type through FKeyIME.
    TF_INPUTPROCESSORPROFILE fk = {};
    fk.dwProfileType = TF_PROFILETYPE_INPUTPROCESSOR;
    fk.langid = fkeyime::kLangIdEnglish;
    fk.clsid = fkeyime::kClsidTextService;
    fk.guidProfile = fkeyime::kProfileEnglish;
    int stuck_ok = 0, stuck_total = 0;
    for (int r = 0; r < rounds; ++r) {
        Activate(fk, TF_IPPMF_FORSESSION | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
        Pump(200);
        Activate(ms, TF_IPPMF_FORPROCESS);  // only this window goes back to Microsoft's IME
        Pump(300);
        const bool was_stuck = query(g_edit) < 0;
        SetWindowTextW(g_edit, L"");
        SendKey(VK_F18, 0x69);
        Pump(600);
        for (WORD scan : {WORD{0x13}, WORD{0x25}}) {  // ㄱ ㅏ
            SendKey(0, scan, false, true);
            Pump(60);
        }
        SendKey(VK_RETURN, 0x1c);
        Pump(300);
        wchar_t text[16] = L"";
        GetWindowTextW(g_edit, text, 16);
        const bool ok = was_stuck && query(g_edit) == 1 && !wcscmp(text, L"가");
        stuck_ok += ok;
        ++stuck_total;
        if (!ok) {
            std::wprintf(L"  round %d: was stuck %d, API now %d, text [%s]\n", r, was_stuck,
                         query(g_edit), Escaped(text).c_str());
        }
    }
    std::wprintf(L"Stuck window (session FKeyIME, window Microsoft): %d presses of F18, FKeyIME "
                 L"took over and typed U+AC00 %d\n", stuck_total, stuck_ok);

    hr = Activate(g_prior, TF_IPPMF_FORSESSION);
    std::wprintf(L"restored the session's input method (hr=%#lx)\n", hr);
    FreeLibrary(dll);
    return ok_profile == total && ok_api == total && ahk_ok == ahk_total && stuck_ok == stuck_total
               ? 0
               : 2;
}

// --typing: key sequences in each mode, checked against the text that lands in the Edit control.
// A sequence is a list of tokens: a letter/digit/symbol is that JIS key by scan code ("S-" prefix
// = with Shift), and names in braces are special keys.
constexpr WORD kCheckWindow = 0xFFFF;

struct Tok {
    WORD vk;      // for special keys; 0 for scan-code keys; kCheckWindow for {WIN}
    WORD scan;
    bool shift;
    bool alt = false;
};

std::vector<Tok> Keys(const wchar_t* seq) {
    struct Named {
        const wchar_t* name;
        WORD vk, scan;
    };
    static const Named kNamed[] = {
        {L"F6", VK_F6, 0x40},   {L"F7", VK_F7, 0x41},     {L"F8", VK_F8, 0x42},
        {L"F16", VK_F16, 0x67}, {L"F17", VK_F17, 0x68},   {L"F18", VK_F18, 0x69},
        {L"F19", VK_F19, 0x6a}, {L"F20", VK_F20, 0x6b},   {L"BS", VK_BACK, 0x0e},
        {L"ESC", VK_ESCAPE, 0x01}, {L"ENTER", VK_RETURN, 0x1c}, {L"SP", VK_SPACE, 0x39},
        {L"RO", 0, 0x73},       {L"YEN", 0, 0x7d},
        {L"LEFT", VK_LEFT, 0x4b}, {L"RIGHT", VK_RIGHT, 0x4d}, {L"DOWN", VK_DOWN, 0x50},
        {L"WIN", kCheckWindow, 0},
        // Escape-hatch keys (options CycleKeys, F17Keys). GRAVE is the key left of 1.
        {L"APPS", VK_APPS, 0x5d}, {L"GRAVE", 0, 0x29},
    };
    // JIS scan codes of the characters on the unshifted keys.
    static const struct {
        wchar_t ch;
        WORD scan;
    } kChars[] = {
        {L'1', 0x02}, {L'2', 0x03}, {L'3', 0x04}, {L'4', 0x05}, {L'5', 0x06}, {L'6', 0x07},
        {L'7', 0x08}, {L'8', 0x09}, {L'9', 0x0a}, {L'0', 0x0b}, {L'-', 0x0c}, {L'^', 0x0d},
        {L'q', 0x10}, {L'w', 0x11}, {L'e', 0x12}, {L'r', 0x13}, {L't', 0x14}, {L'y', 0x15},
        {L'u', 0x16}, {L'i', 0x17}, {L'o', 0x18}, {L'p', 0x19}, {L'@', 0x1a}, {L'[', 0x1b},
        {L'a', 0x1e}, {L's', 0x1f}, {L'd', 0x20}, {L'f', 0x21}, {L'g', 0x22}, {L'h', 0x23},
        {L'j', 0x24}, {L'k', 0x25}, {L'l', 0x26}, {L';', 0x27}, {L':', 0x28}, {L']', 0x2b},
        {L'z', 0x2c}, {L'x', 0x2d}, {L'c', 0x2e}, {L'v', 0x2f}, {L'b', 0x30}, {L'n', 0x31},
        {L'm', 0x32}, {L',', 0x33}, {L'.', 0x34}, {L'/', 0x35},
    };
    std::vector<Tok> out;
    for (const wchar_t* c = seq; *c;) {
        bool shift = false, alt = false;
        if (c[0] == L'A' && c[1] == L'-') {  // A- is Alt; it comes before S-
            alt = true;
            c += 2;
        }
        if (c[0] == L'S' && c[1] == L'-') {
            shift = true;
            c += 2;
        }
        if (*c == L'{') {
            const wchar_t* end = wcschr(c, L'}');
            const std::wstring name(c + 1, end);
            for (const Named& n : kNamed) {
                if (name == n.name) out.push_back({n.vk, n.scan, shift, alt});
            }
            c = end + 1;
            continue;
        }
        for (const auto& k : kChars) {
            if (k.ch == *c) out.push_back({0, k.scan, shift, alt});
        }
        ++c;
    }
    return out;
}

std::wstring Escaped(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        if (c >= 0x20 && c < 0x7f) {
            out += c;
        } else {
            wchar_t buf[8];
            swprintf_s(buf, L"\\u%04X", c);
            out += buf;
        }
    }
    return out;
}

int Typing(const wchar_t* only) {  // only: run cases whose name starts with it
    struct Case {
        const wchar_t* name;
        const wchar_t* keys;
        const wchar_t* expected;
    };
    const Case cases[] = {
        {L"EN JIS symbols", L"{F19}S-2{RO}S-{RO}{YEN}S-{YEN}@[:aS-a1S-7S-^", L"\"\\_\u00A5|@[:aA1'~"},
        {L"EN transient Caps Lock", L"{F19}{F17}ab{F17}c{F17}d{F19}e", L"ABcDe"},
        {L"EN backspace", L"{F19}hu{BS}x", L"hx"},
        {L"EN backspace after symbols", L"{F19}S-2a{BS}{BS}b", L"b"},
        {L"KO 2-Set + JIS symbol", L"{F18}dkssudgktpdyS-2{SP}rkrk{ENTER}", L"\uC548\uB155\uD558\uC138\uC694\" \uAC00\uAC00"},
        {L"KO backspace by jamo", L"{F18}rhk{BS}{BS}l{ENTER}", L"\uAE30"},
        {L"KO Shift inside a syllable", L"{F18}dlS-t{ENTER}", L"\uC788"},
        {L"KO tense, no double final, compound vowel", L"{F18}S-qkfmsS-rdhk{ENTER}",
         L"\uBE60\uB978\u3132\uC640"},
        {L"KO commit on F-key", L"{F18}rk{F19}a", L"\uAC00a"},
        {L"KO commit on F-key, nothing after", L"{F18}rk{F19}", L"\uAC00"},
        {L"KO commit on Enter, then F-key", L"{F18}rk{ENTER}{F19}a", L"\uAC00a"},
        {L"KO commit on F-key to Japanese", L"{F18}rk{F20}3{ENTER}", L"\uAC00\u3042"},
        {L"JA kana + dakuten", L"{F20}3t@{ENTER}", L"\u3042\u304C"},
        {L"JA F7 katakana", L"{F20}3e{F7}{ENTER}", L"\u30A2\u30A4"},
        {L"JA F8 half-width", L"{F20}t@{F8}{ENTER}", L"\uFF76\uFF9E"},
        {L"JA F7 then F6", L"{F20}3{F7}{F6}{ENTER}", L"\u3042"},
        {L"JA typing after F7 commits it", L"{F20}3{F7}e{ENTER}", L"\u30A2\u3044"},
        {L"JA backspace", L"{F20}3e{BS}{ENTER}", L"\u3042"},
        {L"JA escape cancels", L"{F20}3e{ESC}{F19}a", L"a"},
        {L"JA small tsu", L"{F20}S-z{ENTER}", L"\u3063"},
        {L"JA commit on F-key", L"{F20}3{F18}rk{ENTER}", L"\u3042\uAC00"},
        // The same with every key queued at once (the fastest possible typing).
        {L"FAST KO commit on F-key", L"{F18}rk{F19}a", L"\uAC00a"},
        {L"FAST KO commit on F-key to Japanese", L"{F18}rk{F20}3{ENTER}", L"\uAC00\u3042"},
        {L"FAST JA commit on F-key", L"{F20}3{F18}rk{ENTER}", L"\u3042\uAC00"},
        {L"FAST KO words", L"{F18}dkssudgktpdy{ENTER}", L"\uC548\uB155\uD558\uC138\uC694"},
        {L"FAST EN symbols", L"{F19}S-2{RO}S-{RO}{YEN}a", L"\"\\_\u00A5a"},
        {L"FAST JA kana", L"{F20}3t@{F7}{ENTER}", L"\u30A2\u30AC"},
        // Kanji conversion (bundled Mozc server). First candidates or cancel only: Mozc learns
        // what's chosen, which would change the order on the next run.
        {L"CONV first candidate", L"{F20}i-y{SP}{WIN}{ENTER}{WIN}", L"\u65E5\u672C[-][-]"},
        {L"CONV sentence, Shift inside, two segments", L"{F20}0qdft@hpew@r{SP}{RIGHT}{ENTER}",
         L"\u79C1\u306F\u5B66\u751F\u3067\u3059"},
        {L"CONV Shift doesn't split the kana", L"{F20}3S-93{ENTER}", L"\u3042\u3087\u3042"},
        {L"CONV candidate window, escape back to kana", L"{F20}i-y{SP}{SP}{WIN}{ESC}{WIN}{ENTER}",
         L"\u306B\u307B\u3093[W][-]"},
        {L"CONV backspace back to kana, then edit", L"{F20}i-y{SP}{BS}{BS}{ENTER}", L"\u306B\u307B"},
        {L"CONV F7 katakana", L"{F20}wrs{SP}{F7}{ENTER}", L"\u30C6\u30B9\u30C8"},
        {L"CONV typing on accepts", L"{F20}i-y{SP}3{ENTER}", L"\u65E5\u672C\u3042"},
        {L"CONV F-key accepts", L"{F20}i-y{SP}{F19}a", L"\u65E5\u672Ca"},
        {L"FAST CONV first candidate", L"{F20}i-y{SP}{ENTER}", L"\u65E5\u672C"},
        // Escape hatch (spec section 8). Need CycleKeys=3 and F17Keys=1; run them with a name
        // filter: --typing COMBO. Never inject Win keys here: they reach the shell.
        {L"COMBO cycle with Alt+`", L"{F19}aA-{GRAVE}aA-{GRAVE}aA-{GRAVE}a", L"a\u3141\u3061a"},
        {L"COMBO cycle with Menu", L"{F19}a{APPS}a{APPS}a{APPS}a", L"a\u3141\u3061a"},
        {L"COMBO Alt+Shift+` is F17", L"{F19}A-S-{GRAVE}abA-S-{GRAVE}c", L"ABc"},
        {L"COMBO ANSI ` is a long vowel in kana", L"{F20}{GRAVE}{ENTER}", L"ー"},
        {L"COMBO cycle ends a composition", L"{F18}rkA-{GRAVE}3{ENTER}", L"\uAC00\u3042"},
    };
    // A tap key that leaks to the app may open a menu, whose modal loop would stall this thread:
    // close any popup menu (class #32768) and report it.
    static bool menu_seen = false;
    CreateThread(nullptr, 0, [](void*) -> DWORD {
        for (;;) {
            Sleep(300);
            HWND menu = FindWindowW(L"#32768", nullptr);
            if (menu && IsWindowVisible(menu)) {
                menu_seen = true;
                SendKey(VK_ESCAPE, 0x01);
            }
        }
    }, nullptr, 0, nullptr);
    int failed = 0;
    for (const Case& c : cases) {
        menu_seen = false;
        if (only && wcsncmp(c.name, only, wcslen(only))) continue;
        SetWindowTextW(g_edit, L"");
        Pump(100);
        const bool fast = !wcsncmp(c.name, L"FAST", 4);  // every key queued at once
        std::wstring marks;  // {WIN} results, appended to the text
        for (const Tok& t : Keys(c.keys)) {
            if (t.vk == kCheckWindow) {  // record whether the candidate window shows now
                if (!fast) Pump(150);
                HWND w = FindWindowW(L"FKeyIMECandidateWindow", nullptr);
                marks += w && IsWindowVisible(w) ? L"[W]" : L"[-]";
                continue;
            }
            const bool extended = t.vk == VK_LEFT || t.vk == VK_RIGHT || t.vk == VK_DOWN ||
                                  t.vk == VK_APPS;
            SendKey(t.vk, t.scan, t.shift, t.vk == 0, extended, t.alt);
            if (!fast) Pump(40);
        }
        Pump(250);
        wchar_t text[256] = L"";
        GetWindowTextW(g_edit, text, 256);
        const std::wstring got = text + marks + (menu_seen ? L"[menu]" : L"");
        const bool ok = got == c.expected;
        failed += !ok;
        std::wprintf(L"%s %s", ok ? L"PASS" : L"FAIL", c.name);
        if (!ok) {
            std::wprintf(L"\n     got      [%s]\n     expected [%s]", Escaped(got).c_str(),
                         Escaped(c.expected).c_str());
        }
        std::wprintf(L"\n");
    }
    // Leave Caps Lock off and English selected.
    if (GetKeyState(VK_CAPITAL) & 1) SendKey(VK_CAPITAL, 0x3a);
    SendKey(VK_F19, 0x6a);
    Pump(200);
    std::wprintf(L"%d of %zu cases failed\n", failed, std::size(cases));
    return failed ? 2 : 0;
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
    if (argc > 1 && !wcscmp(argv[1], L"--hanja-fg")) return HanjaForeground();
    if (argc > 1 && !wcscmp(argv[1], L"--keylog")) return KeyLog(argc > 2 ? _wtoi(argv[2]) : 30);
    if (argc > 2 && !wcscmp(argv[1], L"--type-fg")) {
        // Types a --typing token sequence into the foreground window, then prints its title (a
        // test page can mirror its text and key events there).
        for (const Tok& t : Keys(argv[2])) {
            SendKey(t.vk, t.scan, t.shift, t.vk == 0,
                    t.vk == VK_LEFT || t.vk == VK_RIGHT || t.vk == VK_DOWN);
            Pump(80);
        }
        Pump(500);
        wchar_t title[512] = L"";
        GetWindowTextW(GetForegroundWindow(), title, 512);
        std::wprintf(L"title: [%s]\n", Escaped(title).c_str());
        return 0;
    }
    if (argc > 1 && !wcscmp(argv[1], L"--passive")) {
        // A plain window that never picks an input method itself, like an ordinary app: prints
        // which one its thread has, once a second, so session-wide switches can be watched.
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        HWND frame = CreateWindowExW(0, L"STATIC", L"FKeyIME passive probe",
                                     WS_OVERLAPPEDWINDOW | WS_VISIBLE, 820, 200, 400, 120, nullptr,
                                     nullptr, nullptr, nullptr);
        g_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 10, 360, 30,
                                 frame, nullptr, nullptr, nullptr);
        SetFocus(g_edit);
        const int seconds = argc > 2 ? _wtoi(argv[2]) : 10;
        if (argc > 3 && !wcscmp(argv[3], L"ms")) {  // start like a window stuck on Microsoft's IME
            TF_INPUTPROCESSORPROFILE ms = {};
            ms.dwProfileType = TF_PROFILETYPE_INPUTPROCESSOR;
            ms.langid = 0x0412;
            ms.clsid = {0xa028ae76, 0x01b1, 0x46c2, {0x99, 0xc4, 0xac, 0xd9, 0x85, 0x8a, 0xe0, 0x2f}};
            ms.guidProfile = {0xb5fe1f02, 0xd5f2, 0x4445, {0x9c, 0x03, 0xc5, 0x68, 0xf2, 0x3c, 0x99, 0xa1}};
            std::wprintf(L"start on Microsoft Korean: hr=%#lx\n", Activate(ms, TF_IPPMF_FORPROCESS));
        }
        std::wprintf(L"hwnd %p\n", g_edit);
        for (int i = 0; i < seconds; ++i) {
            Pump(1000);
            const TF_INPUTPROCESSORPROFILE p = ActiveProfile();
            std::wprintf(L"%2d s: lang %04x fkeyime=%d hkl=%08IX\n", i + 1, p.langid,
                         IsEqualGUID(p.clsid, fkeyime::kClsidTextService) != FALSE,
                         reinterpret_cast<ULONG_PTR>(GetKeyboardLayout(0)));
        }
        DestroyWindow(frame);
        return 0;
    }
    if (argc > 1 && !wcscmp(argv[1], L"--session-ms")) {  // whole session to Microsoft Korean IME
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        TF_INPUTPROCESSORPROFILE ms = {};
        ms.dwProfileType = TF_PROFILETYPE_INPUTPROCESSOR;
        ms.langid = 0x0412;
        ms.clsid = {0xa028ae76, 0x01b1, 0x46c2, {0x99, 0xc4, 0xac, 0xd9, 0x85, 0x8a, 0xe0, 0x2f}};
        ms.guidProfile = {0xb5fe1f02, 0xd5f2, 0x4445, {0x9c, 0x03, 0xc5, 0x68, 0xf2, 0x3c, 0x99, 0xa1}};
        std::wprintf(L"FORSESSION Microsoft Korean: hr=%#lx\n",
                     Activate(ms, TF_IPPMF_FORSESSION | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE));
        return 0;
    }
    if (argc > 1 && !wcscmp(argv[1], L"--active-loop")) {  // a long-lived thread's view, like AHK
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const int seconds = argc > 2 ? _wtoi(argv[2]) : 10;
        for (int i = 0; i < seconds; ++i) {
            Pump(1000);
            const TF_INPUTPROCESSORPROFILE p = ActiveProfile();
            std::wprintf(L"%2d s (no window): lang %04x fkeyime=%d\n", i + 1, p.langid,
                         IsEqualGUID(p.clsid, fkeyime::kClsidTextService) != FALSE);
        }
        return 0;
    }
    if (argc > 1 && !wcscmp(argv[1], L"--session-ko")) {  // switch the whole session to FKeyIME KO
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        TF_INPUTPROCESSORPROFILE p = {};
        p.dwProfileType = TF_PROFILETYPE_INPUTPROCESSOR;
        p.langid = fkeyime::kLangIdKorean;
        p.clsid = fkeyime::kClsidTextService;
        p.guidProfile = fkeyime::kProfileKorean;
        std::wprintf(L"FORSESSION FKeyIME KO: hr=%#lx\n",
                     Activate(p, TF_IPPMF_FORSESSION | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE));
        return 0;
    }
    if (argc > 1 && !wcscmp(argv[1], L"--active")) {  // the session's current input method
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const TF_INPUTPROCESSORPROFILE p = ActiveProfile();
        wchar_t clsid[40] = L"", profile[40] = L"";
        StringFromGUID2(p.clsid, clsid, 40);
        StringFromGUID2(p.guidProfile, profile, 40);
        std::wprintf(L"lang %04x clsid %s profile %s fkeyime=%d\n", p.langid, clsid, profile,
                     IsEqualGUID(p.clsid, fkeyime::kClsidTextService) != FALSE);
        return 0;
    }
    const bool handoff = argc > 1 && !wcscmp(argv[1], L"--handoff");
    // --typing-rich: the same cases in a TSF-aware RichEdit instead of the IMM-based Edit.
    const bool rich = argc > 1 && (!wcscmp(argv[1], L"--typing-rich") || !wcscmp(argv[1], L"--hanja-rich"));
    const bool typing = rich || (argc > 1 && !wcscmp(argv[1], L"--typing"));
    const bool hanja = argc > 1 && (!wcscmp(argv[1], L"--hanja") || !wcscmp(argv[1], L"--hanja-rich"));
    const int rounds =
        handoff ? (argc > 2 ? _wtoi(argv[2]) : 20) : (argc > 1 ? _wtoi(argv[1]) : 20);
    HWND frame = CreateWindowExW(0, L"STATIC", L"FKeyIME probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 200, 200, 600, 200, nullptr, nullptr, nullptr, nullptr);
    if (rich) LoadLibraryW(L"msftedit.dll");
    g_edit = CreateWindowExW(0, rich ? MSFTEDIT_CLASS : L"EDIT", L"",
                             WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 10, 560, 30, frame, nullptr,
                             nullptr, nullptr);
    if (rich) SendMessageW(g_edit, EM_SETEDITSTYLE, SES_USECTF, SES_USECTF);
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
    if (hanja) {
        const int result = Hanja();
        DestroyWindow(frame);
        return result;
    }
    if (handoff || typing) {
        const int result = handoff ? Handoff(rounds) : Typing(argc > 2 ? argv[2] : nullptr);
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
