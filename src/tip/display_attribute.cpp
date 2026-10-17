#include "display_attribute.h"

#include <new>

#include "dll.h"
#include "guids.h"

namespace fkeyime {
namespace {

class InputAttribute final : public ITfDisplayAttributeInfo {
public:
    InputAttribute() { DllAddRef(); }

    STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_INVALIDARG;
        if (riid == IID_IUnknown || riid == IID_ITfDisplayAttributeInfo) {
            *object = static_cast<ITfDisplayAttributeInfo*>(this);
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

    STDMETHODIMP GetGUID(GUID* guid) override {
        if (!guid) return E_INVALIDARG;
        *guid = kDisplayAttributeInput;
        return S_OK;
    }
    STDMETHODIMP GetDescription(BSTR* description) override {
        if (!description) return E_INVALIDARG;
        *description = SysAllocString(L"FKeyIME composing text");
        return *description ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* attribute) override {
        if (!attribute) return E_INVALIDARG;
        *attribute = {};
        attribute->crText.type = TF_CT_NONE;
        attribute->crBk.type = TF_CT_NONE;
        attribute->lsStyle = TF_LS_SOLID;
        attribute->fBoldLine = FALSE;
        attribute->crLine.type = TF_CT_NONE;  // underline in the text color
        attribute->bAttr = TF_ATTR_INPUT;
        return S_OK;
    }
    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) override { return E_NOTIMPL; }
    STDMETHODIMP Reset() override { return S_OK; }

private:
    ~InputAttribute() { DllRelease(); }
    LONG refs_ = 1;
};

class AttributeEnum final : public IEnumTfDisplayAttributeInfo {
public:
    explicit AttributeEnum(ULONG position = 0) : position_(position) { DllAddRef(); }

    STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_INVALIDARG;
        if (riid == IID_IUnknown || riid == IID_IEnumTfDisplayAttributeInfo) {
            *object = static_cast<IEnumTfDisplayAttributeInfo*>(this);
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

    STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** clone) override {
        if (!clone) return E_INVALIDARG;
        *clone = new (std::nothrow) AttributeEnum(position_);
        return *clone ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP Next(ULONG count, ITfDisplayAttributeInfo** infos, ULONG* fetched) override {
        if (!infos) return E_INVALIDARG;
        ULONG n = 0;
        while (n < count && position_ < kCount) {
            infos[n] = new (std::nothrow) InputAttribute();
            if (!infos[n]) break;
            ++n;
            ++position_;
        }
        if (fetched) *fetched = n;
        return n == count ? S_OK : S_FALSE;
    }
    STDMETHODIMP Reset() override {
        position_ = 0;
        return S_OK;
    }
    STDMETHODIMP Skip(ULONG count) override {
        position_ = position_ + count > kCount ? kCount : position_ + count;
        return position_ < kCount ? S_OK : S_FALSE;
    }

private:
    ~AttributeEnum() { DllRelease(); }
    static constexpr ULONG kCount = 1;
    LONG refs_ = 1;
    ULONG position_;
};

}  // namespace

IEnumTfDisplayAttributeInfo* NewDisplayAttributeEnum() {
    return new (std::nothrow) AttributeEnum();
}

ITfDisplayAttributeInfo* NewDisplayAttributeInfo(REFGUID guid) {
    if (!IsEqualGUID(guid, kDisplayAttributeInput)) return nullptr;
    return new (std::nothrow) InputAttribute();
}

}  // namespace fkeyime
