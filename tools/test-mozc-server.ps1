# Regression test for the conversion server: starts it with a 10 s session watch dog (Mozc's
# default is 180 s) and fails if it exits within 30 s. Before third_party/patches/
# mozc-fkeyime-server-path.patch, the watch dog rejected the server's own path and aborted it
# (0xC0000409) on its first cycle. Stops any running server first; the TIP restarts it on demand.
#   tools/test-mozc-server.ps1 [-Server <path>]   (default: build\mozc\fkeyime_mozc_server.exe)
param([string] $Server = "$PSScriptRoot\..\build\mozc\fkeyime_mozc_server.exe")
$ErrorActionPreference = 'Stop'
Get-Process fkeyime_mozc_server -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
$p = Start-Process (Resolve-Path $Server) -ArgumentList '--watch_dog_interval=10' -PassThru
foreach ($i in 1..30) {
    Start-Sleep -Seconds 1
    if ($p.HasExited) {
        Write-Host ("FAIL: server exited after {0} s with 0x{1:X8}" -f $i, $p.ExitCode) -ForegroundColor Red
        exit 1
    }
}
Stop-Process -Id $p.Id -Force
Write-Host 'PASS: server survived 30 s of 10 s watch dog cycles'
exit 0
