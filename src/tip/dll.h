// Module-wide state of the TIP DLL.
#pragma once

#include <windows.h>

namespace fkeyime {

HINSTANCE DllInstance();
void DllAddRef();  // objects and server locks keep the DLL loaded (DllCanUnloadNow)
void DllRelease();

}  // namespace fkeyime
