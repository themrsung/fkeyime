# Build FKeyIME's Mozc conversion server and copy it to build\mozc\fkeyime_mozc_server.exe.
# Needs tools/fetch-mozc.ps1 first (it applies third_party/patches/). x64 only: the x86 TIP talks
# to the same server process. About a minute with a warm Bazel cache (docs/toolchain.md).
$ErrorActionPreference = 'Stop'
$root = Resolve-Path "$PSScriptRoot\.."
$src = Join-Path $root 'third_party\mozc\src'
Push-Location $src
try {
    # Symlinks need Developer Mode or admin; copying works without either.
    $ErrorActionPreference = 'Continue'
    bazelisk --nowindows_enable_symlinks build //server:mozc_server_win --config release_build
    if ($LASTEXITCODE) { throw "bazel build failed ($LASTEXITCODE)" }
    # The target builds in a transitioned configuration, so ask for its file, not bazel-bin.
    $exec = (bazelisk --nowindows_enable_symlinks info --config release_build execution_root 2>$null | Select-Object -Last 1).Trim()
    $file = (bazelisk --nowindows_enable_symlinks cquery //server:mozc_server_win --config release_build --output=files 2>$null | Select-Object -Last 1).Trim()
} finally {
    Pop-Location
}
$ErrorActionPreference = 'Stop'
$dest = Join-Path $root 'build\mozc'
New-Item $dest -ItemType Directory -Force | Out-Null
Copy-Item (Join-Path $exec $file) (Join-Path $dest 'fkeyime_mozc_server.exe') -Force
Write-Host "Built $dest\fkeyime_mozc_server.exe"
