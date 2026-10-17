# For agents: upgrading hhkb-ekj-trilingual to v2.0.0 (FKeyIME or AutoHotkey)

Audience: a coding agent working in
[`themrsung/hhkb-ekj-trilingual`](https://github.com/themrsung/hhkb-ekj-trilingual). This file says
what to change in that repo to release **v2.0.0**. In v2.0.0, Windows users **choose** between
FKeyIME (this repo's IME) and the **legacy AutoHotkey switching** from v1.5.2. The agent explains
the tradeoffs below and asks the user to choose.

This file is updated with every FKeyIME commit. Read the version at the FKeyIME tag you're
targeting, not `main`.

**A preview exists:** branch `experimental/2.0.0-preview` (commit `4d88e7d`) on top of `v1.5.3`,
made from this guide. Differences from the plan below: it's based on `v1.5.3` (which added the
104/108-key page; that page now says Path B doesn't fit ANSI keycaps), pins FKeyIME by commit
(`33209e2`) because there's no release yet, installs Path B from source with `tools/spike.ps1`,
and its README prompt is pinned to the branch, not a tag. No tag was created and `main` wasn't
touched. For the real v2.0.0, start from that branch and swap the source install for the MSI
once the gate below is met.

## 0. Readiness gate (check first)

> [!IMPORTANT]
> **Status at this commit: not ready. Don't release v2.0.0 yet.** FKeyIME has no installable
> build. Typing and kanji conversion work in a development build (FKeyIME spec, sections 8 and
> 9), but there's no Hanja, romaji input or MSI, and the conversion server still runs without
> Mozc's sandbox (spec section 9, "Security"). You may prepare the changes on a branch, but don't tag,
> merge to `main` or push without the owner's OK.

v2.0.0 can ship once **all** of these are true. Check each one against the FKeyIME release,
don't assume:

- [ ] A tagged FKeyIME release with a signed MSI (milestone M5).
- [ ] The M6 soak-test results are published, with failure rates per app.
- [ ] Korean and English get **JIS symbols** under FKeyIME, measured on the release. Decided
      approach: FKeyIME types every character from its own JIS table in all modes (M0 showed
      Windows' layout substitution doesn't work). If the release doesn't deliver it, rewrite the
      tradeoff table below before release.
- [ ] `docs/migration-from-v1.5.2.md` in FKeyIME has no ⏳ steps left.
- [ ] The installer wizard behaves as specified (FKeyIME spec, section 7): keeping the Microsoft
      IMEs is the default.
- [ ] The conversion server runs sandboxed and starts without an app having to launch it
      (FKeyIME spec, section 9, "Security" and "Lifetime").

## 1. Why a major version

- The Windows setup becomes a **choice between two paths**. The interactive prompt asks a new
  question, and AGENTS.md section 6 is restructured.
- Path B adds **external dependencies**: the repo stops being self-contained for Windows.
- The macOS setup and the HHKB Studio keymap (`docs/hhkb-studio-setting-all.hks`) are unchanged.
  Profile 4 serves both Windows paths. Say this in the release notes so macOS users know they
  can skip v2.0.0.

## 2. Rules to keep

- The repo stays **CC0 and docs-only**. No FKeyIME source, binaries or installer go in it. Link to
  a FKeyIME **release tag** and give the SHA-256 of the MSI.
- Path A (AutoHotkey) stays fully documented and working. It's the legacy option, not a deprecated
  one. Don't delete or weaken `docs/windows/03-autohotkey.md`.
- Work from `main` at `v1.5.2` (`git switch -c release/2.0.0 v1.5.2`). The `windows-support`
  branch is older (cut from v1.4.0) and superseded. Don't base the release on it.
- Commit in small steps. Don't tag, push or touch `main` without the owner's OK.
- Measure, don't assume. Mark anything inferred.

## 3. The tradeoffs (what the agent must explain to the user)

Tell the user plainly: **choosing FKeyIME installs dependencies.** Path A has one (AutoHotkey).

| | Path A: AutoHotkey (legacy, v1.5.2) | Path B: FKeyIME |
| --- | --- | --- |
| **Dependencies** | AutoHotkey v2 + a script in the docs + a logon task. All user-level, readable | **FKeyIME MSI**: system-wide in-process COM DLLs (x64 + x86) loaded into every app that takes text input, the bundled **Mozc** conversion server (`fkeyime_mozc_server.exe`, about 23 MB of memory while running, its own profile folder `%LOCALAPPDATA%Low\FKeyIME Mozc` with what it learned), binaries from a separate BSD-3-Clause repo. Needs admin to install. Updates come from FKeyIME releases |
| Korean | Microsoft Korean IME, **US ANSI symbols** (the compromise), full Hanja | FKeyIME's own 2-Set, **JIS symbols** from its own table (decided; check the release), or US symbols with the `KeyboardLayout=1` setting on an ANSI board (built, not measured yet). Hanja for single syllables only. No 3-Set, no old Hangul |
| English | Japanese with the IME off: JIS symbols, taskbar shows JA | Real en-US profile, taskbar shows EN, JIS symbols from FKeyIME's own table. Apps that read raw keys (games, some terminals) still see the Windows layout |
| Japanese | Microsoft Japanese IME (cloud suggestions, MS user dictionary) | FKeyIME's own IME and kana table (F6/F7/F8 like macOS), with a bundled, renamed `mozc_server` for conversion only (Option B, decided): offline, different conversion, no MS dictionary |
| F17 | 한자 in Korean only | Hanja / transient other Japanese input / transient Caps Lock |
| HHKB Studio on the macOS keymap (Profile1) | Not supported (Profile 4 required) | Opt-in `MacLangKeys`: 英数 → English, かな → Japanese, F18 → Korean, **no F17**. The installer asks "Do you have an HHKB Studio?" (M5). Measured on the owner's Studio |
| F16 | Not used | Japanese with romaji input (needs a key set to F16; FKeyIME spec, open question 11) |
| Keyboard without F16-F20 | Needs the keymap (HHKB Profile 4, or a remap tool) | Same, or the optional escape hatch (off by default, no remap): ``Alt+` `` (or Menu) cycles English → Korean → Japanese, ``Alt+Shift+` `` acts as F17. Probe-tested, not yet on a keyboard |
| Both installed | — | Supported with the FKeyIME-aware AutoHotkey script. Win+Space picks: with FKeyIME chosen, F-keys go to FKeyIME everywhere (windows still on a Microsoft IME are moved to it); with a Microsoft IME chosen, AutoHotkey switches as in v1.5.2 |
| Consistency | Per-window language, can drift (`issues/windows-inconsistent-symbols.md`); 150 ms wait-and-retry | One global mode, no message posting or retries. M0 measured about 5 ms per switch, with every app following within about 25 ms (median) |
| Elevated (admin) windows | Doesn't switch (UIPI) | Works |
| Password fields, UAC, sign-in | Switches the language | F17–F20 do nothing |
| Fn1+P → Play/Pause | Yes | **Out of scope** for FKeyIME (it only handles language input). Path B docs may offer an optional AutoHotkey one-liner for it, without the F-key lines; ask the owner |
| Microsoft IMEs | Required (they do the typing) | Optional. The installer wizard keeps them by default and suggests removing them on a laptop or with a wired mouse |
| Removal | Delete the task and the script | Uninstall the MSI; it restores the Microsoft IMEs to the language list |
| ARM64 PCs | AutoHotkey runs (x64 emulation) | No ARM64 build. Not blocked, so a fork could add one |
| Maturity | Tested on the owner's PC, USB and Bluetooth | Newer. See the FKeyIME release's soak-test results |

Recommendation to give: Path A if they want nothing installed system-wide, use Korean mostly for
Hangul, or rely on the Microsoft IMEs' dictionaries. Path B if they want JIS symbols everywhere and
one consistent mode, and accept the dependencies.

## 4. Changes, file by file

| File | Change |
| --- | --- |
| `README.md` | Windows note: replace the single compromise table with a short "two paths" summary (Path A: AutoHotkey, Path B: FKeyIME, with the dependency warning). Pin every `v1.5.2` → `v2.0.0` (`git grep -n 'v1\.5\.2'`). In the interactive prompt, Windows bullet: "explain both Windows paths and their tradeoffs (AGENTS.md section 6), say that FKeyIME installs dependencies, and ask which one I want" |
| `AGENTS.md` | Title: drop "experimental" only if the owner says so. Add `docs/windows/04-fkeyime.md` to the doc table. Rewrite section 6 (below) |
| `docs/windows/01-hardware.md` | The Profile 4 table's "Windows meaning" column gets one column per path. F17: "Path A: 한자 in Korean. Path B: Hanja / romaji / Caps Lock". Bluetooth: "AutoHotkey acts on the key" becomes "both paths act on the key, not the device". No keymap change |
| `docs/windows/02-windows.md` | Mark each step with the path it applies to. Step 2 (languages): Path A unchanged; Path B: FKeyIME adds its own EN/KO/JA profiles, and the installer wizard handles the Microsoft IMEs (kept by default). Step 3 (106/109): **both paths** (Path B keeps it for raw-key apps). Path B note: FKeyIME types JIS symbols in Korean itself, so the Korean options 1–3 are Path A only. Check the release before writing this. Steps 4–5 (Microsoft IME settings): Path A only. Add Path B: "Let me use a different input method for each app window" = off |
| `docs/windows/03-autohotkey.md` | Title: "Windows 03 — Path A (legacy): AutoHotkey v2". Add one line pointing to 04 for the alternative. Replace the script with FKeyIME's `examples/autohotkey/hhkb-ekj-trilingual.ahk` **at the FKeyIME release tag** (CC0-compatible: it's v1.5.2's script plus the FKeyIME check). Without FKeyIME it behaves exactly like v1.5.2's, so Path A users lose nothing. Explain the three changes in a short section, linking to FKeyIME's `docs/api.md` |
| `docs/windows/04-fkeyime.md` (new) | Path B: what FKeyIME is, the **dependency list**, install (MSI from the release tag, check the SHA-256), settings, verification (same checks as 01's Bluetooth table plus Korean symbols, F17 in each mode, an elevated app), uninstall and rollback to Path A. For migrating from v1.5.2, link to `docs/migration-from-v1.5.2.md` **at the FKeyIME tag** instead of copying it |
| `issues/windows-inconsistent-symbols.md` | "Applies to: Path A". Add: on Path B, Korean uses JIS too and the mode is global, so this shouldn't happen. Confirm with the M6 results before writing it as fact |
| `issues/README.md` | Update that row. Add an FKeyIME row if the release lists known issues (e.g. F-keys do nothing in password fields) |
| `docs/hhkb-studio-setting-all.hks` | Add an F16 key (HID usage 107) to Profile 4 once open question 11 picks the key, and update the Profile 4 screenshots and tables with it. No other change |
| `CLAUDE.md`, `LICENSE`, macOS docs | No change |

### AGENTS.md section 6, new shape

1. Scope line: Windows 11, HHKB Studio JIS only, **two paths**.
2. "Choose a path": the table from section 3 above (trimmed), the sentence "FKeyIME installs
   dependencies", and the instruction to **ask the user and wait**. Default when the user has no
   preference: ask the owner which one (don't guess). Also check whether Path A is already set up
   (`Get-ScheduledTask hhkb-ekj-trilingual`). If it is and they choose B, follow the FKeyIME
   migration guide: they can keep AutoHotkey with the FKeyIME-aware script (both installed) or
   stop it.
3. Order of operations, Path A: the existing v1.5.2 steps 1–4, unchanged.
4. Order of operations, Path B: (1) hardware: same Profile 4; (2) Windows: 106/109 layout
   (02 step 3), restart; (3) install FKeyIME (04); (4) verify (04).
5. Keep "Every install and every system-setting change needs the user's OK first. Measure; don't
   assume."

## 5. Values to take from the FKeyIME release

Copy these from the release you link, not from memory. Fixed values (permanent, from
`src/tip/guids.h`):

| Value | |
| --- | --- |
| CLSID | `{66749D2A-1770-43D3-A269-A051B5BC1AAA}` |
| Profile EN (en-US) | `0409:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{8F61A2C0-75BC-4042-B739-CC7D63B542E5}` |
| Profile KO (ko-KR) | `0412:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{28C201BF-390E-482F-8259-CD26E05844A9}` |
| Profile JA (ja-JP) | `0411:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{1AC13F3E-07AC-4B03-A606-967BD48BFD99}` |

Per release (not known yet): release tag, MSI file name(s) and SHA-256, signing certificate
subject, minimum Windows build, and any decisions made after this commit (`spec.md`,
section 5). ARM64: none (not shipped).

## 6. Done means

- [ ] `git grep 'v1\.5\.2'` shows only history or "migrating from" mentions.
- [ ] A fresh agent following the v2.0.0 prompt on Windows explains both paths, mentions the
      dependencies, and asks.
- [ ] Path A still works from the docs alone on a clean PC.
- [ ] Path B works from the docs alone on a clean PC, and the migration from v1.5.2 Path A works.
- [ ] Release notes: macOS unchanged; Windows: new FKeyIME option, AutoHotkey kept as legacy.
- [ ] Owner approved; then tag `v2.0.0`.

## Changelog of this guide

One entry per FKeyIME commit (newest first).

- **Settings listing, still open.** Measured that Windows Settings lists another third-party IME
  (Mozc) but not FKeyIME; the remaining suspect is FKeyIME's three languages under one CLSID.
  Keep pointing to the Start-menu settings shortcut. Still not ready.

- **Typo fix in docs/issues.** No change for the 2.0.0 work.

- **Registration cleanup.** Display names and an icon for the three profiles, no substitute
  layout. The "Settings doesn't list FKeyIME" issue is still open: in `04-fkeyime.md`, point
  users to the Start-menu "FKeyIME settings" rather than Windows Settings. Still not ready.

- **Installer, first version.** An MSI builds and passes ICE validation, not yet installed on a
  PC, unsigned, without the device page or the IME-removal choice. Readiness gate unchanged (no
  signed, tagged MSI). Known issue for `04-fkeyime.md`: Windows Settings doesn't list FKeyIME's
  keyboards; its settings window is in the Start menu.

- **Settings window.** FKeyIME now has Report an issue and Copy system details (with an opt-in
  for AutoHotkey details). In `04-fkeyime.md`'s troubleshooting, tell users to attach the copied
  details to FKeyIME issues. Still not ready.

- **Mac 英数 / かな keys (opt-in).** New tradeoff row: with `MacLangKeys`, the Studio's Profile1
  (macOS keymap) works on Path B too, without F17. In `04-fkeyime.md`, mention it as optional
  only; Profile 4 stays the documented keymap. Still not ready.

- **Default input method: not solved.** Measured after a reboot: the override alone didn't start
  the session in FKeyIME (`docs/issues/sign-in-starts-on-microsoft-ime.md`, reopened). In
  `04-fkeyime.md`, keep the override step but add "after signing in, pick FKeyIME once with
  Win+Space" until a fix is measured. Still not ready.

- **Settings apply without restarting apps.** FKeyIME's options now take effect at the next key in
  running apps (registry change notification; FKeyIME spec section 8). Path B docs can say that
  changing `KeyboardLayout` or the escape-hatch settings needs no restart, but updating FKeyIME
  itself still does (restart apps or sign out). Still not ready.
- **US ANSI keyboards, and an escape hatch without F16-F20.** FKeyIME now has, off by default
  in `HKLM\Software\fkeyime` (FKeyIME spec, sections 7 and 8):
  - `KeyboardLayout=1` for US ANSI boards, with `AnsiExtraKeys` for ー and ろ (the `` ` `` key, or
    F14 / F15 from the keyboard's firmware);
  - for keyboards that can't send F16-F20, no remap needed: ``Alt+` `` (or Menu) cycles English →
    Korean → Japanese (`CycleKeys`, `CycleJapanese`), ``Alt+Shift+` `` acts as F17 (`F17Keys`).

  The preview's 104/108-key page says Path B doesn't fit ANSI keycaps. Once these are tried on a
  keyboard (so far only FKeyIME's test program), that page can offer Path B with them; for the
  F14 / F15 route, say which keys to set in the keymap tool. Still not ready.
- **Issue notes.** FKeyIME's `docs/issues/` lists the problems met so far with causes and
  solutions. Path B's `issues/` entries in hhkb-ekj-trilingual can link to them at the pinned
  commit instead of repeating them. Still not ready.
- **Default input method.** The preview's `docs/windows/04-fkeyime.md` (branch
  `experimental/2.0.0-preview`) needs one more install step, next to "one input method for all
  windows": set "Override for default input method" to FKeyIME EN, or sign-in starts on a
  Microsoft IME (FKeyIME spec, section 3). Not yet applied to that branch. Still not ready.
- **2.0.0-preview pushed.** hhkb-ekj-trilingual's `experimental/2.0.0-preview` branch carries the
  changes from section 4 on top of v1.5.3, with FKeyIME pinned at 33209e2. New note above the
  gate lists how the preview differs from the plan. The gate itself is still not met.
- **Conversion server watch-dog fix.** No change for the 2.0.0 work. Path B's verification could
  include FKeyIME's `tools/test-mozc-server.ps1` (the server survives watch dog cycles). Still not
  ready.
- **Restart apps after installing.** Path B docs (04-fkeyime.md) must tell users to sign out or
  restart their apps after installing or updating FKeyIME: running apps may not load it (spec
  section 3). Still not ready.
- **AutoHotkey script: Win+Space decides.** The "Both installed" row now describes the session
  check. Copy the script at the FKeyIME release tag, not an older one: the first FKeyIME-aware
  version could leave every window on the Microsoft IMEs. Still not ready.
- **Diagnostic probe modes.** No change for the 2.0.0 work. Path A note: Hanja on the AutoHotkey
  path picked correctly in tests (digit, Enter, Down+Enter) in a Win32 Edit and in Notepad.
- **Kanji conversion (development build).** Still not ready. The Dependencies row now names the
  server process, its memory and its profile folder. New gate condition from the spec: the
  server must run sandboxed before release (spec section 9, "Security").
- **Typing works (development build).** Still not ready: no MSI, kanji conversion, Hanja or romaji.
  The Japanese row mentions F6/F7/F8. Path B's 04-fkeyime.md verification should reuse FKeyIME's
  `fkeyime_probe.exe --typing` cases (spec section 8) as its checklist.
- **API and F16.** FKeyIME now has a public API (`docs/api.md`), and an FKeyIME-aware version of
  the v1.5.2 AutoHotkey script (`examples/autohotkey/`) lets both be installed (measured). The
  03-autohotkey.md row now replaces the script with it. New rows: F16 (Japanese romaji) and
  "Both installed"; the .hks row waits on open question 11 for the F16 key. Still not ready.
- **Spike install fix.** Development script only; no change for the 2.0.0 work. Still not ready.
- **Renamed to FKeyIME** (was FKeyIDE). Use FKeyIME, `fkeyime` and `docs/windows/04-fkeyime.md`
  in the hhkb-ekj-trilingual changes, and link to `github.com/themrsung/fkeyime`. GUIDs
  unchanged. Still not ready.
- **Owner's decisions after M0.** Still not ready. The Korean-JIS gate item now names the decided
  approach (own JIS table in every mode); the Fn1+P gate item is replaced by the wizard default.
  Tradeoff table: Korean/English/Japanese rows rewritten, Fn1+P out of scope for FKeyIME, new
  rows for the Microsoft IMEs (optional, kept by default) and ARM64 (not shipped). 02-windows.md
  and .hks rows updated.
- **한/영 key removed.** The optional 한/영 toggle and its tradeoff-table row are gone (owner's
  decision). Don't mention a 한/영 key in 04-fkeyime.md. Readiness gate unchanged: not ready.
- **M0 done.** Readiness gate: still not ready. The status banner says M0 is finished. The
  Consistency row gives the measured switch time. The Japanese row names the proposed Mozc
  setup (Option B: a bundled, renamed `mozc_server`, so the dependency list in Path B will name
  it). The Korean-JIS gate item is unchanged: it waits on open question 8.
- **Optional 한/영 key.** Added a 한/영 row to the tradeoff table (optional, off by default,
  untested on a real Korean keyboard). Readiness gate unchanged: not ready. In 04-fkeyime.md,
  mention the option only if the release ships it.
- **Spike uninstall cleanup.** No change for the 2.0.0 work. Still not ready.
- **Spec wording fix.** No change for the 2.0.0 work. Note: commit 7652657's message says
  spec section 6 and open question 8 weren't written yet; they were, and are in that commit.
- **M0 spike, work in progress.** Still not ready. Measured: FKeyIME KO/EN get US symbols, not
  JIS (registered substitute layout ignored), so the readiness gate and the Korean/English rows
  now say so. Measured: v1.5.2's AutoHotkey hook takes F17–F20 from FKeyIME, so Path B docs
  must have users stop the AutoHotkey task first. The unsigned DLL loads in packaged apps.
- **Repo setup, toolchain, mode logic.** First version. Readiness gate: not ready (nothing
  installable). The profile GUIDs are fixed from this commit on.
