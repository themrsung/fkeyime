# AutoHotkey takes the F-keys away from FKeyIME

**Status:** solved in stages (2026-10-17). Current script:
`../../examples/autohotkey/hhkb-ekj-trilingual.ahk`.

## 1. The v1.5.2 script hides the F-keys from FKeyIME

With hhkb-ekj-trilingual's v1.5.2 AutoHotkey script running, its keyboard hook took F17–F20
before FKeyIME could see them *(measured in M0)*. Fix (commit a079a15): the script asks FKeyIME's
API (`../api.md`) whether FKeyIME is active in the focused window and steps aside if it is.

## 2. Switching to Korean lands back in FKeyIME

With a Microsoft IME chosen, the script switches language, and Windows picks that language's
**last-used** input method, which can be FKeyIME's. The first F-key after leaving FKeyIME landed
back in FKeyIME *(measured)*. Fix (commit a079a15): if FKeyIME is active right after the switch,
the script selects the Microsoft IME explicitly.

## 3. One F-key pulls everything back to Microsoft

That correction is session-wide. One F-key in a window without FKeyIME (Notepad during a Hanja
test) moved the whole session to the Microsoft IMEs. Every window stayed there, and the script
kept taking the keys *(measured)*. Fix (commit 0691dd6): the script reads the session's input
method, which its own thread follows *(measured)*, and **Win+Space decides**:

- FKeyIME chosen: the script never takes F16–F20. A window not on FKeyIME yet is moved to it and
  the key is sent again.
- A Microsoft IME chosen: v1.5.2's switching.

Measured with the deployed script: FKeyIME active 120/120, Microsoft chosen 60/60 to AutoHotkey,
a window stuck on Microsoft's IME taken over by FKeyIME 30/30 (`fkeyime_probe.exe --handoff 30`).
