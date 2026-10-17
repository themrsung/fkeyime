#include "edit_session.h"

#include <wrl/client.h>

#include <new>

#include "dll.h"
#include "log.h"

using Microsoft::WRL::ComPtr;

namespace fkeyime {
namespace {

class EditSession final : public ITfEditSession {
public:
    explicit EditSession(std::function<HRESULT(TfEditCookie)> body) : body_(std::move(body)) {
        DllAddRef();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_INVALIDARG;
        if (riid == IID_IUnknown || riid == IID_ITfEditSession) {
            *object = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override {
        const LONG refs = InterlockedDecrement(&refs_);
        if (!refs) delete this;
        return refs;
    }
    STDMETHODIMP DoEditSession(TfEditCookie ec) override { return body_(ec); }

private:
    ~EditSession() { DllRelease(); }
    LONG refs_ = 1;
    std::function<HRESULT(TfEditCookie)> body_;
};

}  // namespace

HRESULT RunEditSession(ITfContext* context, TfClientId client_id,
                       std::function<HRESULT(TfEditCookie)> body) {
    if (!context) return E_INVALIDARG;
    ComPtr<EditSession> session;
    session.Attach(new (std::nothrow) EditSession(std::move(body)));
    if (!session) return E_OUTOFMEMORY;
    HRESULT session_hr = E_FAIL;
    HRESULT hr = context->RequestEditSession(client_id, session.Get(),
                                             TF_ES_SYNC | TF_ES_READWRITE, &session_hr);
    if (SUCCEEDED(hr) && session_hr == TF_E_SYNCHRONOUS) {
        log::Write(L"edit session: synchronous refused, queued instead");
        hr = context->RequestEditSession(client_id, session.Get(),
                                         TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &session_hr);
    }
    return FAILED(hr) ? hr : session_hr;
}

HRESULT CaretToEnd(TfEditCookie ec, ITfContext* context, ITfRange* range) {
    ComPtr<ITfRange> caret;
    HRESULT hr = range->Clone(&caret);
    if (FAILED(hr)) return hr;
    caret->Collapse(ec, TF_ANCHOR_END);
    TF_SELECTION selection = {};
    selection.range = caret.Get();
    selection.style.ase = TF_AE_NONE;
    selection.style.fInterimChar = FALSE;
    return context->SetSelection(ec, 1, &selection);
}

HRESULT InsertAtSelection(TfEditCookie ec, ITfContext* context, std::wstring_view text,
                          ITfRange** inserted) {
    ComPtr<ITfInsertAtSelection> insert;
    HRESULT hr = context->QueryInterface(IID_PPV_ARGS(&insert));
    if (FAILED(hr)) return hr;
    ComPtr<ITfRange> range;
    hr = insert->InsertTextAtSelection(ec, 0, text.data(), static_cast<LONG>(text.size()), &range);
    if (FAILED(hr)) return hr;
    CaretToEnd(ec, context, range.Get());
    if (inserted) *inserted = range.Detach();
    return S_OK;
}

}  // namespace fkeyime
