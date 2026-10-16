// COM and TSF registration (regsvr32 for development; the MSI takes over in M5).
#pragma once

#include <windows.h>

namespace fkeyide {

HRESULT RegisterServer(HINSTANCE module);  // COM class: HKLM\Software\Classes\CLSID
HRESULT RegisterProfiles(HINSTANCE module);
HRESULT RegisterCategories();

HRESULT UnregisterCategories();
HRESULT UnregisterProfiles();
HRESULT UnregisterServer();

}  // namespace fkeyide
