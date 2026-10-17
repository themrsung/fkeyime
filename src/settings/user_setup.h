// Per-user setup, run by the MSI as the signed-in user (not as SYSTEM): put FKeyIME's three
// profiles in the user's language list and make FKeyIME EN the default input method, or undo it.
#pragma once

namespace fkeyime::settings {

// Returns 0 on success, else a Win32 error code. Both log to %ProgramData%\fkeyime\setup.log
// when that folder exists.
int UserInstall();
int UserUninstall();

}  // namespace fkeyime::settings
