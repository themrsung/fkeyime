# FKeyIME

One Windows IME for **English, Korean and Japanese** on a JIS **HHKB Studio**, switched with the
**F keys**, which is where the name comes from:

| Key | Goes to | F17 in that mode |
| --- | --- | --- |
| **F18** | Korean 2-Set | Hanja |
| **F19** | English, JIS symbols | transient Caps Lock |
| **F20** | Japanese, kana typing (Mozc) | transient romaji |
| **F16** | Japanese, romaji typing (Mozc) | transient kana |

Every key is one-way: pressing it never toggles you out, and the JIS keycap symbols are right in
every mode. It replaces the AutoHotkey switching of
[`hhkb-ekj-trilingual`](https://github.com/themrsung/hhkb-ekj-trilingual) v1.5.2 (see
[`docs/migration-from-v1.5.2.md`](docs/migration-from-v1.5.2.md)).

> [!WARNING]
> **Early development: no installer yet.** English (JIS), Korean 2-Set, Japanese kana input
> (F6-F8) and kanji conversion (bundled Mozc server) work through a development install
> (`tools/spike.ps1`). No Hanja or romaji input yet. Status: [`docs/spec.md`](docs/spec.md),
> sections 4, 8 and 9.

## Build

Windows 11 x64 with Visual Studio 2022 Build Tools (C++, ATL), CMake, Ninja, Python 3.12+,
Bazelisk and the .NET SDK. See [`docs/toolchain.md`](docs/toolchain.md).

```powershell
tools/bootstrap.ps1        # check the toolchain (-Install to install it), set up the repo
tools/build.ps1            # build x64 + x86, run the tests
```

## License

[BSD 3-Clause](LICENSE). Mozc is BSD-3-Clause as well.
