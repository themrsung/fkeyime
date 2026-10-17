// "Copy system details" in the settings window (docs/spec.md, section 7, item 8): plain text
// for an issue report. Nothing leaves the PC; the caller puts it on the clipboard.
#pragma once

#include <string>

namespace fkeyime::settings {

// FKeyIME version, Windows version, FKeyIME's registration and options. With
// `include_autohotkey` (only after the user agreed): whether AutoHotkey is installed, its
// version, and the command lines of the AutoHotkey processes running now.
std::wstring SystemDetails(bool include_autohotkey);

}  // namespace fkeyime::settings
