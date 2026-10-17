# Clone Mozc at the pinned commit into third_party/mozc (git-ignored; see third_party/README.md).
# Building it is a separate, long step: see docs/toolchain.md, "Mozc".
param([string] $Commit = (Get-Content "$PSScriptRoot\..\third_party\mozc.commit" -Raw).Trim())
$ErrorActionPreference = 'Stop'
$dest = Join-Path $PSScriptRoot '..\third_party\mozc'

if (-not (Test-Path $dest)) {
    git clone --filter=blob:none https://github.com/google/mozc.git $dest
}
git -C $dest fetch origin $Commit
git -C $dest checkout --detach $Commit
git -C $dest submodule update --init --recursive
# FKeyIME's renames (own pipe, profile folder and names), so it never collides with a real Mozc.
foreach ($patch in Get-ChildItem "$PSScriptRoot\..\third_party\patches\*.patch") {
    git -C $dest apply --check $patch.FullName 2>$null
    if ($LASTEXITCODE -eq 0) { git -C $dest apply $patch.FullName; Write-Host "applied $($patch.Name)" }
}
Write-Host "Mozc at $(git -C $dest rev-parse HEAD)"
