# A changed setting doesn't reach running apps

**Status:** solved for apps that load the current build. Apps still running an older FKeyIME
build need one restart (or a sign-out).

## Symptom

2026-10-17: after `KeyboardLayout` was removed (back to JIS), Shift+2 still typed `@` (US) in apps
that were already open. A newly opened Notepad window typed `"` (JIS).

## Cause *(measured)*

Two things:

1. FKeyIME re-read its keyboard options only in `OnSetFocus`. TSF calls that when FKeyIME's
   thread manager gains focus, which rarely happens when you just switch windows: the log showed
   no `OnSetFocus` from the open apps at all. (Before, one check of "a running Notepad picks up
   the change" passed only because it opened a new window.)
2. Claude, Discord, Explorer and Search had loaded FKeyIME before the escape-hatch build was
   installed, so they ran an older DLL. That's the same as
   [apps-dont-load-fkeyime.md](apps-dont-load-fkeyime.md): a running app keeps the FKeyIME build
   it loaded.

## Solution

FKeyIME now registers for change notifications on `HKCU` and `HKLM\Software\fkeyime`
(`RegNotifyChangeKeyValue`) and checks the signal, without waiting, on every key. A changed
option applies at the next key. `fkeyime_probe --typing OPTION` changes `KeyboardLayout` while
its window runs and passes in both the Win32 Edit and RichEdit.

Apps that are still running an older build must be restarted once, or sign out and back in.
