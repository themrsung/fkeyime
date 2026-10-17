// The FKeyIME text service: one COM object per thread that has TSF, activated by TSF for any of
// the three FKeyIME profiles.
//
// Milestone M0 spike: F17-F20 run through the mode machine (src/core) and F18/F19/F20 switch
// profiles with ITfInputProcessorProfileMgr::ActivateProfile. No text is composed yet. Every
// step is written to the diagnostic log (log.h) so latency and key delivery can be measured.
#pragma once

#include <msctf.h>
#include <windows.h>
#include <wrl/client.h>

#include "mode_machine.h"

namespace fkeyime {

class TextService final : public ITfTextInputProcessorEx,
                          public ITfKeyEventSink,
                          public ITfActiveLanguageProfileNotifySink {
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

    // Posted from the key handler when activation is deferred (spike option "Defer").
    void ApplyDeferred();

private:
    ~TextService();

    bool IsOurFKey(WPARAM vk) const;
    void HandleFKey(WPARAM vk, const wchar_t* path);
    void ActivateMode(Mode mode);

    LONG refs_ = 1;
    Microsoft::WRL::ComPtr<ITfThreadMgr> thread_mgr_;
    TfClientId client_id_ = TF_CLIENTID_NULL;
    DWORD profile_sink_cookie_ = TF_INVALID_COOKIE;
    HWND message_window_ = nullptr;

    State state_;
    Policy policy_;
    Mode pending_mode_ = Mode::kEnglish;
    long long key_down_us_ = 0;  // when the last F-key arrived, for the latency log

    // Spike options from HKLM\Software\fkeyime (tools/spike.ps1).
    DWORD activate_flags_;
    bool defer_activation_ = false;
    bool preserve_keys_ = true;
};

}  // namespace fkeyime
