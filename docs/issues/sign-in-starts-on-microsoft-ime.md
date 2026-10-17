# After a reboot, the F-keys go to the Microsoft IMEs

**Status:** open again (2026-10-17, 21:50 reboot). The default input method override from
commit 8004c29 was set, and the session still started on the Microsoft Korean IME.

## Symptom

After restarting the PC, FKeyIME wasn't active. F18 / F20 went through AutoHotkey to the
Microsoft IMEs until the owner picked FKeyIME's languages with Win+Space (first time) or by
clicking FKeyIME **ENG** in the taskbar's input menu (second time).

Once FKeyIME EN was picked, everything worked: JIS symbols, and switching between ㅁㄴㅇ and
ちとし with the F-keys without picking anything by hand again (owner). Whether FKeyIME's English
would come up on its own wasn't tested, because the owner picked ENG by hand.

## First occurrence and first fix (commit 8004c29)

After signing in, Windows starts with the **first input method in the language list**. The list
was ko (Microsoft Korean IME first), ja, en-US, so the session started on the Microsoft Korean IME
*(measured)*. The FKeyIME-aware AutoHotkey script then did the right thing for that choice: it
handled the F-keys the Microsoft way.

Fix applied then: Windows' default input method override set to FKeyIME EN:

```powershell
Set-WinDefaultInputMethodOverride -InputTip "0409:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{8F61A2C0-75BC-4042-B739-CC7D63B542E5}"
```

It was marked solved without a reboot to confirm it. That was a mistake: it hadn't been measured.

## Second occurrence (reboot at 21:50:06, 2026-10-17)

State at the time *(measured right after, 21:58; nothing was changed in between)*:

| | Value |
| --- | --- |
| Language list, in order | 1. **ko**: Microsoft Korean IME `0412:{A028AE76-…}{B5FE1F02-…}`, then FKeyIME KO `0412:{66749D2A-…}{28C201BF-…}` |
| | 2. **ja**: Microsoft Japanese IME `0411:{03B5835F-…}{A76C93D9-…}`, then FKeyIME JA `0411:{66749D2A-…}{1AC13F3E-…}` |
| | 3. **en-US**: FKeyIME EN `0409:{66749D2A-…}{8F61A2C0-…}` only |
| Default input method override | FKeyIME EN (`Get-WinDefaultInputMethodOverride`, and `InputMethodOverride` in `HKCU\Control Panel\International\User Profile`) |
| `HKCU\Keyboard Layout\Preload` | 1 `00000412`, 2 `00000409`, 3 `00000411` |
| Language bar | `IsLegacyLanguageBar` False, `IsLegacySwitchingMode` False |
| AutoHotkey | Logon task `hhkb-ekj-trilingual` running `Documents\AutoHotkey\hhkb-ekj-trilingual.ahk`, the FKeyIME-aware script (same content as `examples/autohotkey/`, commit 0691dd6), started 21:50:34. It does nothing at startup; it only acts on F-keys |
| FKeyIME | Registered (development install) |

What happened, from FKeyIME's log (`%ProgramData%\fkeyime\fkeyime.log`; its timestamps restart
at boot):

- Boot at 21:50:06. **No process asked for FKeyIME for about 5 minutes**: the first entry after
  boot is `SearchHost.exe DllGetClassObject` at 304.9 s (about 21:55). So Windows didn't try to
  load FKeyIME and fail; it chose the Microsoft Korean IME *(measured: absence of log entries;
  the clock mapping is inferred)*.
- From 304.9 s on, the owner's picks show up as FKeyIME JA, EN, KO activations in SearchHost and
  Explorer.
- So the override didn't decide the input method at sign-in.

## Third occurrence (reboot at 01:33:48, 2026-10-18)

An unplanned reboot (a Mozc uninstall restarted the PC). Same settings as above, the override
still FKeyIME EN. FKeyIME's log has **no entry at all** from the boot to 01:37:31, while the
owner was using the PC *(measured)*: the session started on a Microsoft IME again. The
override alone doesn't work on this PC, now measured twice.

## Cause

Not known yet. Hypotheses, none tested:

1. Windows restores the input method that was active when the previous session ended (the
   owner had been on the Microsoft IMEs), and the override only applies when there's nothing to
   restore.
2. The override is read only when the default language's first input method is used, and the
   first language (ko) wins.
3. The override applies to new apps, not to the shell processes that start before it's read.

## Next steps

- Test in a VM first if possible (AGENTS.md). On the owner's PC, each test needs the owner's OK,
  because it changes language settings.
- Test 1: move en-US to the top of the language list (FKeyIME EN first), reboot, and check which
  input method Explorer and the first app get, from FKeyIME's log.
- Test 2: keep the order, switch to FKeyIME EN before shutting down, reboot (hypothesis 1).
- The installer (M5) must not rely on the override alone until one of these is measured to work.
