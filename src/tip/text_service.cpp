#include "text_service.h"

#include <cwchar>
#include <new>

#include "dll.h"
#include "guids.h"
#include "log.h"
#include "state_table.h"

using Microsoft::WRL::ComPtr;

namespace fkeyime {
namespace {

// The F-keys FKeyIME handles, with their preserved-key ids. The ids are runtime only (never
// stored), unlike the GUIDs in guids.h.
struct OurKey {
    UINT vk;
    FKey key;
    GUID preserved;
};
constexpr OurKey kOurKeys[] = {
    {VK_F16, FKey::kF16, {0x1492b3d0, 0x7f17, 0x4e95, {0xa3, 0xd3, 0x51, 0x80, 0x05, 0x43, 0xc3, 0x5c}}},
    {VK_F17, FKey::kF17, {0x08e4ae93, 0x17ac, 0x4146, {0xa9, 0xe4, 0x4a, 0xcd, 0x2f, 0x8f, 0x9b, 0xdc}}},
    {VK_F18, FKey::kF18, {0xcb893626, 0x692e, 0x4b0c, {0xa6, 0x2b, 0xfa, 0x80, 0x0e, 0x57, 0x4c, 0xda}}},
    {VK_F19, FKey::kF19, {0x3ec3ce66, 0x6001, 0x408d, {0xb5, 0xb6, 0xca, 0x27, 0x3d, 0x02, 0xcf, 0x83}}},
    {VK_F20, FKey::kF20, {0xff19845d, 0xc0a9, 0x4422, {0xbe, 0x90, 0xd5, 0x61, 0xda, 0x4b, 0x41, 0x1e}}},
};

constexpr UINT kMessageApply = WM_APP + 1;
constexpr wchar_t kWindowClass[] = L"FKeyIMEMessageWindow";

const wchar_t* ModeName(Mode mode) {
    switch (mode) {
        case Mode::kEnglish: return L"EN";
        case Mode::kKorean: return L"KO";
        case Mode::kJapanese: return L"JA";
    }
    return L"?";
}

const GUID& ProfileFor(Mode mode, LANGID* langid) {
    switch (mode) {
        case Mode::kKorean: *langid = kLangIdKorean; return kProfileKorean;
        case Mode::kJapanese: *langid = kLangIdJapanese; return kProfileJapanese;
        case Mode::kEnglish: break;
    }
    *langid = kLangIdEnglish;
    return kProfileEnglish;
}

bool ModeFor(REFGUID profile, Mode* mode) {
    if (IsEqualGUID(profile, kProfileEnglish)) *mode = Mode::kEnglish;
    else if (IsEqualGUID(profile, kProfileKorean)) *mode = Mode::kKorean;
    else if (IsEqualGUID(profile, kProfileJapanese)) *mode = Mode::kJapanese;
    else return false;
    return true;
}

bool HasModifiers() {
    for (int vk : {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN}) {
        if (GetKeyState(vk) & 0x8000) return true;
    }
    return false;
}

DWORD ReadOption(const wchar_t* name, DWORD fallback) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\fkeyime", name, RRF_RT_REG_DWORD, nullptr,
                     &value, &size) == ERROR_SUCCESS) {
        return value;
    }
    return fallback;
}

// What the thread's keyboard layout produces for the keys whose symbols differ between JIS and
// US: Shift+2, ろ, Shift+ろ, ¥, @ [ : (JIS) = [ ] ' (US). Logged on every profile change.
void LogLayoutProbe(const wchar_t* when) {
    struct Probe {
        UINT scan;
        bool shift;
    };
    constexpr Probe kProbes[] = {{0x03, true}, {0x73, false}, {0x73, true}, {0x7d, false},
                                 {0x1a, false}, {0x1b, false}, {0x28, false}};
    const HKL hkl = GetKeyboardLayout(0);
    wchar_t out[64] = L"";
    size_t at = 0;
    for (const Probe& p : kProbes) {
        BYTE keys[256] = {};
        if (p.shift) keys[VK_SHIFT] = 0x80;
        const UINT vk = MapVirtualKeyExW(p.scan, MAPVK_VSC_TO_VK_EX, hkl);
        wchar_t ch[4] = L"";
        // Flag 4: don't change the keyboard state (no dead-key side effects), Windows 10 1607+.
        const int n = vk ? ToUnicodeEx(vk, p.scan, keys, ch, 4, 4, hkl) : 0;
        out[at++] = n > 0 ? ch[0] : L'-';
        out[at++] = L' ';
    }
    out[at] = 0;
    log::Write(L"layout %s hkl=%08IX probe=[%s] (JIS: \" \\ _ ¥ @ [ :, US: @ - - - [ ] ')", when,
               reinterpret_cast<ULONG_PTR>(hkl), out);
}

