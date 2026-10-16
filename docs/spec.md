# FKeyIDE: behavior spec and architecture

FKeyIDE is one Windows IME (a TSF text input processor) that **replaces** US English, the
Microsoft Korean IME and the Microsoft Japanese IME. It is driven by the HHKB Studio's F17–F20
keys (the name comes from this: switching languages with the **F keys**) and keeps the **JIS
hardware layout and symbols** in every mode.

Naming: **FKeyIDE** in prose and UI, `fkeyide` for identifiers, files and the repo, `f_key_ide`
only where snake case is required.

Items marked **[verify]** are assumptions to check in the M0 spike before relying on them.

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

| Profile | LANGID | Substitute HKL |
| --- | --- | --- |
| FKeyIDE EN | en-US 0x0409 | Japanese layout 0x04110411 (kbd106) |
| FKeyIDE KO | ko-KR 0x0412 | Japanese layout 0x04110411 |
| FKeyIDE JA | ja-JP 0x0411 | 0x04110411 |

- F18/F19/F20 switch between FKeyIDE's own profiles (`ITfInputProcessorProfileMgr::ActivateProfile`),
  so the TIP stays loaded. **[verify]** that `hklSubstitute` gives en-US/ko-KR the JIS symbols,
  the switch latency, and that the language overlay doesn't flash. Fallback: one ja-JP profile
  that does all three modes.
- OS: the Japanese hardware keyboard (106/109) stays on. The stock EN/KO/JA IMEs are removed from
  the language list but stay installed as a fallback.
- F-keys: `ITfKeystrokeMgr::PreserveKey` / `ITfKeyEventSink` for VK_F17–VK_F20, eaten.
- Composition: `ITfContextComposition`, edit sessions, display attributes, `ITfReconversion`.
- UI: own candidate window (per-monitor DPI v2) and UI-less mode (`ITfCandidateListUIElement`).
  Mode indicator: EN / 한 / あ / A (romaji) / ⇪ (Caps).
- **Japanese engine: Mozc** (BSD-3-Clause), pinned in `third_party/mozc.commit`. Option B
  (recommended): FKeyIDE owns the TIP and UI and talks to `mozc_server` for conversion. Option A:
  fork Mozc's TIP. Decided in M0.
- **Korean engine:** own 2-Set automaton. Hanja from Unicode Unihan (`kHangul`) plus a word
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
  APPLICATION PACKAGES". **[verify]** whether code signing is required (plan: sign anyway).
- Compared with the Microsoft IMEs: no cloud suggestions, no Microsoft user dictionary, different
  (Mozc) conversion quality, no Korean 3-Set or old Hangul in v1.

## 4. Milestones

| | Milestone | Size | Status |
| --- | --- | --- | --- |
| — | Repo, toolchain, mode logic + tests, DLL skeleton | S | **done** |
| M0 | Spikes: build Mozc; 3 profiles + JIS substitute; F-key delivery per app; decide A/B | S–M | not started |
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
