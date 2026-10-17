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

| TIP-level `Enable = "1"` (`ITfInputProcessorProfiles::Register`, as Mozc calls it) | present | No |
| Test only: no per-profile `Enable` values (Mozc has none) | removed, then restored by re-registering | No |

**Settings does list a third-party IME** *(measured 2026-10-18)*: with Mozc 3.35.6354 (built in
M0, `Mozc64.msi`) installed on the owner's PC, 일본어 → 설치된 키보드 shows "Mozc 입력기" with its
own 키보드 옵션, next to the Microsoft IME. FKeyIME JA, in the same language list, isn't shown.
So the cause is specific to FKeyIME.

Remaining difference from Mozc: FKeyIME is **one CLSID with three languages** (en-US, ko-KR,
ja-JP); Mozc and the Microsoft IMEs have one language per CLSID. Testing it means hiding the EN
and KO profiles for a moment (deleting their HKLM keys, restored by re-registering); that needs
the owner's OK.

All of the kept changes: they match how Mozc's TIP registers (`third_party/mozc/src/win32/base/tsf_registrar.cc`:
same categories, a display name, an icon index), they're harmless, and the substitute layout had
no effect on typing (M0). Typing hasn't been re-measured since, though.

## Cause

Not known. Settings lists Mozc, so third-party IMEs are listed. Hypothesis: Settings skips a
TIP that registers profiles in several languages. If that's it, the fix needs one CLSID per
language, which changes `src/tip/guids.h`'s design (its GUIDs are permanent: new ones would be
added, not changed) and is the owner's decision.

## Workaround

Open the settings window from the Start menu ("FKeyIME settings", added by the MSI) or run
`fkeyime-settings.exe` from the install folder. `ITfFnConfigure::Show` itself works (measured
through COM).
