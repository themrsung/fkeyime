# third_party

- `mozc.commit`: the pinned [google/mozc](https://github.com/google/mozc) commit (BSD-3-Clause).
  `tools/fetch-mozc.ps1` clones it into `third_party/mozc/`, which is git-ignored, and applies
  `patches/`.
- `patches/`: FKeyIME's changes to Mozc (`docs/spec.md`, section 9): own names, pipe and
  profile folder (`mozc-fkeyime-names.patch`), and a server that runs without Mozc's sandbox
  (`mozc-fkeyime-unsandboxed-server.patch`, to be replaced before a release).
