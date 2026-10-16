# Toolchain

Everything needed to build FKeyIDE on Windows 11 x64. `tools/bootstrap.ps1` checks all of it
(`-Install` installs what's missing with winget) and wires up the repo (commit hook, WiX).

| Tool | Version (this PC, 2026-10-17) | Used for |
| --- | --- | --- |
| Visual Studio 2022 Build Tools: MSVC v143 x64/x86, ATL x64/x86 | 17.14.41, MSVC 14.44 | the TIP DLL; Mozc |
| Windows SDK | 10.0.26100 | TSF / Win32 headers |
| CMake | 4.4.3 (needs ≥ 3.28) | FKeyIDE build (`CMakePresets.json`) |
| Ninja | 1.13.2 | CMake generator |
| Git | Git for Windows | repo, Mozc checkout |
| Python | 3.12.10 (Mozc needs ≥ 3.12) | Mozc's `update_deps.py` and build scripts |
| Bazelisk | 1.29.0 | building Mozc |
| .NET SDK | 8.0.426 (Mozc and WiX need ≥ 6) | WiX; Mozc's .NET tools |
| WiX Toolset | 5.0.2, repo-local (`.config/dotnet-tools.json`) | the MSI (M5): `dotnet tool run wix` |

Not installed (optional): **ARM64 MSVC + ATL** (only if ARM64 Windows is needed, open question 4;
add them in the Visual Studio Installer), **GitHub CLI**, **clang-format** (Mozc's
`update_deps.py` downloads LLVM 20, which includes it).

Mozc downloads its own LLVM, MSYS2, Ninja and Qt with `update_deps.py`. They stay inside
`third_party/mozc` and aren't installed system-wide.

## Build FKeyIDE

```powershell
tools/bootstrap.ps1                   # once: check tools, set core.hooksPath, restore WiX
tools/build.ps1                       # x64 + x86 Debug, runs the unit tests
tools/build.ps1 -Config Release
tools/dev.ps1 -Arch x64 -Command 'cmake --build --preset x64-debug'   # anything in a VS dev env
```

Outputs: `build/<arch>/src/tip/<Config>/fkeyide.dll` and
`build/<arch>/tests/<Config>/fkeyide_tests.exe`.

## Mozc (milestone M0)

```powershell
tools/fetch-mozc.ps1                  # clone the pinned commit into third_party/mozc
cd third_party/mozc/src
python build_tools/update_deps.py
python build_tools/build_qt.py --release --confirm_license
bazelisk build package --config release_build
```

Not run yet. The Qt build alone takes a long time, so budget for it.
