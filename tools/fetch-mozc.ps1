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
Write-Host "Mozc at $(git -C $dest rev-parse HEAD)"
