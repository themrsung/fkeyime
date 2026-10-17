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

## Cause

Not known. Hypotheses, untested: Settings shows only profiles with a `Display Description`
(set by `ITfInputProcessorProfiles::SetLanguageProfileDisplayName`, an MUI string resource), or
only profiles whose icon loads.

## Workaround

Open the settings window from the Start menu ("FKeyIME settings", added by the MSI) or run
`fkeyime-settings.exe` from the install folder. `ITfFnConfigure::Show` itself works (measured
through COM).

## Next steps

Give the DLL an icon and string resources, register a display name per profile, and check
the Settings page again (VM first if possible).
