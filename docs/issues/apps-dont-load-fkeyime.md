# Korean and kana type English letters in some apps

**Status:** solved by restarting the apps (2026-10-17, commit 7be6e46).

## Symptom

FKeyIME seemed active (the taskbar showed FKeyIME KO or JA, and the F-keys switched it), but in
Notepad, Claude, Opera, Edge and Windows Terminal, Korean and Japanese typed English letters. In
Korean the symbols were US, in Japanese JIS: Windows' plain keyboard layouts were typing.

## Cause

Apps that were already running while FKeyIME was installed, updated or reinstalled (many times
that day, including the rename from FKeyIDE) never loaded FKeyIME again *(measured)*. They switched
to FKeyIME's language, but never asked its DLL for its COM object: no `DllGetClassObject` in the
log, no `fkeyime.dll` in their loaded modules. A freshly started Win32 window and a fresh WPF app
(PowerShell ISE) loaded it at once.

## Solution

Restart the apps, or sign out and back in. Notepad typed Korean and kana after a restart
*(measured)*. The installer must tell users to restart apps or sign out.

## Diagnosing it again

FKeyIME logs `DllGetClassObject` and `CreateInstance` once per app (no text) to
`%ProgramData%\fkeyime\fkeyime.log`. If an app never logs them, Windows never tried to load
FKeyIME there.
