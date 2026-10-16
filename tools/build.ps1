# Configure, build and test FKeyIDE for each architecture.
#   tools/build.ps1                    # x64 + x86, Debug
#   tools/build.ps1 -Config Release
param(
    [string[]] $Arch = @('x64', 'x86'),
    [ValidateSet('Debug', 'Release')] [string] $Config = 'Debug'
)
$ErrorActionPreference = 'Stop'
$cfg = $Config.ToLower()

foreach ($a in $Arch) {
    Write-Host "== $a $Config ==" -ForegroundColor Cyan
    & "$PSScriptRoot\dev.ps1" -Arch $a -Command "cmake --preset $a; if (`$?) { cmake --build --preset $a-$cfg }; if (`$? -and '$cfg' -eq 'debug') { ctest --preset $a-debug }"
    if ($LASTEXITCODE) { throw "$a $Config failed ($LASTEXITCODE)" }
}
