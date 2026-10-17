# After a reboot, the F-keys go to the Microsoft IMEs

**Status:** solved (2026-10-17, commit 8004c29).

## Symptom

After restarting the PC, FKeyIME wasn't active. F18 / F20 went through AutoHotkey to the
Microsoft IMEs until the owner picked FKeyIME's languages with Win+Space.

## Cause

After signing in, Windows starts with the **first input method in the language list**. The list
was ko (Microsoft Korean IME first), ja, en-US, so the session started on the Microsoft Korean IME
*(measured)*. The FKeyIME-aware AutoHotkey script then did the right thing for that choice: it
handled the F-keys the Microsoft way.

## Solution

Set Windows' default input method override to FKeyIME EN:

```powershell
Set-WinDefaultInputMethodOverride -InputTip "0409:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{8F61A2C0-75BC-4042-B739-CC7D63B542E5}"
```

Or in Settings → Time & language → Typing → Advanced keyboard settings → "Override for default
input method". The installer (M5) must offer this and clear it on uninstall (`../spec.md`,
section 3).
