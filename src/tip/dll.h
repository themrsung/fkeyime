// Module-wide state of the TIP DLL.
#pragma once

#include <windows.h>

namespace fkeyide {

HINSTANCE DllInstance();
void DllAddRef();  // objects and server locks keep the DLL loaded (DllCanUnloadNow)
void DllRelease();

}  // namespace fkeyide
