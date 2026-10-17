// FKeyIME TSF text input processor (in-proc COM server): DLL exports and the class factory.
#include <windows.h>

#include <new>

#include "dll.h"
#include "guids.h"
#include "registration.h"
#include "text_service.h"

namespace fkeyime {
namespace {

HINSTANCE g_module = nullptr;
LONG g_refs = 0;

class ClassFactory final : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_INVALIDARG;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *object = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }
    // Static object: its references are lock counts on the DLL.
    STDMETHODIMP_(ULONG) AddRef() override {
        DllAddRef();
        return 2;
    }
    STDMETHODIMP_(ULONG) Release() override {
        DllRelease();
        return 1;
    }

    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* service = new (std::nothrow) TextService();
        if (!service) return E_OUTOFMEMORY;
        const HRESULT hr = service->QueryInterface(riid, object);
        service->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        lock ? DllAddRef() : DllRelease();
        return S_OK;
    }
};

ClassFactory g_factory;

}  // namespace

HINSTANCE DllInstance() { return g_module; }
void DllAddRef() { InterlockedIncrement(&g_refs); }
void DllRelease() { InterlockedDecrement(&g_refs); }

}  // namespace fkeyime

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        fkeyime::g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow() { return fkeyime::g_refs ? S_FALSE : S_OK; }

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** object) {
    if (!object) return E_INVALIDARG;
    *object = nullptr;
    if (!IsEqualCLSID(clsid, fkeyime::kClsidTextService)) return CLASS_E_CLASSNOTAVAILABLE;
    return fkeyime::g_factory.QueryInterface(riid, object);
}

STDAPI DllUnregisterServer();

// Development registration (regsvr32, as admin). The MSI replaces this in M5.
STDAPI DllRegisterServer() {
    HRESULT hr = fkeyime::RegisterServer(fkeyime::g_module);
    if (SUCCEEDED(hr)) hr = fkeyime::RegisterProfiles(fkeyime::g_module);
    if (SUCCEEDED(hr)) hr = fkeyime::RegisterCategories();
    if (FAILED(hr)) DllUnregisterServer();
    return hr;
}

STDAPI DllUnregisterServer() {
    fkeyime::UnregisterCategories();
    fkeyime::UnregisterProfiles();
    return fkeyime::UnregisterServer();
}
