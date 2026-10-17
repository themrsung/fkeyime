# FKeyIDE: behavior spec and architecture

FKeyIDE is one Windows IME (a TSF text input processor) that **replaces** US English, the
Microsoft Korean IME and the Microsoft Japanese IME. It is driven by the HHKB Studio's F17–F20
keys (the name comes from this: switching languages with the **F keys**) and keeps the **JIS
hardware layout and symbols** in every mode.

Naming: **FKeyIDE** in prose and UI, `fkeyide` for identifiers, files and the repo, `f_key_ide`
only where snake case is required.

Items marked **[verify]** are assumptions not checked yet. Facts are marked *(documented)*,
*(measured)* or *(inferred)*. The M0 spike measurements are in section 6.

## 1. Behavior

Keyboard: HHKB Studio JIS, keymap **Profile 4**, the same keymap the
[hhkb-ekj-trilingual](https://github.com/themrsung/hhkb-ekj-trilingual) v1.5.2 AutoHotkey setup
uses, so no keymap change is needed:

```
Fn1 | F18 | Alt | Win | F19 | Space | F20 | F17 | RAlt | Fn1 | ← | ↓ | →
```

| Key | Action (from any mode) |
| --- | --- |
| **F18** | → **Korean 2-Set** (Hangul composition always on; there is no 영 sub-mode, F19 is English) |
| **F19** | → **English QWERTY**, **JIS symbols** (`Shift+2` = `"`, ろ = `\` / `_`, ¥ = `¥`), typed by FKeyIDE from its own key table |
| **F20** | → **Japanese, kana input** (JIS kana layout), kana-kanji conversion |
| **F17** | Depends on the mode, below |

| Mode | F17 |
| --- | --- |
| Korean | **Hanja**: convert the composing syllable/word, or the syllable before the caret |
| Japanese | **Transient kana ↔ romaji toggle**, forgotten when you leave Japanese |
| English | **Transient Caps Lock**, turned off by any mode key |

Rules carried over from the macOS and AutoHotkey setups:

- **One-way keys.** A mode key pressed in its own mode keeps you there. It never toggles out and
  is never dead.
- **Always re-apply.** Never early-return on "already in this mode". Re-assert the profile and
  the reset rules (kana, Caps Lock off).
- **Bare keys only.** Shift/Ctrl/Alt/Win + F17–F20 go to the app untouched.

Implemented and unit-tested in `src/core/mode_machine.*` (`tests/mode_machine_test.cpp`).

**Scope: language input only.** FKeyIDE handles F17–F20 and the text typed in each mode. Other
custom key bindings that don't handle language input, such as v1.5.2's Fn1+P → Play/Pause, are
out of scope (owner, 2026-10-17). A Korean keyboard's 한/영 key isn't handled either (an optional
toggle was tried and removed; only F17–F20 switch modes).

## 2. Architecture

One TSF TIP (in-proc COM DLL, C++20) with **three language profiles** under one CLSID
(`src/tip/guids.h`; the GUIDs are permanent):

| Profile | LANGID | Registered substitute HKL | Layout the thread actually gets *(measured)* |
| --- | --- | --- | --- |
| FKeyIDE EN | en-US 0x0409 | 0x04110411 (kbd106) | 0x04090409, **US symbols** |
| FKeyIDE KO | ko-KR 0x0412 | 0x04110411 | 0x04120412, **US symbols** (`kbd101a`) |
| FKeyIDE JA | ja-JP 0x0411 | 0x04110411 | 0x04110411, JIS symbols |

- F18/F19/F20 switch between FKeyIDE's own profiles (`ITfInputProcessorProfileMgr::ActivateProfile`
  with `TF_IPPMF_FORSESSION`). The switch takes about 5 ms, every other app follows within about
  25 ms, and the owner saw no overlay or flash *(measured, section 6)*.
- **`hklSubstitute` does not give en-US/ko-KR the JIS layout** *(measured, section 6)*. The value
  is stored in the registry, but Windows keeps each language's own layout.
- **FKeyIDE is its own IME and doesn't rely on Microsoft code for text** (owner, 2026-10-17;
  decides open question 8 as option (b)). In **every** mode it maps scan codes to characters
  with its own JIS table (letters, digits, symbols, ろ, ¥, and the kana table in Japanese) and
  inserts the text itself. It doesn't depend on which Windows keyboard layout the thread has, and
  it doesn't use the Microsoft IMEs. Cost: apps that read raw keys instead of text (games,
  terminals in win32-input mode) still see the Windows layout's keys, not FKeyIDE's.
- OS: the Japanese hardware keyboard (106/109) is still recommended so raw-key apps and the
  F-key scan codes behave as in v1.5.2. FKeyIDE's own output doesn't need it **[verify]** in M1.
  Removing the stock Microsoft IMEs from the language list is **optional**, offered by the
  installer wizard (section 7). They always stay installed.
- F-keys: `ITfKeystrokeMgr::PreserveKey` for bare VK_F17–VK_F20 (they arrive in `OnPreservedKey`),
  plus `ITfKeyEventSink`, which eats the key-ups. The app never sees the keys *(measured)*.
- Composition: `ITfContextComposition`, edit sessions, display attributes, `ITfReconversion`.
- UI: own candidate window (per-monitor DPI v2) and UI-less mode (`ITfCandidateListUIElement`).
  Mode indicator: EN / 한 / あ / A (romaji) / ⇪ (Caps).
- **Japanese engine: Mozc** (BSD-3-Clause), pinned in `third_party/mozc.commit`, **Option B**
  (owner, 2026-10-17): FKeyIDE owns the TIP, the UI and the kana table, and only asks
  `mozc_server` for kana-kanji conversion over Mozc's named pipe (section 6.4). No fork of Mozc's TIP.
- **Korean engine:** own 2-Set automaton, fed by scan code, so it doesn't depend on the layout.
  **Hanja: single syllable only** (owner, 2026-10-17), from Unicode Unihan (`kHangul`). No word
  dictionary in v1. Don't link LGPL libhangul.
- **English:** FKeyIDE's own JIS table, like the other modes. Caps Lock = the real system Caps
  Lock (`SendInput(VK_CAPITAL)`), so the LED matches.
- **Global mode** (owner, 2026-10-17): one mode for all windows. `ActivateProfile` with
  `TF_IPPMF_FORSESSION` already switches every TSF thread *(measured, section 6.3)*. A small named
  shared-memory block still carries the romaji flag, so every process agrees on it.
- **Builds:** x64 + x86 (32-bit apps load the 32-bit DLL). Static CRT, so no VC++
  redistributable is needed. **ARM64: not built, not tested, not shipped, but not blocked**
  (owner, 2026-10-17), so advanced users can fork and add it. Keep the code free of x86/x64-only
  assumptions (no inline asm, no hardcoded `x64` paths, pointer-size-agnostic structs).
  `CMakePresets.json` has an `arm64` preset and `tools/dev.ps1 -Arch arm64` works once the ARM64
  MSVC tools are installed (`toolchain.md`). Mozc supports `--platforms=//:windows-arm64`.

## 3. Known limits

- Password fields, the secure desktop (UAC, sign-in) and some legacy contexts don't load TIPs.
  F17–F20 do nothing there, and keys fall back to the raw JIS layout.
- Store/AppContainer apps need the immersive caps and an install path readable by "ALL
  APPLICATION PACKAGES" *(documented)*. Code signing isn't needed to load: the unsigned spike
  DLL loaded and switched in packaged apps (Notepad 11.2607, Start search `SearchHost.exe`,
  Settings) *(measured)*. Plan: sign anyway (M5).
- A TIP only sees keys while one of its own profiles is active. Another program's low-level
  keyboard hook gets F17–F20 first: v1.5.2's AutoHotkey script took them all until it was
  stopped *(measured)*. The MSI must make sure AutoHotkey switching is off.
- Compared with the Microsoft IMEs: no cloud suggestions, no Microsoft user dictionary, different
  (Mozc) conversion quality, no Korean 3-Set or old Hangul, and Hanja for single syllables only, in v1.

## 4. Milestones

| | Milestone | Size | Status |
| --- | --- | --- | --- |
| — | Repo, toolchain, mode logic + tests, DLL skeleton | S | **done** |
| M0 | Spikes: build Mozc; 3 profiles + JIS substitute; F-key delivery per app; decide A/B | S–M | **done** (section 6); decisions in section 5 |
| M1 | Skeleton + English: profiles, F-key routing, shared state, own JIS key table, Caps Lock, indicator | S | not started |
| M2 | Korean: 2-Set automaton + tests, display attributes, single-syllable Hanja | M | not started |
| M3 | Japanese: own kana table, `mozc_server` client (Option B), conversion, romaji toggle | L | not started |
| M4 | UI-less candidates, DPI, dark mode, Store apps, x86 (ARM64 stays unbuilt) | M | not started |
| M5 | WiX MSI, install wizard (section 7), language-list setup, uninstall restores the stock IMEs, signing | S–M | not started |
| M6 | Soak tests (hundreds of switches per key, per app), app matrix, docs | M | not started |

## 5. Decisions and open questions

Decided by the owner (2026-10-17):

| # | Question | Decision |
| --- | --- | --- |
| 2 | Global or per-window mode | **Global** |
| 3 | Hanja scope in v1 | **Single syllable only** |
| 4 | ARM64 | **Not built or shipped, but not blocked** (section 2, "Builds") |
| 6 | Remove the stock Microsoft IMEs from the language list | **Optional**, through the installer wizard; keeping them is the default (section 7) |
| 7 | Fn1+P → Play/Pause and other non-language key bindings | **Out of scope** (section 1, "Scope") |
| 8 | JIS symbols in English and Korean | **(b): FKeyIDE's own key table in every mode**, no Microsoft layout or IME code for text |
| A/B | Japanese engine | **Option B**: own TIP, `mozc_server` for conversion only |
| 9 | 한/영 key toggle | **Removed** for now (reverted in 876010c) |

Still open (the implemented default stands until the owner says otherwise):

1. F20 while already in Japanese resets romaji → kana? (implemented: yes, `Policy::f20_resets_romaji`)
5. Shift/Ctrl + F17–F20: unused (implemented), or extra functions?
10. Installer wizard: does the HHKB Studio's own pointing stick count as a "wired mouse" when the
    HHKB is on USB? Proposed: no. Devices with vendor ID `04FE` (PFU) are listed but don't
    trigger the suggestion.

## 6. M0 results (measured 2026-10-17)

Setup: Windows 11 Pro 25H2 build 26200, i7-1165G7, HHKB Studio JIS on USB, Profile 4, Japanese
106/109 hardware layout. Spike TIP from this repo's `src/tip` (Release, unsigned), installed with
`tools/spike.ps1` (x64 + x86, in `C:\Program Files\FKeyIDE-spike`), stock Microsoft IMEs still
in the list. Raw log: `%ProgramData%\fkeyide\fkeyide.log`. Probe: `tools/spike/probe.cpp`.

### 6.1 F-key delivery per app

Bare F17–F20 arrive through `OnPreservedKey` (the TIP preserves them) and are eaten
(nothing reaches the Edit control in the probe). `PreserveKeys=0` (key event sink only) wasn't
measured.

| App | How | F18/F19/F20/F17 reach the TIP | Notes |
| --- | --- | --- | --- |
| Win32 Edit (probe) | injected (`SendInput`) | yes, 110/110 switches | nothing leaks into the text |
| Opera (Chromium; Chrome isn't installed) | **physical HHKB keys** | yes | owner's pass |
| Edge | injected | yes | |
| Windows Terminal | injected | yes | |
| Notepad 11.2607 (packaged/MSIX) | injected | yes | first try missed: the window wasn't focused yet |
| Start search (`SearchHost.exe`, system packaged app) | injected | yes | |
| VS Code | — | not tested | not installed |
| Calculator (UWP) | injected | no | no text field, so no TSF key sink; not a meaningful test |

Also measured: **another program's low-level keyboard hook wins**. With v1.5.2's AutoHotkey
running, F18–F20 never reached the TIP.

### 6.2 Symbols per profile

`ToUnicodeEx` on the thread's layout after each switch (TIP log) and the same keys typed by scan
code into the probe's Edit control. Keys: Shift+2, ろ, Shift+ろ, ¥, `@` key, `[` key, `:` key.

| Profile | Thread HKL | Result | Expected (JIS keycaps) |
| --- | --- | --- | --- |
| FKeyIDE EN | 04090409 | `@`, –, –, –, `[`, `]`, `'` (US) | `"` `\` `_` `¥` `@` `[` `:` |
| FKeyIDE KO | 04120412 | `@`, –, –, –, `[`, `]`, `'` (US) | same |
| FKeyIDE JA | 04110411 | `"` `\` `_` `\` `@` `[` `:` (JIS) | same |

– = no character. The ¥ key gives U+005C under `kbd106`, which Japanese fonts draw as ¥; that's
the same as v1.5.2's English. The registry has `SubstituteLayout = 0x04110411` for all three
profiles, and it has no effect. Resolved by decision 8 (b) in section 5: FKeyIDE types every character from its own table.

### 6.3 Switch latency and global mode

`ActivateProfile(..., TF_IPPMF_FORSESSION | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE)` from the key
handler (not deferred). QPC timestamps from the shared log, 110 mode-changing presses:

| Interval | Median | p95 | Max |
| --- | --- | --- | --- |
| `ActivateProfile` call | 5.0 ms | 7.0 ms | 21 ms |
| Key → `OnActivated` in the same process | 2.6 ms | 3.8 ms | 18 ms |
| Key → `WM_INPUTLANGCHANGE` in the probe window | 4.5–4.9 ms | 6.3 ms | 6.3 ms |
| Key → last **other** process switched | 26 ms | 41 ms | 1.2 s (once, Terminal) |

- `FORSESSION` switches every TSF thread on the desktop (median 9 other processes per switch):
  the mode is global without shared memory. The TIP is loaded into every one of those processes.
- **No overlay and no flash**, and the taskbar shows the right language (owner, by eye, in Opera).
- Re-pressing the current mode key re-asserts in 0.3 ms.

### 6.4 Mozc: Option A vs B

Mozc builds on this PC from the pinned commit in about 57 minutes total (`toolchain.md`, "Mozc").
It needs one workaround (`--nowindows_enable_symlinks`).

From reading Mozc at the pinned commit:

- Mozc's TIP (`src/win32/tip`, 79 files, about 14k lines) is **ja-JP only**: one profile, LANGID
  0x0411 hardcoded in several places. Its kana input calls the real `ToUnicode` with VK_KANA set,
  so it needs the thread's layout to be Japanese (FKeyIDE JA gets 04110411, so that part works).
- The TIP ↔ server protocol is simple: a message-mode named pipe `\\.\pipe\mozc.<key>.session`
  (key from `LocalLow\Mozc\session.ipc`) carrying `commands::Input`/`Output` protobufs. The
  server doesn't authenticate clients. The pipe's DACL already admits AppContainer apps at Low
  integrity. The client checks the server's image path.
- `mozc_server` embeds its dictionary. It needs only a writable profile folder. The product name,
  pipe prefix and folder are compile-time constants in `base/const.h`. Qt is needed only for
  `mozc_tool` (settings, dictionary tool).
- Mozc builds with Bazel and clang-cl and produces no static `.lib` for its client. Linking it
  into a CMake/MSVC DLL means matching absl/protobuf/CRT by hand.

**Recommendation: Option B.** FKeyIDE's TIP stays the one place for the three modes, F-keys,
UI and Korean. It speaks the pipe protocol with its own small client: `commands.proto` compiled
with protobuf in CMake, an estimated few hundred lines *(inferred)* instead of Mozc's 5.6k-line
client closure. Ship a `mozc_server` built by Bazel from the pinned commit with a small patch to
`base/const.h` (own product name, pipe prefix and profile folder), so it can't collide with a
real Mozc install. Option A would mean un-hardcoding ja-JP across Mozc's TIP, adding Korean and English
into a large foreign codebase, and carrying that fork against upstream.

## 7. Installer wizard (M5)

Requirements from the owner (2026-10-17):

1. **List every keyboard and mouse, and system info.** Keyboards and pointing devices with name,
   connection (USB, Bluetooth, built-in/PS/2) and VID:PID; Windows edition and build, CPU
   architecture, PC model, and chassis type (laptop or desktop). Read-only, on its own page.
   Nothing leaves the PC.
2. **If the PC is a laptop or a wired mouse is connected, suggest removing the Microsoft IMEs**
   from the language list. The page explains why and shows the option, **not selected**.
   Selecting it is one click.
3. **Otherwise, removing them takes two clicks:** one to start ("Remove the Microsoft IMEs…")
   and one to confirm, in a dialog whose default button is "Keep".
4. **Keeping the Microsoft IMEs is always the default.** Clicking Next through the whole wizard
   never removes them.

Removal only takes them out of the language list. They stay installed, and uninstalling FKeyIDE
puts them back.

Detection *(inferred, to check in M5)*: laptop = `Win32_SystemEnclosure.ChassisTypes` in
{8, 9, 10, 14, 30, 31, 32}, or a battery (`Win32_Battery`). Wired mouse = a present pointing
device whose instance path isn't Bluetooth (`BTHENUM` / `BTHLE`), excluding PFU `04FE` devices
(open question 10). The implementation (WiX custom dialog + custom action, or a Burn
bootstrapper with its own UI) is decided in M5.
