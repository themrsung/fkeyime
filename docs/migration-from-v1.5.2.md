# Migrating from hhkb-ekj-trilingual v1.5.2 to FKeyIDE

For people who set up Windows with
[`themrsung/hhkb-ekj-trilingual`](https://github.com/themrsung/hhkb-ekj-trilingual) at tag
**v1.5.2**: HHKB Studio JIS on keymap Profile 4, AutoHotkey v2 switching between the Microsoft
Korean and Japanese IMEs, started by the `hhkb-ekj-trilingual` logon task.

> [!IMPORTANT]
> **Status at this commit: FKeyIDE can't be installed yet.** The repo has the toolchain, the
> mode logic (unit-tested) and an empty TIP DLL. There's no IME and no installer. **Keep using
> v1.5.2.** The steps below describe the planned migration and get updated with every commit.
> Steps marked ⏳ depend on milestones that aren't done (`spec.md`, section 4).

## Who needs to migrate

| You use v1.5.2 on | What to do |
| --- | --- |
| **macOS** (Karabiner + Hammerspoon, Profile1) | Nothing. FKeyIDE is Windows-only, and macOS is unchanged |
| **Windows**, Profile 4 + AutoHotkey | This guide, if you want FKeyIDE. AutoHotkey stays supported as the legacy option |
| **Windows**, Profile3 fallback (no AutoHotkey) | Optional. FKeyIDE needs Profile 4, below |
| HYBRID Type-S, or a US ANSI board | Not applicable. FKeyIDE targets the HHKB Studio JIS |

## What changes for you

| | v1.5.2 (AutoHotkey) | FKeyIDE |
| --- | --- | --- |
| F18 → Korean | Microsoft Korean IME, 한 mode | FKeyIDE KO (own 2-Set engine) |
| F19 → English | Japanese language with the IME off (taskbar shows JA / A) | FKeyIDE EN (en-US, taskbar shows EN) |
| F20 → Japanese | Microsoft Japanese IME, kana typing | FKeyIDE JA (Mozc conversion), kana typing |
| F17 | Korean: 한자. Elsewhere: nothing | Korean: Hanja. Japanese: transient romaji. English: transient Caps Lock |
| Symbols in Korean | **US ANSI** (the compromise) | Goal: **JIS**. Measured in M0: the planned layout substitution doesn't work, so FKeyIDE KO (and EN) type **US symbols** for now. A fix is being chosen (spec open question 8) ⏳ |
| Mode across windows | Per window, so a window can drift back to Korean (`issues/windows-inconsistent-symbols.md`) | One global mode |
| Elevated (admin) windows | Keys don't switch (UIPI) | Switch works, the TIP loads in-process ⏳ |
| Password fields, UAC, sign-in screen | AutoHotkey still switches the language | F17–F20 do nothing there (TIPs aren't loaded) |
| Fn1+P → Play/Pause | AutoHotkey | Not handled by FKeyIDE. See "Fn1+P" below |
| Japanese conversion | Microsoft (cloud suggestions, your MS user dictionary) | Mozc (offline, different results). Your MS dictionary doesn't carry over |
| Korean extras | Full Microsoft Korean IME (3-Set, old Hangul, …) | 2-Set only in v1 |

**FKeyIDE adds dependencies** that v1.5.2 doesn't have: an MSI that installs system-wide COM DLLs
(x64 + x86) and needs admin rights, the bundled Mozc conversion engine (`mozc_server`), and binaries
built from this separate BSD-3-Clause repo instead of a script you can read in the docs.

## What stays the same

- **The keyboard.** DIP switches all OFF, keymap **Profile 4**. No change in the keymap tool.
- **Japanese keyboard (106/109) as the hardware layout** (v1.5.2 `docs/windows/02-windows.md`,
  step 3). FKeyIDE relies on it for the JIS symbols. Don't switch to "Detect connected keyboard".
- **The one-way key rules**: a mode key never toggles out and is never dead.

## Steps

Every step changes the system. Do them in order, and keep the backups.

1. **Back up** (works today):

   ```powershell
   $stamp = Get-Date -f yyyyMMddHHmmss
   Get-WinUserLanguageList | ForEach-Object { "$($_.LanguageTag): $($_.InputMethodTips -join ', ')" } |
     Set-Content "$env:USERPROFILE\languagelist.bak-$stamp.txt"
   Export-ScheduledTask hhkb-ekj-trilingual | Set-Content "$env:USERPROFILE\hhkb-ekj-trilingual-task.bak-$stamp.xml"
   Copy-Item "$env:USERPROFILE\Documents\AutoHotkey\hhkb-ekj-trilingual.ahk" "$env:USERPROFILE\hhkb-ekj-trilingual.ahk.bak-$stamp"
   ```

2. **Stop AutoHotkey switching** (works today; needed before step 3. Measured in M0: while the
   v1.5.2 script runs, its keyboard hook takes F17–F20 and FKeyIDE never sees them):

   ```powershell
   Stop-ScheduledTask hhkb-ekj-trilingual
   Get-CimInstance Win32_Process -Filter "Name='AutoHotkey64.exe'" |
     Where-Object CommandLine -match 'hhkb-ekj-trilingual' | Invoke-CimMethod -MethodName Terminate
   Disable-ScheduledTask hhkb-ekj-trilingual   # disable rather than delete until FKeyIDE is verified
   ```

   AutoHotkey itself can stay installed.

3. ⏳ **Install FKeyIDE** (M5): run `fkeyide-<version>-x64.msi` from a FKeyIDE release. It
   installs both the x64 and x86 DLLs and adds **FKeyIDE EN / KO / JA** to your language list.

4. ⏳ **Language list** (M5): the installer offers to remove the Microsoft Korean and Japanese IMEs
   from the list (they stay installed). Accept it, or remove them later in Settings → Time &
   language → Language & region. Turn **off** "Let me use a different input method for each app
   window" (Settings → Time & language → Typing → Advanced keyboard settings).

5. ⏳ **Verify** (M6): sign out and back in, then check F18 → `안녕하세요`, F19 → `the"` with
   Shift+2 → `"`, F20 → `3` gives あ, F17 in each mode, and the symbols in Korean
   (``@[]:;"`` should now match the keycaps).

6. Once it's verified, delete the logon task: `Unregister-ScheduledTask hhkb-ekj-trilingual`.

### Fn1+P (Play/Pause)

v1.5.2 turned Fn1+P (`Pause`) into Play/Pause with AutoHotkey. FKeyIDE doesn't do this
(open question 7 in `spec.md`). Until that's decided, either keep a one-line AutoHotkey script
(`Pause::Send("{Media_Play_Pause}")`) on the same logon task, or live without it.

## Rolling back to v1.5.2

1. ⏳ Uninstall FKeyIDE (Settings → Apps). The uninstaller puts the Microsoft IMEs back in the
   language list. If it doesn't, restore them from the backup in step 1.
2. `Enable-ScheduledTask hhkb-ekj-trilingual; Start-ScheduledTask hhkb-ekj-trilingual`
3. Check that F18–F20 work again as in v1.5.2 `docs/windows/03-autohotkey.md`.

## Changelog of this guide

One entry per FKeyIDE commit (newest first).

- **Spike uninstall cleanup.** No user-visible change (`tools/spike.ps1` is for development only).
- **Spec wording fix.** No user-visible change.
- **M0 spike, work in progress.** The TIP now registers the three profiles and handles F18–F20
  (development install via `tools/spike.ps1`, not for end users). Measured: Korean and English
  profiles get US symbols, not JIS, so the Korean-symbols row now says so; AutoHotkey must be
  stopped first because its hook takes the F keys. Still nothing installable.
- **Repo setup, toolchain, mode logic.** First version. Nothing is installable yet. Steps 1, 2
  and 6 and the rollback are runnable. The others wait on M0–M6.
