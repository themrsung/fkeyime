# FKeyIME: behavior spec and architecture

FKeyIME is one Windows IME (a TSF text input processor) that **replaces** US English, the
Microsoft Korean IME and the Microsoft Japanese IME. It is driven by the HHKB Studio's F17–F20
keys (the name comes from this: switching languages with the **F keys**) and keeps the **JIS
hardware layout and symbols** in every mode.

Naming: **FKeyIME** in prose and UI, `fkeyime` for identifiers, files and the repo, `f_key_ime`
only where snake case is required.
Renamed from FKeyIDE (`fkeyide`) on 2026-10-17, before any release. The old GitHub URL
redirects. The CLSID and profile GUIDs didn't change.

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
| **F19** | → **English QWERTY**, **JIS symbols** (`Shift+2` = `"`, ろ = `\` / `_`, ¥ = `¥`), typed by FKeyIME from its own key table |
| **F20** | → **Japanese, kana input** (JIS kana layout), kana-kanji conversion |
| **F16** | → **Japanese, romaji input**, kana-kanji conversion (owner, 2026-10-17) |
| **F17** | Depends on the mode, below |

| Mode | F17 |
| --- | --- |
| Korean | **Hanja**: convert the composing syllable/word, or the syllable before the caret |
| Japanese | **Transient switch to the other input**: kana → romaji after F20, romaji → kana after F16. Forgotten when you leave Japanese |
| English | **Transient Caps Lock**, turned off by any mode key |

F16 isn't on Profile 4 yet: a key must be set to F16 in the Studio keymap tool (open question
11). Until then FKeyIME handles F16 from any keyboard or remapper that sends it.

Rules carried over from the macOS and AutoHotkey setups:

- **One-way keys.** A mode key pressed in its own mode keeps you there. It never toggles out and
  is never dead.
- **Always re-apply.** Never early-return on "already in this mode". Re-assert the profile and
  the reset rules (F20 kana, F16 romaji, Caps Lock off).
- **Bare keys only.** Shift/Ctrl/Alt/Win + F17–F20 go to the app untouched.

Implemented and unit-tested in `src/core/mode_machine.*` (`tests/mode_machine_test.cpp`).

**Scope: language input only.** FKeyIME handles F17–F20 and the text typed in each mode. Other
custom key bindings that don't handle language input, such as v1.5.2's Fn1+P → Play/Pause, are
out of scope (owner, 2026-10-17). A Korean keyboard's 한/영 key isn't handled either (an optional
toggle was tried and removed; only F17–F20 switch modes).

## 2. Architecture

One TSF TIP (in-proc COM DLL, C++20) with **three language profiles** under one CLSID
(`src/tip/guids.h`; the GUIDs are permanent):

| Profile | LANGID | Registered substitute HKL | Layout the thread actually gets *(measured)* |
| --- | --- | --- | --- |
| FKeyIME EN | en-US 0x0409 | 0x04110411 (kbd106) | 0x04090409, **US symbols** |
| FKeyIME KO | ko-KR 0x0412 | 0x04110411 | 0x04120412, **US symbols** (`kbd101a`) |
| FKeyIME JA | ja-JP 0x0411 | 0x04110411 | 0x04110411, JIS symbols |

- F18/F19/F20 switch between FKeyIME's own profiles (`ITfInputProcessorProfileMgr::ActivateProfile`
  with `TF_IPPMF_FORSESSION`). The switch takes about 5 ms, every other app follows within about
  25 ms, and the owner saw no overlay or flash *(measured, section 6)*.
- **`hklSubstitute` does not give en-US/ko-KR the JIS layout** *(measured, section 6)*. The value
  is stored in the registry, but Windows keeps each language's own layout.
- **FKeyIME is its own IME and doesn't rely on Microsoft code for text** (owner, 2026-10-17;
  decides open question 8 as option (b)). In **every** mode it maps scan codes to characters
  with its own JIS table (letters, digits, symbols, ろ, ¥, and the kana table in Japanese) and
  inserts the text itself. It doesn't depend on which Windows keyboard layout the thread has, and
  it doesn't use the Microsoft IMEs. Cost: apps that read raw keys instead of text (games,
  terminals in win32-input mode) still see the Windows layout's keys, not FKeyIME's.
- OS: the Japanese hardware keyboard (106/109) is still recommended so raw-key apps and the
  F-key scan codes behave as in v1.5.2. FKeyIME's own output doesn't need it **[verify]** in M1.
  Removing the stock Microsoft IMEs from the language list is **optional**, offered by the
  installer wizard (section 7). They always stay installed.
- F-keys: `ITfKeystrokeMgr::PreserveKey` for bare VK_F17–VK_F20 (they arrive in `OnPreservedKey`),
  plus `ITfKeyEventSink`, which eats the key-ups. The app never sees the keys *(measured)*.
- Composition: `ITfContextComposition`, edit sessions, display attributes, `ITfReconversion`.
- UI: own candidate window (per-monitor DPI v2) and UI-less mode (`ITfCandidateListUIElement`).
  Mode indicator: EN / 한 / あ / A (romaji) / ⇪ (Caps).
- **Japanese engine: Mozc** (BSD-3-Clause), pinned in `third_party/mozc.commit`, **Option B**
  (owner, 2026-10-17): FKeyIME owns the TIP, the UI and the kana table, and only asks
  `mozc_server` for kana-kanji conversion over Mozc's named pipe (section 6.4). No fork of Mozc's TIP.
  Built: section 9.
- **Korean engine:** own 2-Set automaton, fed by scan code, so it doesn't depend on the layout.
  **Hanja: single syllable only** (owner, 2026-10-17), from Unicode Unihan (`kHangul`). No word
  dictionary in v1. Don't link LGPL libhangul.
- **English:** FKeyIME's own JIS table, like the other modes. Caps Lock = the real system Caps
  Lock (`SendInput(VK_CAPITAL)`), so the LED matches.
- **Public API** (`docs/api.md`): `fkeyime.dll` exports `FKeyImeQueryWindow` and friends, which
  read a session-wide table of the threads where FKeyIME is active. The v1.5.2 AutoHotkey script
  uses it to step aside while FKeyIME is active.
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
- **After signing in, Windows starts with the first input method in the language list**, not
  FKeyIME *(measured 2026-10-17, after a reboot)*: the list was ko (Microsoft Korean IME first), ja,
  en-US, so the session started on the Microsoft Korean IME, the FKeyIME-aware AutoHotkey script
  correctly handled the F-keys the Microsoft way, and FKeyIME only took over after the owner
  picked it with Win+Space. Fix: Windows' default input method override set to FKeyIME EN
  (`Set-WinDefaultInputMethodOverride -InputTip "0409:{66749D2A-1770-43D3-A269-A051B5BC1AAA}{8F61A2C0-75BC-4042-B739-CC7D63B542E5}"`,
  or Settings → Time & language → Typing → Advanced keyboard settings). The installer (M5) must
  offer it, and uninstalling must clear it.
- **Apps already running when FKeyIME is installed, updated or reinstalled may not load it until
  they restart** *(measured 2026-10-17)*: Notepad, Claude, Opera, Edge and Windows Terminal,
  running since before the day's reinstalls (and the rename from FKeyIDE), switched to FKeyIME's
  language but never asked the DLL for its object, so Windows' plain layouts typed instead.
  Freshly started apps (Win32, WPF) and the same Notepad after a restart loaded it at once. The
  installer (M5) must ask to restart apps or sign out.
- Compared with the Microsoft IMEs: no cloud suggestions, no Microsoft user dictionary, different
  (Mozc) conversion quality, no Korean 3-Set or old Hangul, and Hanja for single syllables only, in v1.

## 4. Milestones

| | Milestone | Size | Status |
| --- | --- | --- | --- |
| — | Repo, toolchain, mode logic + tests, DLL skeleton | S | **done** |
| M0 | Spikes: build Mozc; 3 profiles + JIS substitute; F-key delivery per app; decide A/B | S–M | **done** (section 6); decisions in section 5 |
| M1 | Skeleton + English: profiles, F-key routing, shared state, own JIS key table, Caps Lock, indicator | S | **done except the mode indicator** (section 8) |
| M2 | Korean: 2-Set automaton + tests, display attributes, single-syllable Hanja | M | **done except Hanja** (section 8) |
| M3 | Japanese: own kana table, `mozc_server` client (Option B), conversion, romaji toggle | L | **kana input and kanji conversion done** (sections 8, 9); romaji input not started |
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
| 8 | JIS symbols in English and Korean | **(b): FKeyIME's own key table in every mode**, no Microsoft layout or IME code for text |
| A/B | Japanese engine | **Option B**: own TIP, `mozc_server` for conversion only |
| 9 | 한/영 key toggle | **Removed** for now (reverted in 876010c) |
| 12 | Keyboards that can't send F16-F20 (US ANSI boards without remapping, or no remap software) | **Escape hatch, off by default, no remap needed**: ``Alt+` `` (and optionally Menu) cycles English → Korean → Japanese → English (Japanese in kana or romaji, a setting), ``Alt+Shift+` `` acts as F17. The installer asks (section 7). The cycle is the one exception to "a mode key never toggles" (section 8) |

Still open (the implemented default stands until the owner says otherwise):

1. A Japanese key while already in Japanese re-selects its input (F20 → kana, F16 → romaji)?
   (implemented: yes, `Policy::japanese_key_resets_input`)
5. Shift/Ctrl + F17–F20: unused (implemented), or extra functions?
10. Installer wizard: does the HHKB Studio's own pointing stick count as a "wired mouse" when the
    HHKB is on USB? Proposed: no. Devices with vendor ID `04FE` (PFU) are listed but don't
    trigger the suggestion.
11. Which physical key sends F16? Profile 4's bottom row is full (F18, F19, F20, F17). Options:
    an Fn layer key (for example Fn1 + the F20 key), or a key that's free on the Standard layer.

## 6. M0 results (measured 2026-10-17)

Setup: Windows 11 Pro 25H2 build 26200, i7-1165G7, HHKB Studio JIS on USB, Profile 4, Japanese
106/109 hardware layout. Spike TIP from this repo's `src/tip` (Release, unsigned), installed with
`tools/spike.ps1` (x64 + x86, in `C:\Program Files\FKeyIME-spike`), stock Microsoft IMEs still
in the list. Raw log: `%ProgramData%\fkeyime\fkeyime.log`. Probe: `tools/spike/probe.cpp`.

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
| FKeyIME EN | 04090409 | `@`, –, –, –, `[`, `]`, `'` (US) | `"` `\` `_` `¥` `@` `[` `:` |
| FKeyIME KO | 04120412 | `@`, –, –, –, `[`, `]`, `'` (US) | same |
| FKeyIME JA | 04110411 | `"` `\` `_` `\` `@` `[` `:` (JIS) | same |

– = no character. The ¥ key gives U+005C under `kbd106`, which Japanese fonts draw as ¥; that's
the same as v1.5.2's English. The registry has `SubstituteLayout = 0x04110411` for all three
profiles, and it has no effect. Resolved by decision 8 (b) in section 5: FKeyIME types every character from its own table.

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
  so it needs the thread's layout to be Japanese (FKeyIME JA gets 04110411, so that part works).
- The TIP ↔ server protocol is simple: a message-mode named pipe `\\.\pipe\mozc.<key>.session`
  (key from `LocalLow\Mozc\session.ipc`) carrying `commands::Input`/`Output` protobufs. The
  server doesn't authenticate clients. The pipe's DACL already admits AppContainer apps at Low
  integrity. The client checks the server's image path.
- `mozc_server` embeds its dictionary. It needs only a writable profile folder. The product name,
  pipe prefix and folder are compile-time constants in `base/const.h`. Qt is needed only for
  `mozc_tool` (settings, dictionary tool).
- Mozc builds with Bazel and clang-cl and produces no static `.lib` for its client. Linking it
  into a CMake/MSVC DLL means matching absl/protobuf/CRT by hand.

**Recommendation: Option B.** FKeyIME's TIP stays the one place for the three modes, F-keys,
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

5. **Keyboard page** (owner, 2026-10-17): JIS or US ANSI (`KeyboardLayout`, section 8). For US
   ANSI, the ー / ろ route (`AnsiExtraKeys`), then: "Can your keyboard send F16-F20 (remapping in
   its firmware or a remap tool)?" If not, offer the escape hatch: ``Alt+` `` and/or Menu cycle the modes
   (`CycleKeys`), Japanese by kana or romaji (`CycleJapanese`), ``Alt+Shift+` `` as F17 (`F17Keys`).
   **Every escape-hatch option is off by default**; Next without choosing leaves them off. The
   wizard writes them to `HKLM\Software\fkeyime` (section 8: packaged apps can't read HKCU).

Removal only takes them out of the language list. They stay installed, and uninstalling FKeyIME
puts them back.

Detection *(inferred, to check in M5)*: laptop = `Win32_SystemEnclosure.ChassisTypes` in
{8, 9, 10, 14, 30, 31, 32}, or a battery (`Win32_Battery`). Wired mouse = a present pointing
device whose instance path isn't Bluetooth (`BTHENUM` / `BTHLE`), excluding PFU `04FE` devices
(open question 10). The implementation (WiX custom dialog + custom action, or a Burn
bootstrapper with its own UI) is decided in M5.

## 8. Typing (M1-M3, built 2026-10-17)

What works now. Keys come from FKeyIME's own tables by scan code (`src/core/jis_layout.*`); the
composers are pure C++ with unit tests (`src/core/hangul.*`, `src/core/kana.*`,
`tests/input_test.cpp`).

| Mode | Keys | Behavior |
| --- | --- | --- |
| English (F19) | letters, digits, symbols | Inserted directly, JIS keycaps macOS style: Shift+2 `"`, ろ `\` / `_`, ¥ `¥` / `\|`, `@` `[` `:` … Caps Lock (F17) uppercases letters, Shift reverses it |
| Korean (F18) | letter keys | 2-Set syllables, composed in place (underlined, block cursor). Backspace removes one jamo (ㅘ → ㅗ, ㄳ → ㄱ). A final moves to the next syllable before a vowel (각+ㅏ → 가가) |
| Korean | digits, symbols | JIS symbols (US symbols with `KeyboardLayout=1`, below); they end the syllable first |
| Japanese (F20) | every key | JIS kana layout (かな入力), hiragana, composed in place. ゛ / ゜ combine with the kana before. Shift gives small kana, を, 「」、。・ |
| Japanese, composing | F6 / F7 / F8 | Whole composition to hiragana / full-width katakana / half-width katakana, like macOS. The next kana typed accepts it; Escape undoes it |
| Japanese, composing | Space | Kanji conversion (section 9). Without the conversion server, accepts the kana as they are |
| Japanese, composing | Enter | Accept the kana as they are |
| Japanese, composing | Backspace, Escape | Remove the last kana / cancel the composition |
| Any | Ctrl, Alt, Win + key | Passed to the app (shortcuts); a composition ends first |
| Any | F16-F20 | A composition ends first and keeps its text |
| Japanese romaji (F16) | — | **Not implemented yet**: types the English JIS table |

Two workarounds for apps that reach TSF through the IMM compatibility layer (classic Win32 Edit
controls and similar), both measured with `fkeyime_probe.exe --typing`:

- **Korean interim character.** The composing syllable is selected as an interim character
  (block cursor), as the Microsoft Korean IME does. With a plain caret, the compatibility layer
  ended every Korean composition at once (each jamo came out on its own: ㅇㅏㄴ…).
- **Profile switch after a commit.** When an F-key commits a composition, the profile switch is
  delayed until the app has processed the commit (a 30 ms timer, or the next key, whichever comes
  first). Switching at once made the compatibility layer deliver a Korean syllable twice
  (가 F19 → 가가). Keys typed in between already follow the new mode.

### US ANSI keyboards (option, built 2026-10-17, not measured on a device yet)

A US ANSI board reports the JIS scan code at every key position the two share *(documented: PS/2
set 1)*, so only the printed symbols differ. Its `` ` `` key is JIS 半角/全角 (0x29), its \ key above
Enter is JIS ] / む (0x2B), and it has no ろ (0x73) or ¥ (0x7D) key. Measured on the TFG Magnetox
(104-key ANSI) before this option: English, Korean and kana typed, with the symbols at their JIS
positions (the \ key gave `]`).

Options are DWORDs in `HKCU\Software\fkeyime`, else `HKLM\Software\fkeyime`. **Set them in
HKLM** (`tools/spike.ps1 -Option`, later the installer): packaged (Store) apps don't see the
user's `HKCU\Software\fkeyime` and fall back to HKLM *(measured: Notepad read `KeyboardLayout=0`
with only HKCU set, conhost read 1)*. HKLM is readable by all app packages (its ACL grants ALL
APPLICATION PACKAGES read). The keyboard options (this section) are read again whenever an app's
window gets focus, so a change applies without restarting apps *(measured: a running Notepad
picked up HKLM values on focus)*.

| Option | Values |
| --- | --- |
| `KeyboardLayout` | `0` JIS (default): the table above. `1` US ANSI: English and the symbols in Korean follow the US keycaps (Shift+2 `@`, `'` `"`, `\` `\|`, `` ` `` `~` …). Letters, 2-Set and the kana positions don't change |
| `AnsiExtraKeys` | US ANSI only: how to type the two kana ANSI has no key for, ー (JIS ¥ key) and ろ. Bits, default `3` (both). `1` the spare `` ` `` key: `` ` `` = ー, Shift+`` ` `` = ろ (owner's choice: ー is far more common). `2` F14 = ー, F15 = ろ, for a keyboard whose firmware sends them (F16-F20 keep their meanings) |

Kana on US ANSI: every other kana sits where JIS has it, including ゛ on `[`, ゜ on `]` and む on
the \ key. F14 / F15 are only read in kana input; elsewhere they pass to the app as before.

```powershell
tools/spike.ps1 -Option KeyboardLayout=1   # HKLM, elevated
```

### Escape hatch: no F16-F20 (option, built and probe-tested 2026-10-17)

For keyboards that can't send F16-F20 (a US ANSI board without remapping, or a user who doesn't
want remap software). No remap of any kind is needed. Off by default (owner's decision 12); the
installer asks (section 7). Independent of `KeyboardLayout`.

| Option | Values |
| --- | --- |
| `CycleKeys` | Bits, `0` off (default). `1` **``Alt+` ``** cycles English → Korean → Japanese → English. `2` **Menu** (the context-menu key) does the same. It acts like the F-key for the next mode (F18, F20 or F16, F19), so a composition ends first and Caps Lock turns off |
| `CycleJapanese` | Which input the cycle enters Japanese with: `0` kana (F20, default), `1` romaji (F16; romaji input isn't built yet, so it types English for now) |
| `F17Keys` | Bits, `0` off (default). `1` **``Alt+Shift+` ``** acts as F17: Hanja, the transient other Japanese input, or transient Caps Lock |

Why these keys (owner, 2026-10-17): every ANSI board has the `` ` `` key, compact ones included, and Windows
itself doesn't use ``Alt+` ``; Microsoft's Japanese IME uses it as on/off on US keyboards, so it's a
known input-method combo. Ctrl+Tab, Ctrl+Space, Shift+Space, Ctrl+Shift+digit and Alt+digit were
rejected for app conflicts. ``Ctrl+Shift+` `` is VS Code's new terminal.

How it works *(measured with `fkeyime_probe --typing COMBO` / `--typing-rich COMBO`)*:

- ``Alt+` ``, ``Alt+Shift+` `` and Menu are TSF preserved keys, like F16-F20, so the app never sees them.
  Under the US and Korean layouts ` is `VK_OEM_3`.
- Under the Japanese layout (Japanese mode), ``Alt+` `` is `VK_KANJI` and no preserved key matches it;
  it reaches the key event sink, where FKeyIME catches it by scan code (`0x29`) with Alt down. It
  works with Windows' own Japanese on/off hotkey (`IME_JHOTKEY_CLOSE_OPEN`, Alt+半角/全角) left as
  it is: no Windows setting changes.
- Menu: FKeyIME also eats its key-up. In TSF-aware controls (RichEdit, which Notepad uses) there's
  no context menu. **Classic Win32 Edit controls still open their context menu** (old dialogs),
  even though FKeyIME eats both key-down and key-up there *(measured; cause not found)*.
- An app may end the composition itself when Alt goes down (RichEdit does). FKeyIME then moves the
  caret past the composition, or the next text would land in front of it (measured: あ가 instead
  of 가あ, fixed).

| Case (probe) | Win32 Edit | RichEdit (TSF) |
| --- | --- | --- |
| ``Alt+` `` cycles EN → KO → JA → EN | Pass | Pass |
| ``Alt+Shift+` `` acts as F17 (Caps Lock in English) | Pass | Pass |
| ``Alt+` `` ends a Korean composition, then kana | Pass | Pass |
| Menu cycles EN → KO → JA → EN | Cycles, but the context menu opens | Pass |
| ANSI ` types ー in kana | Pass | Pass |

Not measured yet: the physical keyboard, browsers, Office, terminals.

**Rejected after measuring:** lone taps of Right Win, Right Ctrl and Right Alt. None reached the
TIP, through the key event sink or as a TSF preserved key, including `TF_MOD_ON_KEYUP` (the way
IMEs bind a tap of Shift). Right Alt's key-up put the window in menu mode, and an injected Right
Win opened the Start menu, which then held all mouse and keyboard input (`docs/issues/`). A
Windows key remap (`Scancode Map`) would work but was rejected by the owner: a required remap
defeats the purpose.

Like F17-F20, these keys only work where FKeyIME is the active input method and a window has
focus *(inferred from how TIPs load)*.

The cycle is the one key that toggles: it breaks "a mode key never toggles out" (section 1) on
purpose, because one key can't name three modes. The F-keys keep the rule.

### Measured (owner's PC, 2026-10-17)

`fkeyime_probe.exe --typing` / `--typing-rich`: 24 key sequences (English symbols and Caps Lock;
Korean words, Backspace, tense consonants, compound vowels, symbols, commits on Enter and F-keys;
Japanese kana, dakuten, F6-F8, Backspace, Escape, Space; commits across modes; 6 of them with
every key queued at once), typed by scan code and read back from the control.

| Process | Win32 Edit (IMM compatibility) | RichEdit 5 with TSF |
| --- | --- | --- |
| x64 | 24/24, twice | 24/24, twice |
| x86 | 24/24 | 24/24 |

`--handoff 20` with the FKeyIME-aware AutoHotkey script still gives 80/80 to FKeyIME and 40/40 to
AutoHotkey. Not tested yet: real hardware keys (the probe injects scan codes), browsers, Office,
terminals, Store apps, games.

Not done yet: Hanja (F17 in Korean does nothing), romaji input, the mode indicator. Kanji
conversion: section 9.

## 9. Kanji conversion (M3, built 2026-10-17)

Option B as decided: FKeyIME keeps its own kana composer and asks a bundled, renamed Mozc server
for the conversion only.

**Pieces**

- `src/core/mozc_protocol.*`: the part of Mozc's session protocol FKeyIME uses, written and read
  as protobuf wire format by hand (field numbers from `commands.proto` and
  `candidate_window.proto` at the pinned commit). No protobuf library, so the x64 and x86 TIPs
  stay free of it. Unit-tested (`tests/mozc_protocol_test.cpp`).
- `src/tip/mozc_client.*`: one request and one response per pipe connection, as Mozc's own
  client does. Every call times out after 2 s, so a stuck server can't freeze the app. The pipe
  is `\\.\pipe\fkeyime-mozc.<key>.session`; the key is the SHA-1 of the user's SID, as Mozc
  derives it, so it works without reading Mozc's key file (which AppContainer apps can't).
  Starts the server when it isn't running.
- The server: Mozc at the pinned commit with `third_party/patches/` applied, built by
  `tools/build-mozc-server.ps1` (`//server:mozc_server_win`, about a minute with a warm Bazel
  cache) and installed as `<install>\mozc\fkeyime_mozc_server.exe`. The x86 TIP talks to the
  same x64 server process.
  - `mozc-fkeyime-names.patch`: own product name, pipe prefix, mutex/event names, registry key
    and profile folder (`%LOCALAPPDATA%Low\FKeyIME Mozc`), so it never collides with a real
    Mozc install.
  - `mozc-fkeyime-unsandboxed-server.patch`: see "Security" below.
  - `mozc-fkeyime-server-path.patch`: see "Watch dog" below.
- `src/tip/candidate_window.*`: the candidate list, a topmost popup that never takes focus,
  under the segment being converted (`ITfContextView::GetTextExt`, the caret as a fallback),
  scaled for the window's DPI, with the page position (`2/36`).
- Display attributes: the segment being converted gets a thick underline, the other converted
  segments a dotted one.

**Keys** (Mozc's MS-IME keymap while converting)

| Key | While converting |
| --- | --- |
| Space / Down, Up | Next / previous candidate (the candidate window opens on the second Space) |
| Left, Right | Move between segments |
| Shift+Left, Shift+Right | Make the segment shorter / longer |
| Enter | Accept the conversion |
| Escape, Backspace | Back to the kana (FKeyIME's kana composer takes over again) |
| F6-F10 | Hiragana / katakana / half-width / full-width and half-width roman |
| PageUp, PageDown, Tab | Candidate pages; predict and convert |
| A kana key | Accepts the conversion and starts new kana |
| F16-F20, Ctrl/Alt/Win shortcuts, other keys | Accept the conversion first |

Modifier keys on their own never end a composition. Before this was fixed, every Shift (ょ,
ㅆ…) committed what was composed so far (measured: とうき|ょうにいく).

**Security.** Mozc's own client starts the server inside Mozc's sandbox (a restricted token),
and the server refuses to run without it. FKeyIME starts it directly, so the second patch lets
the server run without the sandbox thread token. It then runs with the user's normal rights.
Before a release, start it through Mozc's sandbox instead (`base/win32/win_sandbox.*`), or with
an equivalent restricted token (open item for M5).

**Watch dog.** Every 180 s Mozc's session watch dog connects to its own server with Mozc's
client, which only accepts a server at Mozc's install path (`SystemUtil::GetServerPath`). Ours
runs from FKeyIME's folder, so Cleanup and three pings failed and the watch dog aborted the
server (`LOG(FATAL)`, exit `0xC0000409`), exactly 183 s after it started *(measured: event log
crashes at 18:01 and 18:57 on 2026-10-17; reproduced in 15 s with `--watch_dog_interval=10`)*.
The TIP restarted it on the next conversion, so it showed only as a delay. The patch makes the
server's own folder its server directory. `tools/test-mozc-server.ps1` runs the server with a
10 s watch dog for 30 s: it fails on the unpatched server and passes on the patched one.

**Lifetime.** The TIP that needs it first starts the server, detached from that app (and from
its job, where allowed). AppContainer apps can't start processes: they convert only while the
server is already running (started by any other app). Before release, start it at sign-in
or through a broker (M5).

**Learning.** Mozc learns from the candidates the user picks, and keeps that in its profile
folder. Tests should only accept first candidates or cancel, or they'll change the order for
the next run (measured: one manual test that picked いく made it the first candidate).

### Measured (owner's PC, 2026-10-17)

| Check | Result |
| --- | --- |
| `fkeyime_probe.exe --typing` / `--typing-rich`, 34 cases (the 24 typing cases plus 8 conversion cases and 2 Shift-inside cases) | 34/34 in the Win32 Edit and the TSF RichEdit, x64 (twice) and x86 |
| Conversion cases | にほん → 日本 (first candidate, no window); わたしはがくせいです → 私は学生です across two segments with Right; the candidate window opens on the second Space and closes on Escape; Escape and Backspace go back to kana; F7 → テスト; a kana or an F-key accepts the conversion |
| `--handoff 20` with the AutoHotkey script | 80/80 to FKeyIME, 40/40 to AutoHotkey |
| Server start (stopped → first answer) | about 100 ms |
| A conversion round trip (convert + cancel, including starting the test tool) | about 20 ms |
| Server memory | about 23 MB |

Not tested yet: real hardware keys, browsers, Office, terminals, Store apps, games (UI-less
candidates, M4), the candidate window on a second monitor or at other DPIs, dark mode.

