#include "text_service.h"

#include <cwchar>
#include <new>

#include "display_attribute.h"
#include "dll.h"
#include "edit_session.h"
#include "guids.h"
#include "jis_layout.h"
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

constexpr UINT kMessageApply = WM_APP + 1;  // also the timer id
constexpr UINT kSwitchAfterCommitMs = 30;
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

// COM identity: two interface pointers may refer to the same context.
bool SameObject(IUnknown* a, IUnknown* b) {
    ComPtr<IUnknown> x, y;
    if (!a || !b || FAILED(a->QueryInterface(IID_PPV_ARGS(&x))) ||
        FAILED(b->QueryInterface(IID_PPV_ARGS(&y)))) {
        return false;
    }
    return x == y;
}

bool Down(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }

// Shortcut modifiers: with these held, keys belong to the app (Ctrl+C, Alt+F4, Win+...).
bool ShortcutModifiers() {
    return Down(VK_CONTROL) || Down(VK_MENU) || Down(VK_LWIN) || Down(VK_RWIN);
}

bool HasModifiers() { return Down(VK_SHIFT) || ShortcutModifiers(); }

const OurKey* FindFKey(WPARAM vk) {
    for (const OurKey& k : kOurKeys) {
        if (k.vk == vk) return &k;
    }
    return nullptr;
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

// The real Caps Lock, so the LED and other apps agree with FKeyIME's transient Caps Lock.
void SetCapsLock(bool on) {
    if (((GetKeyState(VK_CAPITAL) & 1) != 0) == on) return;
    INPUT in[2] = {{INPUT_KEYBOARD}, {INPUT_KEYBOARD}};
    in[0].ki.wVk = in[1].ki.wVk = VK_CAPITAL;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

// The Mozc key for a key pressed while converting (Mozc's MS-IME keymap), or kNone.
mozc::SpecialKey MozcKey(WPARAM vk) {
    using K = mozc::SpecialKey;
    switch (vk) {
        case VK_SPACE: return K::kSpace;
        case VK_RETURN: return K::kEnter;
        case VK_ESCAPE: return K::kEscape;
        case VK_BACK: return K::kBackspace;
        case VK_DELETE: return K::kDelete;
        case VK_LEFT: return K::kLeft;
        case VK_RIGHT: return K::kRight;
        case VK_UP: return K::kUp;
        case VK_DOWN: return K::kDown;
        case VK_HOME: return K::kHome;
        case VK_END: return K::kEnd;
        case VK_TAB: return K::kTab;
        case VK_PRIOR: return K::kPageUp;
        case VK_NEXT: return K::kPageDown;
        case VK_F6: return K::kF6;
        case VK_F7: return K::kF7;
        case VK_F8: return K::kF8;
        case VK_F9: return K::kF9;
        case VK_F10: return K::kF10;
    }
    return K::kNone;
}

std::wstring_view Wide(const std::u16string& s) {
    return {reinterpret_cast<const wchar_t*>(s.data()), s.size()};
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
    if (message == WM_TIMER && wparam == kMessageApply) {
        KillTimer(window, kMessageApply);
        message = kMessageApply;
    }
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
    } else if (riid == IID_ITfCompositionSink) {
        *object = static_cast<ITfCompositionSink*>(this);
    } else if (riid == IID_ITfDisplayAttributeProvider) {
        *object = static_cast<ITfDisplayAttributeProvider*>(this);
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

    ComPtr<ITfCategoryMgr> categories;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&categories)))) {
        categories->RegisterGUID(kDisplayAttributeInput, &input_attribute_);
        categories->RegisterGUID(kDisplayAttributeTarget, &target_attribute_);
        categories->RegisterGUID(kDisplayAttributeConverted, &converted_attribute_);
    }

    ComPtr<ITfKeystrokeMgr> keystrokes;
    if (SUCCEEDED(thread_mgr_.As(&keystrokes))) {
        HRESULT hr = keystrokes->AdviseKeyEventSink(client_id_, this, TRUE);
        if (FAILED(hr)) log::Write(L"AdviseKeyEventSink failed %#lx", hr);
    }
    PreserveFKeys(preserve_keys_);
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
    // Switching to another input method keeps what's being composed.
    if (composition_context_) CommitAll(composition_context_.Get());
    hangul_.Flush();
    kana_.Flush();
    converting_ = false;
    candidates_.Hide();
    mozc_.reset();  // ends the Mozc session
    if (state::Table* table = state::Shared()) state::Release(table, GetCurrentThreadId());
    if (message_window_) {
        DestroyWindow(message_window_);
        message_window_ = nullptr;
    }
    if (thread_mgr_) {
        ComPtr<ITfKeystrokeMgr> keystrokes;
        if (SUCCEEDED(thread_mgr_.As(&keystrokes))) {
            PreserveFKeys(false);
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

STDMETHODIMP TextService::OnSetFocus(BOOL foreground) {
    log::Write(L"OnSetFocus %d", foreground);
    return S_OK;
}

JapaneseInput TextService::JapaneseInputNow() const {
    if (const state::Table* table = state::Shared()) {
        return (state::SessionFlags(table) & state::kFlagRomaji) ? JapaneseInput::kRomaji
                                                                 : JapaneseInput::kKana;
    }
    return state_.japanese_input;
}

TextService::Plan TextService::PlanKey(WPARAM vk, LPARAM lparam) const {
    const bool composing = Composing();
    const Plan pass = {composing ? Action::kCommitThenPass : Action::kPass};

    // A modifier on its own (Shift for ㅃ or ょ, ...) belongs to the next key: never commit for
    // it. Committing here split compositions at every Shift (measured: とうき|ょうにいく).
    switch (vk) {
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: case VK_CONTROL: case VK_LCONTROL:
        case VK_RCONTROL: case VK_MENU: case VK_LMENU: case VK_RMENU: case VK_LWIN: case VK_RWIN:
        case VK_CAPITAL:
            return {Action::kPass};
    }
    if (FindFKey(vk)) return HasModifiers() ? pass : Plan{Action::kFKey};
    if (ShortcutModifiers()) return pass;

    const auto scan = static_cast<std::uint8_t>((lparam >> 16) & 0xFF);
    const bool extended = (lparam & (1 << 24)) != 0;
    const bool shift = Down(VK_SHIFT);

    if (converting_) {
        if (MozcKey(vk) != mozc::SpecialKey::kNone) return {Action::kConvert};
        // Typing on accepts the conversion and starts new kana.
        if (char16_t kana = extended ? char16_t{} : jis::Kana(scan, shift)) return {Action::kKana, kana};
        return pass;
    }
    if (extended) return pass;  // arrows, Home/End, numpad Enter, ...

    if (state_.mode == Mode::kKorean) {
        if (char16_t jamo = jis::TwoSetJamo(scan, shift)) return {Action::kJamo, jamo};
        if (char16_t ch = jis::Ascii(scan, shift)) return {Action::kInsert, ch};
        if (vk == VK_BACK && !hangul_.Empty()) return {Action::kEdit};
        return pass;
    }
    if (state_.mode == Mode::kJapanese && JapaneseInputNow() == JapaneseInput::kKana) {
        if (char16_t kana = jis::Kana(scan, shift)) return {Action::kKana, kana};
        if (!kana_.Empty()) {
            switch (vk) {
                case VK_BACK: case VK_ESCAPE: case VK_RETURN: case VK_SPACE:
                case VK_F6: case VK_F7: case VK_F8:
                    return {Action::kEdit};
            }
        }
        return pass;
    }
    // English, and Japanese romaji until romaji input exists: the JIS table.
    const bool caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
    if (char16_t ch = jis::Ascii(scan, shift, caps)) return {Action::kInsert, ch};
    return pass;
}

STDMETHODIMP TextService::OnTestKeyDown(ITfContext*, WPARAM wparam, LPARAM lparam, BOOL* eaten) {
    // "Commit then pass" keys are reported as ours so OnKeyDown runs and can end the
    // composition; OnKeyDown then hands them on.
    *eaten = PlanKey(wparam, lparam).action != Action::kPass;
    return S_OK;
}

STDMETHODIMP TextService::OnKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam,
                                    BOOL* eaten) {
    // A key typed before the timer of a delayed switch fires: switch now, before composing in
    // the new mode (WM_TIMER waits while keys are queued).
    ApplyPendingSwitch();
    const Plan plan = PlanKey(wparam, lparam);
    *eaten = TRUE;
    switch (plan.action) {
        case Action::kPass:
            *eaten = FALSE;
            break;
        case Action::kCommitThenPass:
            CommitAll(context);
            *eaten = FALSE;
            break;
        case Action::kInsert: {
            std::u16string text = hangul_.Flush() + kana_.Flush();
            text += plan.ch;
            UpdateText(context, text, {});
            break;
        }
        case Action::kJamo: {
            const std::u16string commit = hangul_.Feed(plan.ch);
            UpdateText(context, commit, hangul_.Preedit());
            break;
        }
        case Action::kKana: {
            std::u16string commit = EndConversion();
            commit += kana_.Input(plan.ch);
            UpdateText(context, commit, kana_.Preedit());
            break;
        }
        case Action::kEdit:
            HandleEdit(context, wparam);
            break;
        case Action::kConvert:
            HandleConversionKey(context, wparam);
            break;
        case Action::kFKey:
            HandleFKey(context, wparam, L"OnKeyDown");
            break;
    }
    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyUp(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
    *eaten = FindFKey(wparam) && !HasModifiers();
    return S_OK;
}

STDMETHODIMP TextService::OnKeyUp(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
    // Eat F-key key-ups too, so the app never sees half a key press.
    *eaten = FindFKey(wparam) && !HasModifiers();
    return S_OK;
}

STDMETHODIMP TextService::OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) {
    *eaten = FALSE;
    for (const OurKey& k : kOurKeys) {
        if (IsEqualGUID(guid, k.preserved)) {
            *eaten = TRUE;
            HandleFKey(context, k.vk, L"OnPreservedKey");
        }
    }
    return S_OK;
}

void TextService::HandleEdit(ITfContext* context, WPARAM vk) {
    if (state_.mode == Mode::kKorean) {
        hangul_.Backspace();  // the only edit key in Korean
        UpdateText(context, {}, hangul_.Preedit());
        return;
    }
    switch (vk) {
        case VK_BACK: kana_.Backspace(); break;
        case VK_ESCAPE: kana_.Escape(); break;
        case VK_SPACE:
            if (StartConversion(context)) return;
            [[fallthrough]];  // no conversion server: Space accepts the kana as they are
        case VK_RETURN:
            UpdateText(context, kana_.Flush(), {});
            return;
        case VK_F6: kana_.Force(KanaForm::kHiragana); break;
        case VK_F7: kana_.Force(KanaForm::kKatakana); break;
        case VK_F8: kana_.Force(KanaForm::kHalfwidthKatakana); break;
    }
    UpdateText(context, {}, kana_.Preedit());
}

MozcClient* TextService::Mozc() {
    if (!mozc_) mozc_ = std::make_unique<MozcClient>(DefaultMozcServerPath(DllInstance()));
    return mozc_.get();
}

bool TextService::StartConversion(ITfContext* context) {
    MozcClient* m = Mozc();
    mozc::Output out;
    // Start from an empty Mozc composition, then hand it the hiragana and convert.
    if (!m->SendCommand(mozc::Command::kRevert, &out)) {
        log::Write(L"conversion server unreachable");
        return false;
    }
    mozc::KeyEvent text;
    text.key_string = mozc::ToUtf8(kana_.Hiragana());
    text.as_is = true;
    mozc::KeyEvent space;
    space.special = mozc::SpecialKey::kSpace;
    const bool sent = m->SendKey(text, &out) && m->SendKey(space, &out);
    if (!sent || !out.Converting()) {
        log::Write(L"conversion didn't start: sent=%d segments=%zu candidates=%d", sent,
                   out.segments.size(), out.has_candidates);
        m->SendCommand(mozc::Command::kRevert, &out);
        return false;
    }
    log::Write(L"conversion started: segments=%zu", out.segments.size());
    kana_.Flush();  // Mozc holds the text now
    converting_ = true;
    ShowConversion(context, out, {});
    return true;
}

void TextService::HandleConversionKey(ITfContext* context, WPARAM vk) {
    mozc::KeyEvent key;
    key.special = MozcKey(vk);
    if (Down(VK_SHIFT)) key.modifiers.push_back(mozc::kModShift);
    mozc::Output out;
    if (!Mozc()->SendKey(key, &out)) {
        // The server went away: keep what the conversion showed.
        converting_ = false;
        UpdateText(context, conversion_text_, {});
        return;
    }
    log::Write(L"conversion key vk=%#x consumed=%d converting=%d segments=%zu result=%d", vk,
               out.consumed, out.Converting(), out.segments.size(), out.has_result);
    std::u16string commit = out.has_result ? mozc::FromUtf8(out.result) : std::u16string();
    if (out.Converting()) {
        ShowConversion(context, out, std::move(commit));
    } else if (out.has_preedit && !out.segments.empty()) {
        // Backspace / Escape went back to plain kana: FKeyIME's kana composer takes over.
        converting_ = false;
        kana_.Reset(mozc::FromUtf8(out.PreeditText()));
        mozc::Output ignored;
        Mozc()->SendCommand(mozc::Command::kRevert, &ignored);
        UpdateText(context, commit, kana_.Preedit());
    } else {
        converting_ = false;  // committed (Enter) or emptied
        UpdateText(context, commit, {});
    }
}

void TextService::ShowConversion(ITfContext* context, const mozc::Output& out,
                                 std::u16string commit) {
    std::u16string preedit;
    std::vector<Span> spans;
    for (const mozc::Segment& s : out.segments) {
        const std::u16string text = mozc::FromUtf8(s.value);
        spans.push_back({preedit.size(), text.size(), s.annotation == mozc::Segment::kHighlight});
        preedit += text;
    }
    conversion_text_ = preedit;
    UpdateText(context, commit, preedit, std::move(spans),
               out.ShowsCandidates() ? std::optional<mozc::Output>(out) : std::nullopt);
}

std::u16string TextService::EndConversion() {
    if (!converting_) return {};
    converting_ = false;
    candidates_.Hide();
    mozc::Output out;
    if (Mozc()->SendCommand(mozc::Command::kSubmit, &out) && out.has_result) {
        return mozc::FromUtf8(out.result);
    }
    return conversion_text_;  // server unreachable: keep what was shown
}

void TextService::HandleFKey(ITfContext* context, WPARAM vk, const wchar_t* path) {
    key_down_us_ = log::NowUs();
    // Any F-key ends the composition first: the text stays as typed.
    const bool committed = Composing() || composition_;
    if (context) CommitAll(context);
    const FKey key = FindFKey(vk) ? FindFKey(vk)->key : FKey::kF17;
    const Result result = OnFKey(state_, key, false, policy_);
    log::Write(L"key F%u via %s tid=%lu mode %s -> %s", static_cast<unsigned>(vk - VK_F1 + 1),
               path, GetCurrentThreadId(), ModeName(state_.mode), ModeName(result.next.mode));
    state_ = result.next;
    // Kana vs romaji is global, like the mode: other windows read it from the shared table.
    if (state::Table* table = state::Shared()) {
        state::SetSessionFlags(table, state_.japanese_input == JapaneseInput::kRomaji
                                          ? state::kFlagRomaji : 0);
    }
    for (const Effect& effect : result.effects) {
        switch (effect.kind) {
            case Effect::Kind::kActivateProfile:
                // After a commit, switch the profile only once the app has processed the commit:
                // WM_TIMER comes after everything already queued. Switching at once (or after a
                // posted message) made the IMM compatibility layer deliver the Korean syllable a
                // second time (measured in a Win32 Edit: 가 F19 gave 가가). Keys typed meanwhile
                // already follow the new mode, which state_ holds.
                if (committed && message_window_) {
                    pending_mode_ = effect.mode;
                    switch_pending_ = true;
                    SetTimer(message_window_, kMessageApply, kSwitchAfterCommitMs, nullptr);
                } else if (defer_activation_ && message_window_) {
                    pending_mode_ = effect.mode;
                    PostMessageW(message_window_, kMessageApply, 0, 0);
                } else {
                    ActivateMode(effect.mode);
                }
                break;
            case Effect::Kind::kSetCapsLock:
                SetCapsLock(effect.caps_lock);
                break;
            case Effect::Kind::kSetJapaneseInput:  // state_ already holds it
            case Effect::Kind::kConvertHanja:      // not implemented yet
                break;
        }
    }
}

void TextService::PreserveFKeys(bool on) {
    if (on == keys_preserved_ || !thread_mgr_) return;
    ComPtr<ITfKeystrokeMgr> keystrokes;
    if (FAILED(thread_mgr_.As(&keystrokes))) return;
    for (const OurKey& k : kOurKeys) {
        const TF_PRESERVEDKEY key = {k.vk, 0};
        const HRESULT hr = on ? keystrokes->PreserveKey(client_id_, k.preserved, &key, L"", 0)
                              : keystrokes->UnpreserveKey(k.preserved, &key);
        if (FAILED(hr)) log::Write(L"%s F%u failed %#lx", on ? L"PreserveKey" : L"UnpreserveKey",
                                   k.vk - VK_F1 + 1, hr);
    }
    keys_preserved_ = on;
}

void TextService::ApplyPendingSwitch() {
    if (!switch_pending_) return;
    if (message_window_) KillTimer(message_window_, kMessageApply);
    ApplyDeferred();
}

void TextService::ApplyDeferred() {
    switch_pending_ = false;
    ActivateMode(pending_mode_);
}

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

void TextService::CommitAll(ITfContext* context) {
    const std::u16string text = EndConversion() + hangul_.Flush() + kana_.Flush();
    // Commit where the composition is, whichever pointer the caller got.
    ITfContext* target = composition_ && composition_context_ ? composition_context_.Get() : context;
    if (composition_ || !text.empty()) UpdateText(target, text, {});
}

void TextService::UpdateText(ITfContext* context, const std::u16string& commit,
                             const std::u16string& preedit, std::vector<Span> spans,
                             std::optional<mozc::Output> candidates) {
    if (!context) return;
    if (!candidates) candidates_.Hide();
    // A composition left in another document (focus moved) ends there as it is.
    if (composition_ && composition_context_ && !SameObject(composition_context_.Get(), context)) {
        ComPtr<ITfComposition> old = composition_;
        RunEditSession(composition_context_.Get(), client_id_, [old](TfEditCookie ec) {
            return old->EndComposition(ec);
        });
        composition_.Reset();
        composition_context_.Reset();
    }
    // Captures copies: when TSF can't run the session synchronously, it runs after this returns.
    ComPtr<ITfContext> context_ref = context;
    auto edit = [this, context_ref, commit, preedit, spans,
                 candidates](TfEditCookie ec) -> HRESULT {
        ITfContext* context = context_ref.Get();
        ComPtr<ITfProperty> attribute;
        context->GetProperty(GUID_PROP_ATTRIBUTE, &attribute);
        // Korean selects the composing syllable as an interim character (block cursor), like the
        // Microsoft Korean IME; with a plain caret, apps running through the IMM compatibility
        // layer end every Korean composition at once (measured in a Win32 Edit control). Other
        // modes put the caret after the composition.
        auto ShowComposing = [&](TfEditCookie cookie, ITfRange* range) -> HRESULT {
            if (state_.mode != Mode::kKorean) return CaretToEnd(cookie, context, range);
            TF_SELECTION selection = {};
            selection.range = range;
            selection.style.ase = TF_AE_NONE;
            selection.style.fInterimChar = TRUE;
            return context->SetSelection(cookie, 1, &selection);
        };
        // The part of `range` from `start` for `length` UTF-16 units.
        auto sub = [&](ITfRange* range, size_t start, size_t length) {
            ComPtr<ITfRange> r;
            LONG moved = 0;
            if (FAILED(range->Clone(&r))) return r;
            r->Collapse(ec, TF_ANCHOR_START);
            r->ShiftEnd(ec, static_cast<LONG>(start + length), &moved, nullptr);
            r->ShiftStart(ec, static_cast<LONG>(start), &moved, nullptr);
            return r;
        };
        auto set = [&](ITfRange* range, TfGuidAtom atom) {
            if (!attribute || atom == TF_INVALID_GUIDATOM) return;
            VARIANT v;
            v.vt = VT_I4;
            v.lVal = static_cast<LONG>(atom);
            attribute->SetValue(ec, range, &v);
        };
        // Composing text gets the input attribute; a conversion marks its target segment and
        // shows the candidate window under it.
        auto mark = [&](ITfRange* range) {
            if (spans.empty()) return set(range, input_attribute_);
            set(range, converted_attribute_);
            ComPtr<ITfRange> target;
            for (const Span& s : spans) {
                if (!s.target) continue;
                target = sub(range, s.start, s.length);
                if (target) set(target.Get(), target_attribute_);
            }
            if (!candidates) return;
            RECT rect = {};
            BOOL clipped = FALSE;
            ComPtr<ITfContextView> view;
            if (FAILED(context->GetActiveView(&view)) ||
                FAILED(view->GetTextExt(ec, target ? target.Get() : range, &rect, &clipped)) ||
                IsRectEmpty(&rect)) {
                GUITHREADINFO gui = {sizeof(gui)};  // fall back to the caret
                if (GetGUIThreadInfo(GetCurrentThreadId(), &gui) && gui.hwndCaret) {
                    rect = gui.rcCaret;
                    MapWindowPoints(gui.hwndCaret, nullptr, reinterpret_cast<POINT*>(&rect), 2);
                }
            }
            candidates_.Show(*candidates, rect);
        };

        if (composition_) {
            ComPtr<ITfRange> range;
            composition_->GetRange(&range);
            if (commit.empty() && !preedit.empty()) {  // still composing: just update it
                range->SetText(ec, 0, Wide(preedit).data(), static_cast<LONG>(preedit.size()));
                mark(range.Get());
                return ShowComposing(ec, range.Get());
            }
            // The composition ends; its text becomes `commit` (possibly nothing).
            if (attribute) attribute->Clear(ec, range.Get());
            range->SetText(ec, 0, Wide(commit).data(), static_cast<LONG>(commit.size()));
            CaretToEnd(ec, context, range.Get());
            composition_->EndComposition(ec);
            composition_.Reset();
            composition_context_.Reset();
        } else if (!commit.empty()) {
            InsertAtSelection(ec, context, Wide(commit));
        }
        if (preedit.empty()) return S_OK;

        ComPtr<ITfInsertAtSelection> insert;
        ComPtr<ITfContextComposition> compositions;
        ComPtr<ITfRange> at;
        if (FAILED(context->QueryInterface(IID_PPV_ARGS(&insert))) ||
            FAILED(context->QueryInterface(IID_PPV_ARGS(&compositions))) ||
            FAILED(insert->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr, 0, &at))) {
            return E_FAIL;
        }
        HRESULT hr = compositions->StartComposition(ec, at.Get(), this, &composition_);
        if (FAILED(hr) || !composition_) {
            // The app refused a composition: type the preedit as final text instead.
            composition_.Reset();
            return InsertAtSelection(ec, context, Wide(preedit));
        }
        composition_context_ = context;
        ComPtr<ITfRange> range;
        composition_->GetRange(&range);
        range->SetText(ec, 0, Wide(preedit).data(), static_cast<LONG>(preedit.size()));
        mark(range.Get());
        return ShowComposing(ec, range.Get());
    };
    const HRESULT session_hr = RunEditSession(context, client_id_, edit);
    if (FAILED(session_hr)) log::Write(L"UpdateText edit session failed %#lx", session_hr);
}

STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie, ITfComposition* composition) {
    // The app ended the composition (a click elsewhere, focus change...): its text stays, and
    // FKeyIME starts fresh.
    log::Write(L"OnCompositionTerminated ours=%d mode=%s", composition == composition_.Get(),
               ModeName(state_.mode));
    if (composition_ && composition == composition_.Get()) {
        composition_.Reset();
        composition_context_.Reset();
        hangul_.Flush();
        kana_.Flush();
        if (converting_) {
            converting_ = false;
            candidates_.Hide();
            mozc::Output out;
            if (mozc_) mozc_->SendCommand(mozc::Command::kRevert, &out);
        }
    }
    return S_OK;
}

STDMETHODIMP TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** infos) {
    if (!infos) return E_INVALIDARG;
    *infos = NewDisplayAttributeEnum();
    return *infos ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP TextService::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** info) {
    if (!info) return E_INVALIDARG;
    *info = NewDisplayAttributeInfo(guid);
    return *info ? S_OK : E_INVALIDARG;
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