LRESULT CALLBACK MessageWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == kMessageApply) {
        if (auto* service = reinterpret_cast<TextService*>(GetWindowLongPtrW(window, GWLP_USERDATA))) {
            service->ApplyDeferred();
        }
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

TextService::TextService()
    : activate_flags_(ReadOption(L"ActivateFlags", TF_IPPMF_FORSESSION |
                                                       TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE)),
      defer_activation_(ReadOption(L"Defer", 0) != 0),
      preserve_keys_(ReadOption(L"PreserveKeys", 1) != 0) {
    DllAddRef();
}

TextService::~TextService() { DllRelease(); }

STDMETHODIMP TextService::QueryInterface(REFIID riid, void** object) {
    if (!object) return E_INVALIDARG;
    if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor ||
        riid == IID_ITfTextInputProcessorEx) {
        *object = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (riid == IID_ITfKeyEventSink) {
        *object = static_cast<ITfKeyEventSink*>(this);
    } else if (riid == IID_ITfActiveLanguageProfileNotifySink) {
        *object = static_cast<ITfActiveLanguageProfileNotifySink*>(this);
    } else {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) TextService::AddRef() { return InterlockedIncrement(&refs_); }

STDMETHODIMP_(ULONG) TextService::Release() {
    const LONG refs = InterlockedDecrement(&refs_);
    if (!refs) delete this;
    return refs;
}

STDMETHODIMP TextService::Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) {
    return ActivateEx(thread_mgr, client_id, 0);
}

STDMETHODIMP TextService::ActivateEx(ITfThreadMgr* thread_mgr, TfClientId client_id,
                                     DWORD flags) {
    thread_mgr_ = thread_mgr;
    client_id_ = client_id;

    ComPtr<ITfInputProcessorProfileMgr> profiles;
    TF_INPUTPROCESSORPROFILE active = {};
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles))) &&
        SUCCEEDED(profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &active))) {
        ModeFor(active.guidProfile, &state_.mode);
    }
    // Publish "FKeyIME is active on this thread" for the public API (fkeyime_api.h).
    if (state::Table* table = state::Shared()) {
        if (!state::Claim(table, GetCurrentThreadId(), GetCurrentProcessId(),
                          static_cast<LONG>(state_.mode), log::NowUs())) {
            log::Write(L"state table full");
        }
    } else {
        log::Write(L"state table unavailable (%lu)", GetLastError());
    }
    log::Write(L"ActivateEx tid=%lu flags=%#lx mode=%s options: ActivateFlags=%#lx Defer=%d "
               L"PreserveKeys=%d",
               GetCurrentThreadId(), flags, ModeName(state_.mode), activate_flags_,
               defer_activation_, preserve_keys_);

    ComPtr<ITfKeystrokeMgr> keystrokes;
    if (SUCCEEDED(thread_mgr_.As(&keystrokes))) {
        HRESULT hr = keystrokes->AdviseKeyEventSink(client_id_, this, TRUE);
        if (FAILED(hr)) log::Write(L"AdviseKeyEventSink failed %#lx", hr);
        if (preserve_keys_) {
            for (const OurKey& k : kOurKeys) {
                const TF_PRESERVEDKEY key = {k.vk, 0};
                hr = keystrokes->PreserveKey(client_id_, k.preserved, &key, L"", 0);
                if (FAILED(hr)) log::Write(L"PreserveKey F%u failed %#lx", k.vk - VK_F1 + 1, hr);
            }
        }
    }
    ComPtr<ITfSource> source;
    if (SUCCEEDED(thread_mgr_.As(&source))) {
        source->AdviseSink(IID_ITfActiveLanguageProfileNotifySink,
                           static_cast<ITfActiveLanguageProfileNotifySink*>(this),
                           &profile_sink_cookie_);
    }

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = MessageWindowProc;
    wc.hInstance = DllInstance();
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);  // fails harmlessly when another thread registered it already
    message_window_ = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                      DllInstance(), nullptr);
    if (message_window_) {
        SetWindowLongPtrW(message_window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
    LogLayoutProbe(L"ActivateEx");
    return S_OK;
}

STDMETHODIMP TextService::Deactivate() {
    log::Write(L"Deactivate tid=%lu", GetCurrentThreadId());
    if (state::Table* table = state::Shared()) state::Release(table, GetCurrentThreadId());
    if (message_window_) {
        DestroyWindow(message_window_);
        message_window_ = nullptr;
    }
    if (thread_mgr_) {
        ComPtr<ITfKeystrokeMgr> keystrokes;
        if (SUCCEEDED(thread_mgr_.As(&keystrokes))) {
            if (preserve_keys_) {
                for (const OurKey& k : kOurKeys) {
                    const TF_PRESERVEDKEY key = {k.vk, 0};
                    keystrokes->UnpreserveKey(k.preserved, &key);
                }
            }
            keystrokes->UnadviseKeyEventSink(client_id_);
        }
        ComPtr<ITfSource> source;
        if (profile_sink_cookie_ != TF_INVALID_COOKIE && SUCCEEDED(thread_mgr_.As(&source))) {
            source->UnadviseSink(profile_sink_cookie_);
        }
    }
    profile_sink_cookie_ = TF_INVALID_COOKIE;
    thread_mgr_.Reset();
    client_id_ = TF_CLIENTID_NULL;
    return S_OK;
}

bool TextService::IsOurFKey(WPARAM vk) const {
    return vk >= VK_F16 && vk <= VK_F20 && !HasModifiers();
}

STDMETHODIMP TextService::OnSetFocus(BOOL foreground) {
    log::Write(L"OnSetFocus %d", foreground);
    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyDown(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
    *eaten = IsOurFKey(wparam);
    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyUp(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
    *eaten = IsOurFKey(wparam);
    return S_OK;
}

STDMETHODIMP TextService::OnKeyDown(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
    *eaten = IsOurFKey(wparam);
    if (*eaten) HandleFKey(wparam, L"OnKeyDown");
    return S_OK;
}

STDMETHODIMP TextService::OnKeyUp(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
    // Eat the key-up too, so the app never sees half a key press.
    *eaten = IsOurFKey(wparam);
    return S_OK;
}

STDMETHODIMP TextService::OnPreservedKey(ITfContext*, REFGUID guid, BOOL* eaten) {
    *eaten = FALSE;
    for (const OurKey& k : kOurKeys) {
        if (IsEqualGUID(guid, k.preserved)) {
            *eaten = TRUE;
            HandleFKey(k.vk, L"OnPreservedKey");
        }
    }
    return S_OK;
}

void TextService::HandleFKey(WPARAM vk, const wchar_t* path) {
    key_down_us_ = log::NowUs();
    FKey key = FKey::kF17;
    for (const OurKey& k : kOurKeys) {
        if (k.vk == vk) key = k.key;
    }
    const Result result = OnFKey(state_, key, false, policy_);
    log::Write(L"key F%u via %s tid=%lu mode %s -> %s", static_cast<unsigned>(vk - VK_F1 + 1),
               path, GetCurrentThreadId(), ModeName(state_.mode), ModeName(result.next.mode));
    state_ = result.next;
    for (const Effect& effect : result.effects) {
        switch (effect.kind) {
            case Effect::Kind::kActivateProfile:
                if (defer_activation_ && message_window_) {
                    pending_mode_ = effect.mode;
                    PostMessageW(message_window_, kMessageApply, 0, 0);
                } else {
                    ActivateMode(effect.mode);
                }
                break;
            case Effect::Kind::kSetCapsLock:
            case Effect::Kind::kSetJapaneseInput:
            case Effect::Kind::kConvertHanja:
                break;  // M1-M3
        }
    }
}

void TextService::ApplyDeferred() { ActivateMode(pending_mode_); }

void TextService::ActivateMode(Mode mode) {
    LANGID langid;
    const GUID& profile = ProfileFor(mode, &langid);
    ComPtr<ITfInputProcessorProfileMgr> profiles;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&profiles));
    const long long start = log::NowUs();
    if (SUCCEEDED(hr)) {
        // Always re-assert, even when this profile is already active (docs/spec.md, rules).
        hr = profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, langid, kClsidTextService,
                                       profile, nullptr, activate_flags_);
    }
    const long long end = log::NowUs();
    log::Write(L"ActivateProfile %s hr=%#lx call=%lldus since-key=%lldus", ModeName(mode), hr,
               end - start, end - key_down_us_);
}

STDMETHODIMP TextService::OnActivated(REFCLSID clsid, REFGUID profile, BOOL activated) {
    Mode mode = Mode::kEnglish;
    const bool ours = IsEqualGUID(clsid, kClsidTextService) && ModeFor(profile, &mode);
    if (ours && activated) {
        state_.mode = mode;
        if (state::Table* table = state::Shared()) {
            state::SetMode(table, GetCurrentThreadId(), static_cast<LONG>(mode), log::NowUs());
        }
    }
    log::Write(L"OnActivated %s %s since-key=%lldus", ours ? ModeName(mode) : L"other",
               activated ? L"on" : L"off", log::NowUs() - key_down_us_);
    if (activated) LogLayoutProbe(L"OnActivated");
    return S_OK;
}

}  // namespace fkeyime
