# Check (and with -Install, install) everything needed to build FKeyIDE, then wire up the repo.
#   tools/bootstrap.ps1            # report only
#   tools/bootstrap.ps1 -Install   # install what's missing with winget (may prompt for UAC)
param([switch] $Install)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path "$PSScriptRoot\.."
$missing = @()

function Check($name, [scriptblock] $test, $wingetId) {
    $ok = try { & $test } catch { $false }
    Write-Host ('{0,-36} {1}' -f $name, $(if ($ok) { 'ok' } else { 'MISSING' }))
    if (-not $ok -and $wingetId) { $script:missing += $wingetId }
    if (-not $ok -and -not $wingetId) { $script:manual += $name }
}
$manual = @()

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
function HasVs($component) {
    (Test-Path $vswhere) -and (& $vswhere -latest -products * -requires $component -property installationPath)
}

Check 'Git'                         { Get-Command git -ErrorAction Stop }        'Git.Git'
Check 'CMake >= 3.28'               { [version]((cmake --version)[0] -split ' ')[2] -ge [version]'3.28' } 'Kitware.CMake'
Check 'Ninja'                       { Get-Command ninja -ErrorAction Stop }      'Ninja-build.Ninja'
Check 'Python >= 3.12 (Mozc)'       { [version]((python --version) -split ' ')[1] -ge [version]'3.12' } 'Python.Python.3.12'
Check 'Bazelisk (Mozc)'             { Get-Command bazelisk -ErrorAction Stop }   'Bazel.Bazelisk'
Check '.NET SDK >= 6 (WiX, Mozc)'   { (dotnet --list-sdks) -match '^([6-9]|\d\d)\.' } 'Microsoft.DotNet.SDK.8'
Check 'VS Build Tools: C++ x64/x86' { HasVs 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64' } 'Microsoft.VisualStudio.2022.BuildTools'
Check 'VS Build Tools: ATL x86/x64' { HasVs 'Microsoft.VisualStudio.Component.VC.ATL' } $null
Check 'Windows SDK 10.0.22621+'     { Test-Path "${env:ProgramFiles(x86)}\Windows Kits\10\Include\10.0.2*" } $null
Write-Host ('{0,-36} {1}' -f 'ARM64 tools (optional)', $(if (HasVs 'Microsoft.VisualStudio.Component.VC.Tools.ARM64') { 'ok' } else { 'not installed' }))

if ($Install -and $missing) {
    foreach ($id in $missing) {
        $extra = @()
        if ($id -eq 'Microsoft.VisualStudio.2022.BuildTools') {
            $extra = @('--override', '--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.ATL --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --includeRecommended')
        }
        winget install --id $id -e --accept-source-agreements --accept-package-agreements @extra
    }
} elseif ($missing) {
    Write-Host "`nRun with -Install to install: $($missing -join ', ')" -ForegroundColor Yellow
}
if ($manual) {
    Write-Host "`nAdd in the Visual Studio Installer (Modify): $($manual -join ', ')" -ForegroundColor Yellow
}

# Repo wiring: commit hook and local .NET tools (WiX).
git -C $root config core.hooksPath .githooks
if (Get-Command dotnet -ErrorAction SilentlyContinue) {
    Push-Location $root; try { dotnet tool restore } finally { Pop-Location }
}
