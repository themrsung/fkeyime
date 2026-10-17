# Toolchain

Everything needed to build FKeyIME on Windows 11 x64. `tools/bootstrap.ps1` checks all of it
(`-Install` installs what's missing with winget) and wires up the repo (commit hook, WiX).

| Tool | Version (this PC, 2026-10-17) | Used for |
| --- | --- | --- |
| Visual Studio 2022 Build Tools: MSVC v143 x64/x86, ATL x64/x86 | 17.14.41, MSVC 14.44 | the TIP DLL; Mozc |
| Windows SDK | 10.0.26100 | TSF / Win32 headers |
| CMake | 4.4.3 (needs ≥ 3.28) | FKeyIME build (`CMakePresets.json`) |
| Ninja | 1.13.2 | CMake generator |
| Git | Git for Windows | repo, Mozc checkout |
| Python | 3.12.10 (Mozc needs ≥ 3.12) | Mozc's `update_deps.py` and build scripts |
| Bazelisk | 1.29.0 (fetches Bazel 9.0.2 for Mozc) | building Mozc |
| .NET SDK | 8.0.426 (Mozc and WiX need ≥ 6) | WiX; Mozc's .NET tools |
| WiX Toolset | 5.0.2, repo-local (`.config/dotnet-tools.json`) | the MSI (M5): `dotnet tool run wix` |
| GitHub CLI | 2.102.0 (not logged in; `gh auth login` to use it directly) | the GitHub repo |

Not installed (optional): **ARM64 MSVC + ATL**. ARM64 isn't built, tested or shipped, but it isn't
blocked either (`spec.md`, section 2, "Builds"). A fork can add it: install "MSVC v143 ARM64/ARM64EC
build tools" and "C++ ATL … (ARM64/ARM64EC)" in the Visual Studio Installer, then run
`tools/build.ps1 -Arch arm64` (the `arm64` CMake preset). Also not installed: **clang-format**
(Mozc's `update_deps.py` downloads LLVM 20, which includes it).

Mozc downloads its own LLVM, MSYS2, Ninja and Qt with `update_deps.py`. They stay inside
`third_party/mozc` and aren't installed system-wide.

## Build FKeyIME

```powershell
tools/bootstrap.ps1                   # once: check tools, set core.hooksPath, restore WiX
tools/build.ps1                       # x64 + x86 Debug, runs the unit tests
tools/build.ps1 -Config Release
tools/dev.ps1 -Arch x64 -Command 'cmake --build --preset x64-debug'   # anything in a VS dev env
```

Outputs: `build/<arch>/src/tip/<Config>/fkeyime.dll` and
`build/<arch>/tests/<Config>/fkeyime_tests.exe`.

## The conversion server

```powershell
tools/fetch-mozc.ps1                  # clone the pinned commit, apply third_party/patches/
tools/build-mozc-server.ps1           # build //server:mozc_server_win, copy it to build\mozc\
```

`tools/spike.ps1 -Install` copies `build\mozc\fkeyime_mozc_server.exe` next to the TIP. With a
warm Bazel cache (after the full build below) the server builds in about a minute; it doesn't
need Qt.

## Mozc (milestone M0)

```powershell
tools/fetch-mozc.ps1                  # clone the pinned commit into third_party/mozc
cd third_party/mozc/src
python build_tools/update_deps.py
python build_tools/build_qt.py --release --confirm_license
bazelisk --nowindows_enable_symlinks build package --config release_build
```

Output: `third_party/mozc/src/bazel-bin/win32/installer/Mozc64.msi` (26.4 MB). It's built for
testing only. Don't install it next to FKeyIME: it registers its own ja-JP TIP.

**Measured 2026-10-17** on this PC (i7-1165G7, 4 cores / 8 threads, 16 GB RAM), Mozc 0136c357
("3.35.6354"):

| Step | Time | Notes |
| --- | --- | --- |
| `fetch-mozc.ps1` | 11 s | blobless clone, 201 MB working tree |
| `update_deps.py` | 11 min | downloads Bazel 9.0.2 (via Bazelisk), LLVM 20.1.1, MSYS2 2025-02-21, Ninja, Qt 6.9.1 sources and the .NET tools; `third_party_cache` is 992 MB |
| `build_qt.py` | 17 min 43 s | two CMake/Ninja stages (host tools, then target) of about 1,031 steps each, MSVC 19.44 |
| `bazelisk build package` | 28 min (1,680 s) | 5,115 actions, clang-cl from LLVM 20.1.1, critical path 192 s. About 2,500 compiler warnings, no errors |

Problems hit:

- **Symlinks.** Mozc's `.bazelrc` sets `startup --windows_enable_symlinks`, and Bazel then fails
  in 25 s with `createSymbolicLinkW failed (permission denied). Either Windows developer mode or
  admin privileges are required.` Fix without changing the system: the startup flag
  `--nowindows_enable_symlinks` (in the command above), so Bazel copies instead. It worked.
  Turning on Developer Mode would also fix it, but that's a system setting, so ask the owner first.
- **Disk.** Everything stays under `third_party/mozc` and `%USERPROFILE%\_bazel_<user>`
  (Bazel's output base). Neither is in git. `bazel clean --expunge` frees the output base.
- Qt (LGPL) is only needed for `mozc_tool` (settings and dictionary GUI). `mozc_server` alone
  doesn't need it *(inferred from the BUILD files, not built separately yet)*. Option B
  (`spec.md`, section 6.4) may skip the Qt step.
