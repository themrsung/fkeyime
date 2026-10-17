# Windows Settings doesn't list FKeyIME under the language's keyboards

**Status:** open, narrowed (2026-10-18). **Installed with the MSI in Windows Sandbox, FKeyIME
KO is listed** under Korean's installed keyboards, but with only "Remove": no keyboard
options, unlike Mozc. On the owner's PC (development install) it still isn't listed at all.

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

**Not the shared CLSID either** *(measured 2026-10-18, with the owner's OK)*: with the TSF key
exported first, FKeyIME's EN and KO profile keys were deleted so only FKeyIME JA remained under
its CLSID, like Mozc's single profile. Settings still listed only the Microsoft IME and Mozc
under 일본어. Re-registering restored all three profiles. Both display strings load
(`SHLoadIndirectString`: "FKeyIME JA", "Mozc").

All of the kept changes: they match how Mozc's TIP registers (`third_party/mozc/src/win32/base/tsf_registrar.cc`:
same categories, a display name, an icon index), they're harmless, and the substitute layout had
no effect on typing (M0). Typing hasn't been re-measured since, though.

## Cause

Not known. Settings lists Mozc, so third-party IMEs are listed, and FKeyIME's HKLM TSF
registration now has the same shape as Mozc's. Differences left, untested: Mozc was installed
by an MSI (its custom actions may write more than its TIP registration code shows); its DLLs
are in `Program Files (x86)\Mozc`; it was added to the language list by its installer rather
than by `Set-WinUserLanguageList`. Next: install FKeyIME with its own MSI (in a VM) and check
Settings there.

Side effect of this test: uninstalling Mozc with `msiexec /x … /qb` rebooted the owner's PC
without warning (exit 1641). Always pass `/norestart`.

## MSI install in Windows Sandbox (2026-10-18)

Windows 11 24H2 (26100) in Windows Sandbox, a clean user, FKeyIME installed with
`installer/build.ps1`'s MSI (`installer/sandbox/`). Settings → Language & region → Korean →
Language options → Installed keyboards: **FKeyIME is listed** (owner, by eye), with Remove
only, no keyboard options. So on the owner's PC the missing entry comes from that PC: its
development install, the way the profiles first entered its language list
(`Set-WinUserLanguageList`), or 25H2 *(hypotheses)*. Remaining question: what makes Settings
offer keyboard options (Mozc has them; FKeyIME implements the same `ITfFnConfigure`). 

Third Sandbox pass (owner, by eye): FKeyIME is listed under **all three** languages (Korean,
Japanese, English), each with only "Remove" in its ⋯ menu. FKeyIME's log in the Sandbox has no
`DllGetClassObject` entry from Settings, so Settings decides about keyboard options without
loading the TIP *(inferred: the log also stays empty when no app has used FKeyIME yet)*. No
Microsoft documentation found on how Settings decides (searched 2026-10-18). Next idea: install
Mozc in the Sandbox next to FKeyIME and diff everything Settings could read (registry under
`CTF`, `Control Panel\International`, the package's file properties).

## Workaround

Open the settings window from the Start menu ("FKeyIME settings", added by the MSI) or run
`fkeyime-settings.exe` from the install folder. `ITfFnConfigure::Show` itself works (measured
through COM).
