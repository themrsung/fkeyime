# Migrating from hhkb-ekj-trilingual v1.5.2 to FKeyIME

For people who set up Windows with
[`themrsung/hhkb-ekj-trilingual`](https://github.com/themrsung/hhkb-ekj-trilingual) at tag
**v1.5.2**: HHKB Studio JIS on keymap Profile 4, AutoHotkey v2 switching between the Microsoft
Korean and Japanese IMEs, started by the `hhkb-ekj-trilingual` logon task.

> [!IMPORTANT]
> **Status at this commit: FKeyIME can't be installed yet.** Milestone M0 (spikes) is done. A
> development TIP switches between its EN/KO/JA profiles with F18/F19/F20, but it doesn't type
> Korean or Japanese yet, and there's no installer. **Keep using v1.5.2.** The steps below
> describe the planned migration and get updated with every commit.
> Steps marked ⏳ depend on milestones that aren't done (`spec.md`, section 4).

## Who needs to migrate

| You use v1.5.2 on | What to do |
| --- | --- |
| **macOS** (Karabiner + Hammerspoon, Profile1) | Nothing. FKeyIME is Windows-only, and macOS is unchanged |
| **Windows**, Profile 4 + AutoHotkey | This guide, if you want FKeyIME. AutoHotkey stays supported as the legacy option |
| **Windows**, Profile3 fallback (no AutoHotkey) | Optional. FKeyIME needs Profile 4, below |
| HYBRID Type-S, or a US ANSI board | Not applicable. FKeyIME targets the HHKB Studio JIS |

## What changes for you

| | v1.5.2 (AutoHotkey) | FKeyIME |
| --- | --- | --- |
| F18 → Korean | Microsoft Korean IME, 한 mode | FKeyIME KO (own 2-Set engine) |
| F19 → English | Japanese language with the IME off (taskbar shows JA / A) | FKeyIME EN (en-US, taskbar shows EN), JIS symbols from FKeyIME's own key table ⏳ |
| F20 → Japanese | Microsoft Japanese IME, kana typing | FKeyIME JA: its own kana table, Mozc only for kana-kanji conversion ⏳ |
| F17 | Korean: 한자 (Microsoft's, words and syllables) | Korean: Hanja for **single syllables only**. Japanese: transient romaji. English: transient Caps Lock |
| Symbols in Korean | **US ANSI** (the compromise) | **JIS**, matching the keycaps. FKeyIME types every character itself from its own table in all three modes (owner's decision after M0 showed Windows' layout substitution doesn't work). Not built yet ⏳ |
| Mode across windows | Per window, so a window can drift back to Korean (`issues/windows-inconsistent-symbols.md`) | One global mode. Measured in M0: a switch takes about 5 ms, and every open app follows within about 25 ms (median), with no on-screen popup |
| Elevated (admin) windows | Keys don't switch (UIPI) | Switch works, the TIP loads in-process ⏳ |
| Password fields, UAC, sign-in screen | AutoHotkey still switches the language | F17–F20 do nothing there (TIPs aren't loaded) |
| Fn1+P → Play/Pause | AutoHotkey | **Out of scope**: FKeyIME only handles language input. See "Fn1+P" below |
| Japanese conversion | Microsoft (cloud suggestions, your MS user dictionary) | Mozc (offline, different results). Your MS dictionary doesn't carry over |
| Korean extras | Full Microsoft Korean IME (3-Set, old Hangul, …) | 2-Set only in v1 |
| Raw-key apps (games, some terminals) | JIS keys in English and Japanese | They read keys, not text, so they see the Windows layout, not FKeyIME's table |
| A Korean keyboard's 한/영 key | Toggles inside the Microsoft Korean IME | Not handled. Only F17–F20 switch modes |

**FKeyIME adds dependencies** that v1.5.2 doesn't have: an MSI that installs system-wide COM DLLs
(x64 + x86) and needs admin rights, the bundled Mozc conversion engine (`mozc_server`), and binaries
built from this separate BSD-3-Clause repo instead of a script you can read in the docs.

## What stays the same

- **The keyboard.** DIP switches all OFF, keymap **Profile 4**. No change in the keymap tool.
- **Japanese keyboard (106/109) as the hardware layout** (v1.5.2 `docs/windows/02-windows.md`,
  step 3). Keep it: FKeyIME types its own JIS characters, but apps that read raw keys still use
  the Windows layout. Don't switch to "Detect connected keyboard".
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
   v1.5.2 script runs, its keyboard hook takes F17–F20 and FKeyIME never sees them):

   ```powershell
   Stop-ScheduledTask hhkb-ekj-trilingual
   Get-CimInstance Win32_Process -Filter "Name='AutoHotkey64.exe'" |
     Where-Object CommandLine -match 'hhkb-ekj-trilingual' | Invoke-CimMethod -MethodName Terminate
   Disable-ScheduledTask hhkb-ekj-trilingual   # disable rather than delete until FKeyIME is verified
   ```

   AutoHotkey itself can stay installed.

3. ⏳ **Install FKeyIME** (M5): run `fkeyime-<version>-x64.msi` from a FKeyIME release. It
   installs both the x64 and x86 DLLs and adds **FKeyIME EN / KO / JA** to your language list.
   The wizard first lists your keyboards, mice and PC info (nothing is sent anywhere). There's
   no ARM64 build.

4. ⏳ **Microsoft IMEs** (M5): removing them from the language list is optional, and **keeping
   them is the default**: clicking Next through the wizard keeps them. On a laptop, or with a
   wired mouse connected, the wizard suggests removing them and it's one click to accept.
   Otherwise it's two clicks (start, then confirm). They stay installed either way, and you can
   remove them later in Settings → Time & language → Language & region. Turn **off** "Let me use a different input method for each app
   window" (Settings → Time & language → Typing → Advanced keyboard settings).

5. ⏳ **Verify** (M6): sign out and back in, then check F18 → `안녕하세요`, F19 → `the"` with
   Shift+2 → `"`, F20 → `3` gives あ, F17 in each mode, and the symbols in Korean
   (``@[]:;"`` should now match the keycaps).

6. Once it's verified, delete the logon task: `Unregister-ScheduledTask hhkb-ekj-trilingual`.

### Fn1+P (Play/Pause)

v1.5.2 turned Fn1+P (`Pause`) into Play/Pause with AutoHotkey. FKeyIME won't do this: key
bindings that aren't language input are out of its scope. If you want it, keep a one-line
AutoHotkey script (`Pause::Send("{Media_Play_Pause}")`) on its own logon task, without the
F-key lines (those would take F17–F20 away from FKeyIME), or use another tool you trust.

## Rolling back to v1.5.2

1. ⏳ Uninstall FKeyIME (Settings → Apps). The uninstaller puts the Microsoft IMEs back in the
   language list. If it doesn't, restore them from the backup in step 1.
2. `Enable-ScheduledTask hhkb-ekj-trilingual; Start-ScheduledTask hhkb-ekj-trilingual`
3. Check that F18–F20 work again as in v1.5.2 `docs/windows/03-autohotkey.md`.

## Changelog of this guide

One entry per FKeyIME commit (newest first).

- **Renamed to FKeyIME** (was FKeyIDE; repo `themrsung/fkeyime`, old URL redirects). Only the
  name changed. Still nothing to install.
- **Owner's decisions after M0.** FKeyIME types every character from its own JIS table in all
  three modes, so Korean gets JIS symbols (rows updated). Global mode, Hanja for single
  syllables, no ARM64 build. Removing the Microsoft IMEs is optional and keeping them is the
  wizard's default (step 4). Fn1+P and other non-language bindings are out of scope (rewritten
  section). New rows: raw-key apps, the 한/영 key. Still nothing to install.
- **한/영 key removed.** The optional Korean ↔ English toggle for a Korean keyboard's 한/영 key is
  gone again (owner's decision): only F17–F20 switch modes. Still nothing to install.
- **M0 done.** Status banner updated: M0 finished, still nothing to install. "Mode across
  windows" now gives the measured switch time (about 5 ms, other apps within about 25 ms, no
  popup). The fix for the US symbols in Korean and English is still the owner's choice
  (`spec.md`, open question 8; proposed: English as a Japanese-language profile like v1.5.2,
  Korean typing JIS symbols itself). Mozc builds on Windows (`toolchain.md`).
- **Optional 한/영 key.** New row in "What changes for you": a Korean keyboard's 한/영 key can
  toggle Korean ↔ English (from Japanese it goes to Korean). Off by default, and not tested on
  a real Korean keyboard yet. Still nothing to install.
- **Spike uninstall cleanup.** No user-visible change (`tools/spike.ps1` is for development only).
- **Spec wording fix.** No user-visible change.
- **M0 spike, work in progress.** The TIP now registers the three profiles and handles F18–F20
  (development install via `tools/spike.ps1`, not for end users). Measured: Korean and English
  profiles get US symbols, not JIS, so the Korean-symbols row now says so; AutoHotkey must be
  stopped first because its hook takes the F keys. Still nothing installable.
- **Repo setup, toolchain, mode logic.** First version. Nothing is installable yet. Steps 1, 2
  and 6 and the rollback are runnable. The others wait on M0–M6.
