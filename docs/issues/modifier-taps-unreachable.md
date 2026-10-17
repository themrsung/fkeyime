# Right Win / Right Ctrl / Right Alt taps never reach FKeyIME

**Status:** not fixable in a TIP. The escape hatch uses ``Alt+` ``, ``Alt+Shift+` `` and Menu instead
(`docs/spec.md`, section 8).

## What was tried *(measured with `fkeyime_probe`, 2026-10-17)*

| Key | Key event sink | TSF preserved key (also with `TF_MOD_ON_KEYUP`) |
| --- | --- | --- |
| Right Alt | Never called (Alt is a system key) | Never fired; its key-up put the window in menu mode |
| Right Win | Not seen; the Start menu opened | Never fired |
| Right Ctrl | Not seen | Never fired |
| Menu | Called, but eating the key didn't stop the context menu | **Fires.** TSF-aware controls: no context menu. Classic Win32 Edit: the menu still opens |

A Windows key remap (`Scancode Map`) to F-keys would work, but the owner rejected it: a required
remap defeats the purpose of the escape hatch.

## Related: the ` key under the Japanese layout *(measured)*

Under the Japanese layout, the ` key is 半角/全角 (`VK_OEM_AUTO` / `VK_OEM_ENLW`) and ``Alt+` `` is
`VK_KANJI`, which Windows also uses for its Japanese on/off hotkey. No preserved key matches it, but
it reaches the key event sink, so FKeyIME catches ``Alt+` `` there by scan code. Moving Windows' hotkey
wasn't needed: with it restored, the cycle still passed.
