# FKeyIME API (version 1)

Lets other programs ask whether FKeyIME is the input method of a window, and in which mode. The
first user is the hhkb-ekj-trilingual v1.5.2 AutoHotkey script: with the check below it steps
aside while FKeyIME is active, so both can be installed at once
([`examples/autohotkey/hhkb-ekj-trilingual.ahk`](../examples/autohotkey/hhkb-ekj-trilingual.ahk)).

## Why an API

From outside, FKeyIME and the Microsoft IMEs look the same: both report the language's keyboard
layout (`GetKeyboardLayout` gives `0x04120412` for FKeyIME KO and for the Microsoft Korean IME).
Only FKeyIME knows on which threads it's active, so it publishes that.

## Finding the DLL

`fkeyime.dll` exports the API. Find it through FKeyIME's permanent CLSID:

```
HKLM\SOFTWARE\Classes\CLSID\{66749D2A-1770-43D3-A269-A051B5BC1AAA}\InprocServer32   (default value)
```

A 32-bit process reading this key gets the x86 DLL through registry redirection
(`WOW6432Node`). If the key is missing, FKeyIME isn't installed.

Loading the DLL doesn't activate the IME. The functions only read shared memory, so a caller may
load and unload the DLL for every call. AutoHotkey's `DllCall("<path>\Function")` does that, and
then the DLL is never locked *(measured: AutoHotkey doesn't keep `fkeyime.dll` loaded)*.

## Functions

C declarations: [`src/tip/fkeyime_api.h`](../src/tip/fkeyime_api.h). All are `WINAPI` (stdcall on
x86), with undecorated export names on x64 and x86.

| Function | Returns |
| --- | --- |
| `int FKeyImeApiVersion(void)` | `1` |
| `int FKeyImeQueryWindow(HWND window)` | FKeyIME's mode on the thread that owns `window`; `window` = `NULL` means the window with keyboard focus in the foreground |
| `int FKeyImeQueryThread(DWORD thread_id)` | The same for a thread |

| Value | Meaning |
| --- | --- |
| `0` / `1` / `2` | FKeyIME is active there: English / Korean / Japanese |
| `-1` | FKeyIME isn't the active input method there |
| `-2` | No state table: no FKeyIME instance has run in this session yet. Treat as `-1` |

## How it works

Each thread with an FKeyIME profile active holds a slot in a session-wide shared-memory table
(`Local\fkeyime.state.v1`): FKeyIME claims it on activation, updates the mode on every profile
change, and releases it on deactivation. Slots of threads that died without releasing are ignored
and reused. Layout (append-only ABI): [`src/tip/state_table.h`](../src/tip/state_table.h).
The header's `session_flags` (bit 0: romaji) holds the global kana/romaji choice; it was the
reserved field, so version 1 readers are unaffected.
Unit-tested in `tests/state_table_test.cpp`.

Known limit **[verify]**: an AppContainer (sandboxed Store) app may not be able to open the
session's table. FKeyIME there wouldn't be reported, and the API answers `-1`.

## AutoHotkey example

The whole script is in [`examples/autohotkey/hhkb-ekj-trilingual.ahk`](../examples/autohotkey/hhkb-ekj-trilingual.ahk):
v1.5.2's script plus three changes.

1. The F-key hotkeys sit under `#HotIf !FKeyImeActive()`. While FKeyIME is active they're off, and
   the keys pass straight through to FKeyIME.

   ```autohotkey
   FKeyImeActive() {
       try dll := RegRead("HKLM\SOFTWARE\Classes\CLSID\{66749D2A-1770-43D3-A269-A051B5BC1AAA}\InprocServer32")
       catch
           return false                        ; FKeyIME isn't installed
       try return DllCall(dll "\FKeyImeQueryWindow", "ptr", FocusHwnd(), "int") >= 0
       catch
           return false                        ; DLL missing or without the API: act as before
   }
   ```

2. When the script switches language, Windows picks that language's **last-used** input method,
   which can be FKeyIME's. If FKeyIME is active right after the switch, the script selects the
   Microsoft IME explicitly (`ITfInputProcessorProfileMgr::ActivateProfile`, whole session).
   Without this, the first F-key after leaving FKeyIME landed back in FKeyIME *(measured)*.
3. The 150 ms check counts "FKeyIME is active" as a failed switch, so the retry runs.

Without FKeyIME installed, the script behaves exactly like v1.5.2's.

## Measured (2026-10-17, owner's PC)

`fkeyime_probe.exe --handoff 50` (`tools/spike/probe.cpp`), with FKeyIME installed by
`tools/spike.ps1` and the script above running from the logon task:

| Phase | Result |
| --- | --- |
| FKeyIME active: F18, F19, F20, F16 × 50 | 200/200 reached FKeyIME (right profile), API right 200/200, none taken by AutoHotkey |
| Microsoft Korean IME active | API says `-1` |
| Microsoft IME active: F20, F18 × 50 | 100/100 handled by AutoHotkey (right language, Microsoft IME). FKeyIME was briefly selected once, on the first switch, and corrected |

An earlier 50-round run gave 77/100 in the last phase. The owner reported a physical key press
during one of the runs, most likely that one *(inferred)*. Runs since then: 60/60, 100/100,
100/100.
