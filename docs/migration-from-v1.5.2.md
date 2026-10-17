# Migrating from hhkb-ekj-trilingual v1.5.2 to FKeyIME

hhkb-ekj-trilingual is now at `v1.5.3` (it only added a 104/108-key page; the Windows setup is the
same as v1.5.2), and its `experimental/2.0.0-preview` branch already offers FKeyIME as Path B
(`docs/windows/04-fkeyime.md` there). This guide applies to v1.5.3 too.

For people who set up Windows with
[`themrsung/hhkb-ekj-trilingual`](https://github.com/themrsung/hhkb-ekj-trilingual) at tag
**v1.5.2**: HHKB Studio JIS on keymap Profile 4, AutoHotkey v2 switching between the Microsoft
Korean and Japanese IMEs, started by the `hhkb-ekj-trilingual` logon task.

> [!IMPORTANT]
> **Status at this commit: no installer yet.** A development build types English (JIS), Korean
> 2-Set and Japanese kana (F6-F8) with kanji conversion, but has no Hanja or romaji input and
> installs only with a developer script. **Keep using v1.5.2 for daily typing.** The steps below
> describe the planned migration and get updated with every commit.
> Steps marked ⏳ depend on milestones that aren't done (`spec.md`, section 4).

## Who needs to migrate

| You use v1.5.2 on | What to do |
| --- | --- |
| **macOS** (Karabiner + Hammerspoon, Profile1) | Nothing. FKeyIME is Windows-only, and macOS is unchanged |
| **Windows**, Profile 4 + AutoHotkey | This guide, if you want FKeyIME. AutoHotkey stays supported as the legacy option |
| **Windows**, Profile3 fallback (no AutoHotkey) | Optional. FKeyIME needs Profile 4, below |
| HYBRID Type-S | Not applicable. FKeyIME targets the HHKB Studio JIS |
| A US ANSI board (104/108-key page) | Optional: FKeyIME has a US ANSI setting (`KeyboardLayout=1`, FKeyIME spec section 8): US symbols in English and Korean, kana at JIS positions, ー and ろ on the `` ` `` key or on F14 / F15 from the keyboard. Without remapping, an optional escape hatch: ``Alt+` `` (or Menu) cycles the modes, ``Alt+Shift+` `` acts as F17 (FKeyIME spec section 8, off by default). Passes FKeyIME's test program; not yet tried on a keyboard ⏳ |

## What changes for you

| | v1.5.2 (AutoHotkey) | FKeyIME |
| --- | --- | --- |
| F18 → Korean | Microsoft Korean IME, 한 mode | FKeyIME KO (own 2-Set engine) |
| F19 → English | Japanese language with the IME off (taskbar shows JA / A) | FKeyIME EN (en-US, taskbar shows EN), JIS symbols from FKeyIME's own key table ⏳ |
| F20 → Japanese | Microsoft Japanese IME, kana typing | FKeyIME JA: its own kana table (F6/F7/F8 to hiragana / katakana / half-width katakana, like macOS) and kanji conversion on Space through a bundled Mozc server, with a candidate window. Works in a development build |
| F17 | Korean: 한자 (Microsoft's, words and syllables) | Korean: Hanja for **single syllables only**. Japanese: transient switch to the other input (romaji after F20, kana after F16). English: transient Caps Lock |
| F16 | Not used | **New:** Japanese with romaji input. Needs a key set to F16 in the keymap tool; which key isn't decided yet (FKeyIME spec, open question 11) |
| Symbols in Korean | **US ANSI** (the compromise) | **JIS**, matching the keycaps. FKeyIME types every character itself from its own table in all three modes (owner's decision after M0 showed Windows' layout substitution doesn't work). Built. On a US ANSI board, the `KeyboardLayout=1` setting gives US symbols instead ⏳ |
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

2. **AutoHotkey: make it step aside, or stop it.** While the v1.5.2 script runs, its keyboard
   hook takes F17–F20 and FKeyIME never sees them *(measured in M0)*. Pick one:

   **a. Keep AutoHotkey, FKeyIME-aware** (works today; lets you try FKeyIME and go back with
   Win+Space). Replace your script with
   [`examples/autohotkey/hhkb-ekj-trilingual.ahk`](../examples/autohotkey/hhkb-ekj-trilingual.ahk)
   from this repo (your backup from step 1 stays), then restart it:
   `Stop-ScheduledTask hhkb-ekj-trilingual; Start-ScheduledTask hhkb-ekj-trilingual`.
   **Win+Space decides:** with FKeyIME chosen, F16–F20 go to FKeyIME everywhere (a window still
   on a Microsoft IME is moved to FKeyIME). With a Microsoft IME chosen, the keys work exactly as
   in v1.5.2 (`api.md`, measured). Without FKeyIME installed, it's the v1.5.2 script.

   **b. Stop AutoHotkey switching** (works today; for FKeyIME only):

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
   window", and set "Override for default input method" to **FKeyIME EN** (both in Settings →
   Time & language → Typing → Advanced keyboard settings). Without the override, each sign-in
   starts on the first input method in your language list (often a Microsoft IME), and the
   F-keys go to AutoHotkey until you pick FKeyIME with Win+Space *(measured after a reboot)*.

5. ⏳ **Verify** (M6): sign out and back in (apps that were running during the install may not
   use FKeyIME until they restart), then check F18 → `안녕하세요`, F19 → `the"` with
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

- **US ANSI keyboards, and an escape hatch without F16-F20.** Two groups of settings in
  `HKLM\Software\fkeyime`, all off by default, for US ANSI boards (FKeyIME spec, section 8).
  JIS users: no change.
  - `KeyboardLayout=1`: English and the symbols in Korean follow US keycaps. Kana keep their JIS
    positions; the two kana ANSI has no key for (ー, ろ) go on the `` ` `` key (Shift for ろ) or on
    F14 / F15 sent by the keyboard (`AnsiExtraKeys`). Set them in HKLM: Store apps such as
    Notepad don't see a per-user HKCU value.
  - For boards that can't send F16-F20, with no remap needed: ``Alt+` `` cycles English → Korean →
    Japanese → English (`CycleKeys=1`; Menu too with `2`), Japanese by kana or romaji
    (`CycleJapanese`), and ``Alt+Shift+` `` acts as F17 (`F17Keys=1`). The planned installer will
    ask.

  Both pass FKeyIME's test program in modern and classic text boxes, except that Menu still opens
  the right-click menu in old dialogs. Not yet tried on a keyboard. New issue notes: a test that
  froze mouse input, and why Right Win / Right Ctrl / Right Alt can't be used.
- **Issue notes.** New `docs/issues/`: every problem met so far (Backspace sending messages,
  apps not loading FKeyIME, sign-in on the Microsoft IME, AutoHotkey taking the keys, server
  crashes…) with cause and solution. Check it before reporting a problem.
- **Start in FKeyIME after signing in.** Step 4 now sets Windows' default input method to
  FKeyIME EN: without it, sign-in started on the Microsoft Korean IME and the F-keys went to
  AutoHotkey until FKeyIME was picked with Win+Space.
- **Preview of hhkb-ekj-trilingual 2.0.0.** Its `experimental/2.0.0-preview` branch now offers
  FKeyIME as Path B next to AutoHotkey. This guide also covers v1.5.3, whose Windows setup is
  unchanged from v1.5.2.
- **Conversion server no longer crashes every 3 minutes.** Mozc's watch dog aborted FKeyIME's
  server because it runs outside Mozc's install folder; conversion recovered on the next Space,
  with a short delay. Fixed with a Mozc patch. Still no installer.
- **Restart apps after installing.** Apps that were already running may keep typing with
  Windows' plain layouts (English letters in Korean and Japanese) until they restart; signing out
  restarts them all. Measured: Notepad typed Korean and kana again after a restart.
- **AutoHotkey script: Win+Space decides.** With FKeyIME chosen, the script no longer takes the
  F-keys anywhere and moves windows still on a Microsoft IME to FKeyIME. Fixes: after one F-key in
  a window without FKeyIME, every window stayed on the Microsoft IMEs and AutoHotkey kept taking
  the keys. Update your copy of the script (step 2a).
- **Diagnostic probe modes.** No user-visible change. A report of Hanja picks also typing the
  candidate number (Microsoft Korean IME through AutoHotkey, in Notepad) didn't reproduce with
  `fkeyime_probe.exe --hanja` / `--hanja-fg`, and went away; `--keylog` records key codes if it
  comes back.
- **Kanji conversion (development build).** Space converts with a bundled Mozc server: segments,
  candidate window, F6-F10, Escape back to kana. Mozc learns your choices in its own profile
  folder; your Microsoft IME dictionary doesn't carry over. Fixed: Shift no longer splits what
  you're typing. Still no installer, Hanja or romaji input.
- **Typing works (development build).** English JIS, Korean 2-Set with JIS symbols, and Japanese
  kana with F6-F8 type correctly in classic and TSF text boxes (24/24 measured cases, x64 and
  x86). Still no installer, kanji conversion, Hanja or romaji input.
- **API and F16.** Step 2 now offers keeping AutoHotkey with an FKeyIME-aware script
  (`examples/autohotkey/`, works today, measured) instead of only stopping it. New F16 row:
  Japanese with romaji input, and F17 in Japanese switches to the other input. Still nothing
  to install.
- **Spike install fix.** `tools/spike.ps1 -Install` now really adds the three FKeyIME profiles to
  the language list. Development only; no user-visible change. Still nothing to install.
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
