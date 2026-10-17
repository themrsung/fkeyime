#include "registration.h"

#include <msctf.h>
#include <wrl/client.h>

#include <cstdio>
#include <cwchar>

#include "guids.h"
#include "resource.h"

using Microsoft::WRL::ComPtr;

namespace fkeyime {
namespace {

struct ProfileInfo {
    const GUID* profile;
    LANGID langid;
    const wchar_t* description;
    UINT display_name;  // string resource: the name Windows Settings shows
};

constexpr ProfileInfo kProfiles[] = {
    {&kProfileEnglish, kLangIdEnglish, L"FKeyIME EN", IDS_PROFILE_ENGLISH},
    {&kProfileKorean, kLangIdKorean, L"FKeyIME KO", IDS_PROFILE_KOREAN},
    {&kProfileJapanese, kLangIdJapanese, L"FKeyIME JA", IDS_PROFILE_JAPANESE},
};

// What the TIP claims to support. Immersive support lets Store apps load it.
const GUID* const kCategories[] = {
    &GUID_TFCAT_TIP_KEYBOARD,
    &GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
    &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    &GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
    &GUID_TFCAT_TIPCAP_COMLESS,
    &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
};

constexpr wchar_t kDescription[] = L"FKeyIME";

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

    // ITfInputProcessorProfilesEx sets the display name ("Display Description", an MUI string)
    // that Windows Settings shows (docs/issues/settings-doesnt-list-fkeyime.md).
    ComPtr<ITfInputProcessorProfilesEx> profiles;
    hr = manager.As(&profiles);
    if (FAILED(hr)) return hr;
    // The older Register writes the TIP-level "Enable" value. Windows Settings lists a TIP's
    // keyboards only with it: Mozc, which calls Register, was listed and FKeyIME, without it,
    // wasn't (measured, docs/issues/settings-doesnt-list-fkeyime.md).
    hr = profiles->Register(kClsidTextService);
    if (FAILED(hr)) return hr;

    // The DLL's own icon (IDI_FKEYIME, the first icon: index 0) and string table.
    wchar_t icon[MAX_PATH];
    const DWORD icon_length = GetModuleFileNameW(module, icon, MAX_PATH);
    for (const ProfileInfo& p : kProfiles) {
        // No substitute layout. Windows ignores one for typing (M0, docs/spec.md section 6.2:
        // FKeyIME types the JIS symbols from its own table), and the Japanese one that every
        // profile used to name is the suspect for Settings not listing FKeyIME KO and EN.
        hr = manager->RegisterProfile(kClsidTextService, p.langid, *p.profile, p.description,
                                      static_cast<ULONG>(wcslen(p.description)), icon,
                                      icon_length, 0, nullptr, 0, TRUE, 0);
        if (FAILED(hr)) return hr;
        // RegisterProfile keeps a value from an older registration: remove it.
        wchar_t clsid[39];
        wchar_t profile[39];
        StringFromGUID2(kClsidTextService, clsid, ARRAYSIZE(clsid));
        StringFromGUID2(*p.profile, profile, ARRAYSIZE(profile));
        wchar_t key[160];
        swprintf_s(key, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\LanguageProfile\\0x%08x\\%s", clsid,
                   p.langid, profile);
        RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, key, L"SubstituteLayout");
        hr = profiles->SetLanguageProfileDisplayName(kClsidTextService, p.langid, *p.profile, icon,
                                                     icon_length, p.display_name);
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
    ComPtr<ITfInputProcessorProfiles> profiles;
    if (SUCCEEDED(manager.As(&profiles))) profiles->Unregister(kClsidTextService);
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

}  // namespace fkeyime
