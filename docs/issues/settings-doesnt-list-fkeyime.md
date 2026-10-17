# Windows Settings doesn't list FKeyIME under the language's keyboards

**Status:** open (2026-10-17).

## Symptom

Settings → Time & language → Language & region → 한국어 ⋯ → Language options → 설치된 키보드
lists only "Microsoft 입력기". FKeyIME KO isn't there, so its "Keyboard options" (which would open
FKeyIME's settings window through `ITfFnConfigure`) can't be reached. The owner couldn't find
FKeyIME in Settings at all; the language list there shows 한국어, 日本語 and 영어(미국), and the
last one exists only for FKeyIME EN.

*(Measured: the page read with UI Automation, read-only. FKeyIME KO is in the user's language
list (`HKCU\Control Panel\International\User Profile\ko`) and is offered by the taskbar's input
menu, where the owner picks it.)*

## Differences from the Microsoft IME's registration

`HKLM\SOFTWARE\Microsoft\CTF\TIP\<CLSID>\LanguageProfile\0x00000412\<profile>`:

| Value | Microsoft IME | FKeyIME KO |
| --- | --- | --- |
| `Description` | Microsoft IME | FKeyIME KO |
| `Display Description` | `@%SystemRoot%\system32\input.dll,-5183` | **missing** |
| `IconFile`, `IconIndex` | `imkrtip.dll`, 0 | `fkeyime.dll` (x86), 0: **the DLL has no icon resource** |
| `Enable` | 0 | 1 |

## What was tried (2026-10-18, each followed by reopening the Settings page and reading it)

| Change | Registry after | Settings lists FKeyIME KO? |
| --- | --- | --- |
| Display name: `SetLanguageProfileDisplayName` with a string resource, and an icon in the DLL (`fkeyime.rc`) | `Display Description = @…\fkeyime.dll,-202`, `IconFile` with a real icon | No |
| No substitute layout (every profile used to name the Japanese 0x04110411) | `SubstituteLayout` gone | No |
| Per-user registration with Windows' `InstallLayoutOrTip` (the MSI's `--user-install` step) for the three profiles | returns success; no per-user `HKCU\Software\Microsoft\CTF\TIP` entry appears for FKeyIME (the Microsoft IMEs have one with `Enable = 1`, being disabled machine-wide) | No |

All three changes are kept: they match how Mozc's TIP registers (`third_party/mozc/src/win32/base/tsf_registrar.cc`:
same categories, a display name, an icon index), they're harmless, and the substitute layout had
no effect on typing (M0). Typing hasn't been re-measured since, though.

## Cause

Not known. FKeyIME's registration now matches Mozc's step for step. Open question: does this
Windows 11 build (25H2, 26200) list **any** third-party IME's keyboards in Settings? Test it by
installing another one, for example `Mozc64.msi` from M0 (`docs/toolchain.md`), in a VM.

## Workaround

Open the settings window from the Start menu ("FKeyIME settings", added by the MSI) or run
`fkeyime-settings.exe` from the install folder. `ITfFnConfigure::Show` itself works (measured
through COM).
