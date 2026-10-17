// The FKeyIME text service: one COM object per thread that has TSF, activated by TSF for any of
// the three FKeyIME profiles.
//
// F16-F20 run through the mode machine (src/core) and switch profiles with
// ITfInputProcessorProfileMgr::ActivateProfile. Text comes from FKeyIME's own tables in every
// mode (src/core/jis_layout.h): English inserts JIS characters, Korean composes 2-Set syllables,
// Japanese composes kana (F6-F8 force hiragana / katakana / half-width katakana). The diagnostic
// log (log.h) records F-keys and profile changes, never typed text.
#pragma once

#include <msctf.h>
#include <windows.h>
#include <wrl/client.h>

#include <string>

#include "hangul.h"
#include "kana.h"
#include "mode_machine.h"

namespace fkeyime {

class TextService final : public ITfTextInputProcessorEx,
                          public ITfKeyEventSink,
                          public ITfActiveLanguageProfileNotifySink,
                          public ITfCompositionSink,
                          public ITfDisplayAttributeProvider {
public:
    TextService();
    TextService(const TextService&) = delete;
    TextService& operator=(const TextService&) = delete;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor(Ex)
    STDMETHODIMP Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) override;
    STDMETHODIMP ActivateEx(ITfThreadMgr* thread_mgr, TfClientId client_id, DWORD flags) override;
    STDMETHODIMP Deactivate() override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL foreground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam,
                               BOOL* eaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* context, WPARAM wparam, LPARAM lparam,
                             BOOL* eaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) override;

    // ITfActiveLanguageProfileNotifySink
    STDMETHODIMP OnActivated(REFCLSID clsid, REFGUID profile, BOOL activated) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ec, ITfComposition* composition) override;

    // ITfDisplayAttributeProvider
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** infos) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** info) override;

    // Posted from the key handler when activation is deferred (spike option "Defer").
    void ApplyDeferred();
    void ApplyPendingSwitch();  // a profile switch delayed after a commit, if one is pending

private:
    ~TextService();

    // What a key press does in the current mode. Decided the same way in OnTestKeyDown (no side
    // effects) and OnKeyDown (acts on it).
    enum class Action {
        kPass,            // not ours
        kCommitThenPass,  // not ours, but the composition must end first
        kInsert,          // insert `ch` (ends any composition first)
        kJamo,            // Korean: feed `ch` to the Hangul composer
        kKana,            // Japanese: feed `ch` to the kana composer
        kEdit,            // Backspace / Escape / Enter / Space / F6-F8 inside a composition
        kFKey,            // F16-F20
    };
    struct Plan {
        Action action = Action::kPass;
        char16_t ch = 0;
    };
    Plan PlanKey(WPARAM vk, LPARAM lparam) const;
    JapaneseInput JapaneseInputNow() const;  // session-wide (state table), else this thread's
    bool Composing() const { return !hangul_.Empty() || !kana_.Empty(); }

    void HandleFKey(ITfContext* context, WPARAM vk, const wchar_t* path);
    void HandleEdit(ITfContext* context, WPARAM vk);
    void ActivateMode(Mode mode);
    void PreserveFKeys(bool on);  // F16-F20 as TSF preserved keys (spike option PreserveKeys)

    // Replaces the composition (if any) with `commit`, which becomes final text, then starts or
    // updates a composition showing `preedit`. Either may be empty.
    void UpdateText(ITfContext* context, const std::u16string& commit,
                    const std::u16string& preedit);
    // Ends the composition, keeping what's composed as final text.
    void CommitAll(ITfContext* context);

    LONG refs_ = 1;
    Microsoft::WRL::ComPtr<ITfThreadMgr> thread_mgr_;
    TfClientId client_id_ = TF_CLIENTID_NULL;
    DWORD profile_sink_cookie_ = TF_INVALID_COOKIE;
    HWND message_window_ = nullptr;
    TfGuidAtom input_attribute_ = TF_INVALID_GUIDATOM;

    State state_;
    Policy policy_;
    HangulComposer hangul_;
    KanaComposer kana_;
    Microsoft::WRL::ComPtr<ITfComposition> composition_;
    Microsoft::WRL::ComPtr<ITfContext> composition_context_;

    Mode pending_mode_ = Mode::kEnglish;
    bool switch_pending_ = false;
    long long key_down_us_ = 0;  // when the last F-key arrived, for the latency log

    // Spike options from HKLM\Software\fkeyime (tools/spike.ps1).
    DWORD activate_flags_;
    bool defer_activation_ = false;
    bool preserve_keys_ = true;
    bool keys_preserved_ = false;
};

}  // namespace fkeyime
