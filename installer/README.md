# installer

WiX v5 MSI (milestone M5, first version). Build with `installer/build.ps1`; the output is
`build/installer/fkeyime-<version>-x64.msi`. The WiX CLI is pinned as a repo-local .NET tool
(`dotnet tool restore`); the script adds the UI and Util extensions (5.0.2) to the global WiX
extension cache. `dotnet tool run wix msi validate <msi>` runs the ICE checks.

What `Package.wxs` does:

- Per machine, x64 Windows. Files in `Program Files\FKeyIME`: `x64\fkeyime.dll`,
  `x86\fkeyime.dll`, `fkeyime-settings.exe`, `mozc\fkeyime_mozc_server.exe`, `LICENSE.txt`.
  Start menu: "FKeyIME settings". Log folder `%ProgramData%\fkeyime`, writable by Store apps.
- Registers the TIP with `regsvr32` (x64 and x86), the same code as the development install.
- As the signed-in user (`fkeyime-settings.exe --user-install`, `InstallLayoutOrTip`): adds
  FKeyIME EN / KO / JA to the language list and sets FKeyIME EN as the default input method.
  The Microsoft IMEs stay. Uninstall undoes both (`--user-uninstall`).
- Wizard: Welcome → "Do you have an HHKB Studio?" (if yes, the opt-in Mac Eisu / Kana keys,
  `MacLangKeys`) → Install → final page with where to report issues.

Not done yet (`docs/spec.md`, section 7): the device / system page, removing the Microsoft
IMEs, the keyboard layout and escape-hatch pages, signing, an ARM64 package. The UpgradeCode is
permanent.
