#include "registration.h"

#include <msctf.h>
#include <wrl/client.h>

#include <cstdio>
#include <cwchar>

#include "guids.h"

using Microsoft::WRL::ComPtr;

namespace fkeyide {
namespace {

struct ProfileInfo {
    const GUID* profile;
    LANGID langid;
    const wchar_t* description;
};

constexpr ProfileInfo kProfiles[] = {
    {&kProfileEnglish, kLangIdEnglish, L"FKeyIDE EN"},
    {&kProfileKorean, kLangIdKorean, L"FKeyIDE KO"},
    {&kProfileJapanese, kLangIdJapanese, L"FKeyIDE JA"},
};

// What the TIP claims to support. Immersive support lets Store apps load it.
const GUID* const kCategories[] = {
    &GUID_TFCAT_TIP_KEYBOARD,
    &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    &GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
    &GUID_TFCAT_TIPCAP_COMLESS,
    &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
};

constexpr wchar_t kDescription[] = L"FKeyIDE";

void ClsidKeyPath(wchar_t* path, size_t size) {
    wchar_t clsid[39];
    StringFromGUID2(kClsidTextService, clsid, ARRAYSIZE(clsid));
    swprintf_s(path, size, L"Software\\Classes\\CLSID\\%s", clsid);
}

}  // namespace

HRESULT RegisterServer(HINSTANCE module) {
    wchar_t dll[MAX_PATH];
    const DWORD length = GetModuleFileNameW(module, dll, MAX_PATH);
    if (!length || length == MAX_PATH) return E_FAIL;

    wchar_t path[128];
    ClsidKeyPath(path, ARRAYSIZE(path));
    HKEY clsid = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, nullptr, 0, KEY_WRITE, nullptr, &clsid,
                        nullptr) != ERROR_SUCCESS) {
        return E_ACCESSDENIED;
    }
    RegSetValueExW(clsid, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(kDescription),
                   sizeof(kDescription));
    HKEY server = nullptr;
    LSTATUS status = RegCreateKeyExW(clsid, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr,
                                     &server, nullptr);
    if (status == ERROR_SUCCESS) {
        constexpr wchar_t kApartment[] = L"Apartment";
        status = RegSetValueExW(server, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(dll),
                                (length + 1) * sizeof(wchar_t));
        if (status == ERROR_SUCCESS) {
            status = RegSetValueExW(server, L"ThreadingModel", 0, REG_SZ,
                                    reinterpret_cast<const BYTE*>(kApartment), sizeof(kApartment));
        }
        RegCloseKey(server);
    }
    RegCloseKey(clsid);
    return HRESULT_FROM_WIN32(status);
}

HRESULT UnregisterServer() {
    wchar_t path[128];
    ClsidKeyPath(path, ARRAYSIZE(path));
    const LSTATUS status = RegDeleteTreeW(HKEY_LOCAL_MACHINE, path);
    return status == ERROR_FILE_NOT_FOUND ? S_OK : HRESULT_FROM_WIN32(status);
}

HRESULT RegisterProfiles(HINSTANCE module) {
    ComPtr<ITfInputProcessorProfileMgr> manager;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&manager));
    if (FAILED(hr)) return hr;

    wchar_t icon[MAX_PATH];
    const DWORD icon_length = GetModuleFileNameW(module, icon, MAX_PATH);
    for (const ProfileInfo& p : kProfiles) {
        // Every profile substitutes the Japanese 106/109 layout, so the symbols match the JIS
        // keycaps in every mode (docs/spec.md, "Architecture").
        hr = manager->RegisterProfile(
            kClsidTextService, p.langid, *p.profile, p.description,
            static_cast<ULONG>(wcslen(p.description)), icon, icon_length, 0,
            reinterpret_cast<HKL>(static_cast<ULONG_PTR>(kHklJapanese)), 0, TRUE, 0);
        if (FAILED(hr)) return hr;
    }
    return S_OK;
}

HRESULT UnregisterProfiles() {
    ComPtr<ITfInputProcessorProfileMgr> manager;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&manager));
    if (FAILED(hr)) return hr;
    for (const ProfileInfo& p : kProfiles) {
        manager->UnregisterProfile(kClsidTextService, p.langid, *p.profile, 0);
    }
    return S_OK;
}

HRESULT RegisterCategories() {
    ComPtr<ITfCategoryMgr> categories;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&categories));
    if (FAILED(hr)) return hr;
    for (const GUID* category : kCategories) {
        hr = categories->RegisterCategory(kClsidTextService, *category, kClsidTextService);
        if (FAILED(hr)) return hr;
    }
    return S_OK;
}

HRESULT UnregisterCategories() {
    ComPtr<ITfCategoryMgr> categories;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&categories));
    if (FAILED(hr)) return hr;
    for (const GUID* category : kCategories) {
        categories->UnregisterCategory(kClsidTextService, *category, kClsidTextService);
    }
    return S_OK;
}

}  // namespace fkeyide
