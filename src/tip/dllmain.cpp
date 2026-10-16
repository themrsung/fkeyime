// FKeyIDE TSF text input processor (in-proc COM server).
//
// Skeleton only: the exports exist so the DLL builds and links for every architecture.
// Class factory, TSF registration and the text service itself arrive in milestone M1.
#include <windows.h>

#include "guids.h"

namespace {
HINSTANCE g_module = nullptr;
}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow() { return S_OK; }

STDAPI DllGetClassObject(REFCLSID, REFIID, void** object) {
    if (object) *object = nullptr;
    return CLASS_E_CLASSNOTAVAILABLE;  // M1
}

STDAPI DllRegisterServer() { return E_NOTIMPL; }    // M1 (dev registration; MSI in M5)

STDAPI DllUnregisterServer() { return E_NOTIMPL; }  // M1
