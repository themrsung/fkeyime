# Testing the MSI in Windows Sandbox

A disposable Windows to install, check and uninstall FKeyIME without touching the host. Needs the
Windows Sandbox feature (Windows 11 Pro; enabling it needs a restart).

1. Make a folder outside the repo, for example `%USERPROFILE%\Desktop\fkeyime-sandbox`, with
   `fkeyime.msi` (from `build/installer/`), the four files here, an empty `results` folder, and
   `fkeyime-test.wsb` below (put the folder's path in `HostFolder`).
2. Open the `.wsb`. In the Sandbox, `C:\fkeyime` is that folder, shared with the host.
3. `1-install.cmd` (the full wizard), `2-check.cmd` (with Settings open on the page to check),
   `3-uninstall.cmd`. Every msiexec call has `/norestart` and a verbose log in `results`.

```xml
<Configuration>
  <vGPU>Disable</vGPU>
  <MappedFolders>
    <MappedFolder>
      <HostFolder>C:\Users\YOU\Desktop\fkeyime-sandbox</HostFolder>
      <SandboxFolder>C:\fkeyime</SandboxFolder>
      <ReadOnly>false</ReadOnly>
    </MappedFolder>
  </MappedFolders>
  <LogonCommand><Command>explorer.exe C:\fkeyime</Command></LogonCommand>
</Configuration>
```

`vGPU` off: with it on, the Sandbox stayed black on the owner's PC (2026-10-18) while holding
the keyboard and mouse (Ctrl+Alt+Break releases them). The Sandbox runs the host's Windows
servicing base, which can differ from the host's own version: 24H2 (26100) on a 25H2 (26200)
host.
