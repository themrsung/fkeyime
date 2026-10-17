# Build the FKeyIME MSI: build/installer/fkeyime-<version>-x64.msi.
#   installer/build.ps1            # builds FKeyIME Release (x64 + x86) first
#   installer/build.ps1 -NoBuild   # package what's already in build/
# Needs the Mozc server (tools/build-mozc-server.ps1) in build/mozc.
param([switch] $NoBuild)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..").Path

if (-not $NoBuild) {
    & "$root\tools\build.ps1" -Config Release
    if ($LASTEXITCODE) { throw 'FKeyIME build failed' }
}
$version = (Select-String "$root\CMakeLists.txt" -Pattern 'project\(fkeyime VERSION ([0-9.]+)').Matches[0].Groups[1].Value
foreach ($f in 'x64\src\tip\Release\fkeyime.dll', 'x86\src\tip\Release\fkeyime.dll',
               'x64\src\settings\Release\fkeyime-settings.exe', 'mozc\fkeyime_mozc_server.exe') {
    if (-not (Test-Path "$root\build\$f")) { throw "missing build\$f" }
}

Push-Location $root
try {
    dotnet tool restore | Out-Null
    foreach ($ext in 'WixToolset.UI.wixext/5.0.2', 'WixToolset.Util.wixext/5.0.2') {
        dotnet tool run wix extension add -g $ext | Out-Null
    }
    New-Item "$root\build\installer" -ItemType Directory -Force | Out-Null
    $msi = "$root\build\installer\fkeyime-$version-x64.msi"
    dotnet tool run wix build "$root\installer\Package.wxs" -arch x64 `
        -ext WixToolset.UI.wixext -ext WixToolset.Util.wixext `
        -d "Version=$version" -d "BuildDir=$root\build" -d "SourceDir=$root" -o $msi
    if ($LASTEXITCODE) { throw "wix build failed ($LASTEXITCODE)" }
    Get-Item $msi | Select-Object Name, Length
} finally {
    Pop-Location
}
