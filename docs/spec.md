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
| **F19** | → **English QWERTY**, passthrough, **JIS symbols** (`Shift+2` = `"`, ろ = `\` / `_`, ¥ = `¥`) |
| **F20** | → **Japanese, kana input** (JIS kana layout), kana-kanji conversion |
| **F17** | Depends on the mode, below |

| Mode | F17 |
| --- | --- |
| Korean | **Hanja**: convert the composing syllable/word, or the syllable before the caret |
| Japanese | **Transient kana ↔ romaji toggle**, forgotten when you leave Japanese |
| English | **Transient Caps Lock**, turned off by any mode key |

Optional (off by default, `Policy::han_yeong_toggles`, open question 9): the **한/영 key of a
Korean keyboard** (for example a laptop's built-in one) toggles: **Korean → English, any other
mode → Korean**, with the same reset rules as the mode keys. It's the only key that isn't
one-way, which is why it's opt-in. It acts when it arrives as `VK_HANGUL` or as the LANG1 scan
code (0x72/0xF2). It doesn't act as right Alt (E0 38). The Korean layout maps E0 38 to
`VK_HANGUL` itself, but under the Japanese layout it's the HHKB's real right Alt *(measured
with `MapVirtualKeyEx`)*. **[verify]** on a real Korean keyboard which of these it sends under
each FKeyIDE profile.

Rules carried over from the macOS and AutoHotkey setups:

- **One-way keys.** A mode key pressed in its own mode keeps you there. It never toggles out and
  is never dead.
- **Always re-apply.** Never early-return on "already in this mode". Re-assert the profile and
  the reset rules (kana, Caps Lock off).
- **Bare keys only.** Shift/Ctrl/Alt/Win + F17–F20 go to the app untouched.

Implemented and unit-tested in `src/core/mode_machine.*` (`tests/mode_machine_test.cpp`).

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
  is stored in the registry, but Windows keeps each language's own layout. So JIS symbols in
  English and Korean need one of the approaches in open question 8.
- OS: the Japanese hardware keyboard (106/109) stays on. The stock EN/KO/JA IMEs are removed from
  the language list but stay installed as a fallback.
- F-keys: `ITfKeystrokeMgr::PreserveKey` for bare VK_F17–VK_F20 (they arrive in `OnPreservedKey`),
  plus `ITfKeyEventSink`, which eats the key-ups. The app never sees the keys *(measured)*.
- Composition: `ITfContextComposition`, edit sessions, display attributes, `ITfReconversion`.
- UI: own candidate window (per-monitor DPI v2) and UI-less mode (`ITfCandidateListUIElement`).
  Mode indicator: EN / 한 / あ / A (romaji) / ⇪ (Caps).
- **Japanese engine: Mozc** (BSD-3-Clause), pinned in `third_party/mozc.commit`. Option B
  (recommended after M0, section 6.4): FKeyIDE owns the TIP and UI and talks to `mozc_server`
  over Mozc's named pipe. Option A: fork Mozc's TIP. The owner decides.
- **Korean engine:** own 2-Set automaton, fed by scan code, so it doesn't depend on the layout. Hanja from Unicode Unihan (`kHangul`) plus a word
  dictionary. **[verify]** the license of any Hanja word list; don't link LGPL libhangul.
- **English:** passthrough. Caps Lock = the real system Caps Lock (`SendInput(VK_CAPITAL)`).
- **Shared state:** named shared memory + an event (mode, romaji, caps), so the mode is
  **global** across windows (open question 2).
- **Builds:** x64 + x86 (32-bit apps load the 32-bit DLL); ARM64 if needed (open question 4).
  Static CRT, so no VC++ redistributable is needed.

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
  (Mozc) conversion quality, no Korean 3-Set or old Hangul in v1.

## 4. Milestones

| | Milestone | Size | Status |
| --- | --- | --- | --- |
| — | Repo, toolchain, mode logic + tests, DLL skeleton | S | **done** |
| M0 | Spikes: build Mozc; 3 profiles + JIS substitute; F-key delivery per app; decide A/B | S–M | **done** (section 6); A/B and question 8 wait on the owner |
| M1 | Skeleton + English: profiles, F-key routing, shared state, Caps Lock, indicator | S | not started |
| M2 | Korean: 2-Set automaton + tests, display attributes, Hanja | M | not started |
| M3 | Japanese: Mozc, kana preedit, conversion, romaji toggle | L | not started |
| M4 | UI-less candidates, DPI, dark mode, Store apps, x86/ARM64 | M | not started |
| M5 | WiX MSI, language-list setup, uninstall restores the stock IMEs, signing | S–M | not started |
| M6 | Soak tests (hundreds of switches per key, per app), app matrix, docs | M | not started |

## 5. Open questions for the owner

1. F20 while already in Japanese resets romaji → kana? (implemented as yes, `Policy::f20_resets_romaji`)
2. Global mode across all windows (proposed), or per window?
3. Hanja: syllable-level only for v1, or a word dictionary from the start?
4. Is ARM64 Windows needed?
5. Shift/Ctrl + F17–F20: unused (implemented), or extra functions?
6. OK to remove the stock Microsoft IMEs from the language list once FKeyIDE is stable?
7. In v1.5.2, AutoHotkey turned Fn1+P (Pause) into Play/Pause. A TIP only sees keys while a text
   field has focus, so it can't take this over. Options: assign Play/Pause to Fn1+P in the
   keymap tool **[verify]** that it offers this, or keep a tiny helper.
8. **JIS symbols in English and Korean** (new from M0, section 6.2). Windows ignores the JIS
   substitute layout, so EN and KO get US symbols. Options:
   - **(a) One ja-JP profile for all three modes** (the old fallback). JIS everywhere with no
     extra code, and no profile switching at all. Costs: the taskbar language always says JA
     (FKeyIDE's own mode icon shows EN / 한 / あ), and apps see Korean text typed with a Japanese
     input language (spell-check language, font choice for Hanja in some apps).
   - **(b) Keep EN/KO/JA, and FKeyIDE types the JIS symbols itself** from a scan-code table: KO
     handles every key anyway, so this is cheap there. In EN, the symbol keys stop being plain
     passthrough: apps that read raw keys (games, terminals in win32-input mode) still see the
     US layout.
   - **(c) Hybrid:** EN is a ja-JP profile (native JIS passthrough, exactly like v1.5.2's
     English), KO is ko-KR with (b)'s table, JA is ja-JP. The taskbar shows JA for English, as in
     v1.5.2.
   - **(d) `LayerDriver KOR = kbd106.dll`** (v1.5.2 option 2), system-wide, untested. Doesn't
     cover en-US.

   Proposed default: **(c)**. English keeps the v1.5.2 behavior users already know, and Korean
   gets JIS through code FKeyIDE needs anyway. The profile GUIDs stay; only EN's LANGID changes.

9. **한/영 key on Korean keyboards** (owner's request, 2026-10-17): implemented as an option,
   off by default. Turn it on by default? And should a Korean keyboard that sends 한/영 as right
   Alt be supported (it would need telling that keyboard apart from the HHKB)?

## 6. M0 results (measured 2026-10-17)

Setup: Windows 11 Pro 25H2 build 26200, i7-1165G7, HHKB Studio JIS on USB, Profile 4, Japanese
106/109 hardware layout. Spike TIP from this repo's `src/tip` (Release, unsigned), installed with
`tools/spike.ps1` (x64 + x86, in `C:\Program Files\FKeyIDE-spike`), stock Microsoft IMEs still
in the list. Raw log: `%ProgramData%keyidekeyide.log`. Probe: `tools/spike/probe.cpp`.

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
profiles, and it has no effect. See open question 8.

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
