# For agents: upgrading hhkb-ekj-trilingual to v2.0.0 (FKeyIDE or AutoHotkey)

Audience: a coding agent working in
[`themrsung/hhkb-ekj-trilingual`](https://github.com/themrsung/hhkb-ekj-trilingual). This file says
what to change in that repo to release **v2.0.0**. In v2.0.0, Windows users **choose** between
FKeyIDE (this repo's IME) and the **legacy AutoHotkey switching** from v1.5.2. The agent explains
the tradeoffs below and asks the user to choose.

This file is updated with every FKeyIDE commit. Read the version at the FKeyIDE tag you're
targeting, not `main`.

## 0. Readiness gate (check first)

> [!IMPORTANT]
> **Status at this commit: not ready. Don't release v2.0.0 yet.** FKeyIDE has no installable
> build (toolchain, mode logic and an empty TIP DLL only). You may prepare the changes on a branch,
> but don't tag, merge to `main` or push without the owner's OK.

v2.0.0 can ship once **all** of these are true. Check each one against the FKeyIDE release,
don't assume:

- [ ] A tagged FKeyIDE release with a signed MSI (milestone M5).
- [ ] The M6 soak-test results are published, with failure rates per app.
- [ ] Korean gets **JIS symbols** under FKeyIDE. M0 measured that it **doesn't yet**: the
      registered JIS substitute layout is ignored, so FKeyIDE KO and EN type US symbols. Wait for
      the fix (spec open question 8), or rewrite the tradeoff table below before release.
- [ ] `docs/migration-from-v1.5.2.md` in FKeyIDE has no ⏳ steps left.
- [ ] Open question 7 (Fn1+P → Play/Pause without AutoHotkey) is decided.

## 1. Why a major version

- The Windows setup becomes a **choice between two paths**. The interactive prompt asks a new
  question, and AGENTS.md section 6 is restructured.
- Path B adds **external dependencies**: the repo stops being self-contained for Windows.
- The macOS setup and the HHKB Studio keymap (`docs/hhkb-studio-setting-all.hks`) are unchanged.
  Profile 4 serves both Windows paths. Say this in the release notes so macOS users know they
  can skip v2.0.0.

## 2. Rules to keep

- The repo stays **CC0 and docs-only**. No FKeyIDE source, binaries or installer go in it. Link to
  a FKeyIDE **release tag** and give the SHA-256 of the MSI.
- Path A (AutoHotkey) stays fully documented and working. It's the legacy option, not a deprecated
  one. Don't delete or weaken `docs/windows/03-autohotkey.md`.
- Work from `main` at `v1.5.2` (`git switch -c release/2.0.0 v1.5.2`). The `windows-support`
  branch is older (cut from v1.4.0) and superseded. Don't base the release on it.
- Commit in small steps. Don't tag, push or touch `main` without the owner's OK.
- Measure, don't assume. Mark anything inferred.

## 3. The tradeoffs (what the agent must explain to the user)

Tell the user plainly: **choosing FKeyIDE installs dependencies.** Path A has one (AutoHotkey).

| | Path A: AutoHotkey (legacy, v1.5.2) | Path B: FKeyIDE |
| --- | --- | --- |
| **Dependencies** | AutoHotkey v2 + a script in the docs + a logon task. All user-level, readable | **FKeyIDE MSI**: system-wide in-process COM DLLs (x64 + x86) loaded into every app that takes text input, the bundled **Mozc** conversion engine (`mozc_server`), binaries from a separate BSD-3-Clause repo. Needs admin to install. Updates come from FKeyIDE releases |
| Korean | Microsoft Korean IME, **US ANSI symbols** (the compromise) | FKeyIDE's own 2-Set. Goal JIS symbols, but **US symbols as measured in M0** until open question 8 is fixed. No 3-Set, no old Hangul |
| English | Japanese with the IME off: JIS symbols, taskbar shows JA | Real en-US profile, taskbar shows EN. Goal JIS symbols; **US symbols as measured in M0** until open question 8 is fixed |
| Japanese | Microsoft Japanese IME (cloud suggestions, MS user dictionary) | Mozc (offline, different conversion, no MS dictionary) |
| F17 | 한자 in Korean only | Hanja / transient romaji / transient Caps Lock |
| 한/영 key (Korean keyboards) | Microsoft Korean IME toggle, while Korean is active | Optional, off by default: Korean → English, otherwise → Korean. Take the default from the release (open question 9) |
| Consistency | Per-window language, can drift (`issues/windows-inconsistent-symbols.md`); 150 ms wait-and-retry | One global mode, no message posting or retries |
| Elevated (admin) windows | Doesn't switch (UIPI) | Works |
| Password fields, UAC, sign-in | Switches the language | F17–F20 do nothing |
| Fn1+P → Play/Pause | Yes | Not by FKeyIDE (open question 7) |
| Removal | Delete the task and the script | Uninstall the MSI; it restores the Microsoft IMEs |
| Maturity | Tested on the owner's PC, USB and Bluetooth | Newer. See the FKeyIDE release's soak-test results |

Recommendation to give: Path A if they want nothing installed system-wide, use Korean mostly for
Hangul, or rely on the Microsoft IMEs' dictionaries. Path B if they want JIS symbols everywhere and
one consistent mode, and accept the dependencies.

## 4. Changes, file by file

| File | Change |
| --- | --- |
| `README.md` | Windows note: replace the single compromise table with a short "two paths" summary (Path A: AutoHotkey, Path B: FKeyIDE, with the dependency warning). Pin every `v1.5.2` → `v2.0.0` (`git grep -n 'v1\.5\.2'`). In the interactive prompt, Windows bullet: "explain both Windows paths and their tradeoffs (AGENTS.md section 6), say that FKeyIDE installs dependencies, and ask which one I want" |
| `AGENTS.md` | Title: drop "experimental" only if the owner says so. Add `docs/windows/04-fkeyide.md` to the doc table. Rewrite section 6 (below) |
| `docs/windows/01-hardware.md` | The Profile 4 table's "Windows meaning" column gets one column per path. F17: "Path A: 한자 in Korean. Path B: Hanja / romaji / Caps Lock". Bluetooth: "AutoHotkey acts on the key" becomes "both paths act on the key, not the device". No keymap change |
| `docs/windows/02-windows.md` | Mark each step with the path it applies to. Step 2 (languages): Path A unchanged; Path B: FKeyIDE adds its own EN/KO/JA profiles, and removing the Microsoft IMEs from the list is optional. Step 3 (106/109): **both paths**. Path B note: once FKeyIDE gives Korean the JIS layout (open question 8; not working in M0), the Korean options 1–3 are Path A only. Check the release before writing this. Steps 4–5 (Microsoft IME settings): Path A only. Add Path B: "Let me use a different input method for each app window" = off |
| `docs/windows/03-autohotkey.md` | Title: "Windows 03 — Path A (legacy): AutoHotkey v2". Add one line pointing to 04 for the alternative. Content unchanged |
| `docs/windows/04-fkeyide.md` (new) | Path B: what FKeyIDE is, the **dependency list**, install (MSI from the release tag, check the SHA-256), settings, verification (same checks as 01's Bluetooth table plus Korean symbols, F17 in each mode, an elevated app), uninstall and rollback to Path A. For migrating from v1.5.2, link to `docs/migration-from-v1.5.2.md` **at the FKeyIDE tag** instead of copying it |
| `issues/windows-inconsistent-symbols.md` | "Applies to: Path A". Add: on Path B, Korean uses JIS too and the mode is global, so this shouldn't happen. Confirm with the M6 results before writing it as fact |
| `issues/README.md` | Update that row. Add an FKeyIDE row if the release lists known issues (e.g. F-keys do nothing in password fields) |
| `docs/hhkb-studio-setting-all.hks` | No change, unless open question 7 moved Play/Pause into the keymap. Then update it and the Profile 4 screenshots together |
| `CLAUDE.md`, `LICENSE`, macOS docs | No change |

### AGENTS.md section 6, new shape

1. Scope line: Windows 11, HHKB Studio JIS only, **two paths**.
2. "Choose a path": the table from section 3 above (trimmed), the sentence "FKeyIDE installs
   dependencies", and the instruction to **ask the user and wait**. Default when the user has no
   preference: ask the owner which one (don't guess). Also check whether Path A is already set up
   (`Get-ScheduledTask hhkb-ekj-trilingual`). If it is and they choose B, follow the FKeyIDE
   migration guide.
3. Order of operations, Path A: the existing v1.5.2 steps 1–4, unchanged.
4. Order of operations, Path B: (1) hardware: same Profile 4; (2) Windows: 106/109 layout
   (02 step 3), restart; (3) install FKeyIDE (04); (4) verify (04).
5. Keep "Every install and every system-setting change needs the user's OK first. Measure; don't
   assume."

## 5. Values to take from the FKeyIDE release

Copy these from the release you link, not from memory. Fixed values (permanent, from
`src/tip/guids.h`):

| Value | |
| --- | --- |
| CLSID | `{66749D2A-1770-43D3-A269-A051B5BC1AAA}` |
| Profile EN (en-US) | `0409:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{8F61A2C0-75BC-4042-B739-CC7D63B542E5}` |
| Profile KO (ko-KR) | `0412:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{28C201BF-390E-482F-8259-CD26E05844A9}` |
| Profile JA (ja-JP) | `0411:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{1AC13F3E-07AC-4B03-A606-967BD48BFD99}` |

Per release (not known yet): release tag, MSI file name(s) and SHA-256, signing certificate
subject, minimum Windows build, ARM64 availability, the decided open questions (`spec.md`,
section 5).

## 6. Done means

- [ ] `git grep 'v1\.5\.2'` shows only history or "migrating from" mentions.
- [ ] A fresh agent following the v2.0.0 prompt on Windows explains both paths, mentions the
      dependencies, and asks.
- [ ] Path A still works from the docs alone on a clean PC.
- [ ] Path B works from the docs alone on a clean PC, and the migration from v1.5.2 Path A works.
- [ ] Release notes: macOS unchanged; Windows: new FKeyIDE option, AutoHotkey kept as legacy.
- [ ] Owner approved; then tag `v2.0.0`.

## Changelog of this guide

One entry per FKeyIDE commit (newest first).

- **Optional 한/영 key.** Added a 한/영 row to the tradeoff table (optional, off by default,
  untested on a real Korean keyboard). Readiness gate unchanged: not ready. In 04-fkeyide.md,
  mention the option only if the release ships it.

- **Spike uninstall cleanup.** No change for the 2.0.0 work. Still not ready.
- **Spec wording fix.** No change for the 2.0.0 work. Note: commit 7652657's message says
  spec section 6 and open question 8 weren't written yet; they were, and are in that commit.
- **M0 spike, work in progress.** Still not ready. Measured: FKeyIDE KO/EN get US symbols, not
  JIS (registered substitute layout ignored), so the readiness gate and the Korean/English rows
  now say so. Measured: v1.5.2's AutoHotkey hook takes F17–F20 from FKeyIDE, so Path B docs
  must have users stop the AutoHotkey task first. The unsigned DLL loads in packaged apps.
- **Repo setup, toolchain, mode logic.** First version. Readiness gate: not ready (nothing
  installable). The profile GUIDs are fixed from this commit on.
