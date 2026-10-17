# Development notes: pitfalls met while building and testing

## Toolchain

- `Launch-VsDevShell.ps1` wants `amd64`, not `x64`, and VsDevCmd looks for `vswhere` on `PATH`;
  native stderr must not abort the script on PowerShell 5.1 (`tools/dev.ps1` handles all three).
- Mozc's Bazel build needs symlinks (Developer Mode or admin). `--nowindows_enable_symlinks`
  copies instead (`../toolchain.md`).
- The server target builds in a transitioned configuration: `bazel info bazel-bin` points at the
  wrong folder; `tools/build-mozc-server.ps1` asks `cquery --output=files` for the file.
- Patch files written by PowerShell got CRLF line endings and `git apply` rejected them.
  `*.patch` is now `eol=lf` in `.gitattributes`; write them from Git Bash.

## Installing the development build (`tools/spike.ps1`)

- The language-list loop iterated the list as one object, so no FKeyIME profile was added and an
  empty en-US entry appeared (commit 402e58b). Iterate `.ToArray()`.
- A process that has the DLL loaded keeps the file open: the script moves the old DLL aside
  (`fkeyime.dll.old-*`) and copies the new one. Those processes keep the old version until they
  restart (`apps-dont-load-fkeyime.md`).

## Testing

- Mozc learns the candidates you pick, in its profile folder. Picking a non-first candidate once
  (いく in a manual test) changed the order for later runs. Tests only accept first candidates or
  cancel.
- The probe's stdout is buffered when redirected; find its window by title instead of reading
  its handle from the output.
- Pressing a key on the real keyboard during a probe run spoils it (one 50-round run gave 77/100
  while the owner pressed a key).
- Typing escapes (`\n`, `\u`) through shell heredocs got mangled several times; write patch
  scripts to files instead.
