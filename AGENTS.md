# AGENTS.md — FKeyIME

FKeyIME (`fkeyime`; `f_key_ime` only where snake case is required) is a Windows TSF IME for
English / Korean / Japanese on a JIS HHKB Studio, switched with F17–F20. Read `docs/spec.md`
(behavior, architecture, milestones, open questions) and `docs/toolchain.md` (build) first.

| Path | What |
| --- | --- |
| `src/core/` | Mode logic (F-key rules), pure C++, no Windows headers. Unit-tested |
| `src/tip/` | The TSF text input processor DLL (`fkeyime.dll`). `guids.h` is permanent |
| `tests/` | Unit tests (`tools/build.ps1` runs them) |
| `docs/issues/` | Issues met so far, with cause and solution. Read the matching one before "fixing" something again |
| `docs/api.md` | Public API (`fkeyime_api.h`): is FKeyIME active in a window? Version 1, append-only |
| `examples/autohotkey/` | The v1.5.2 AutoHotkey script, made FKeyIME-aware with the API |
| `tools/` | `bootstrap.ps1`, `build.ps1`, `dev.ps1`, `fetch-mozc.ps1` |
| `third_party/` | Pinned Mozc commit |
| `installer/` | WiX MSI (M5) |
| `docs/migration-from-v1.5.2.md` | **Maintained every commit.** For users of hhkb-ekj-trilingual v1.5.2 |
| `docs/legacy-repo-2.0.0.md` | **Maintained every commit.** For agents bumping hhkb-ekj-trilingual to v2.0.0 |

## Rule: every commit updates both maintained docs

Every commit must update **both** `docs/migration-from-v1.5.2.md` and `docs/legacy-repo-2.0.0.md`:

1. Bring their content in line with what the commit changes: status banners, ⏳ markers, the
   tradeoff table, the readiness gate, file names, values, steps.
2. Add one changelog entry at the top of each file's "Changelog of this guide", even if it only
   says "no user-visible change".
3. Keep them true. Don't describe anything as working until it has been measured.

`.githooks/pre-commit` rejects commits that don't stage both files (`tools/bootstrap.ps1` sets
`core.hooksPath`). Don't bypass it with `--no-verify`.

## Other rules

- Never change the GUIDs in `src/tip/guids.h`: they're in users' registries and language lists.
- Keep `src/core/` free of Windows headers, so its tests run anywhere.
- ARM64 isn't built or shipped, but don't block it: no inline asm, no hardcoded `x64`/`x86`
  paths or pointer sizes. Forks add ARM64 with the `arm64` preset (`docs/toolchain.md`).
- Scope is language input only (`docs/spec.md`, section 1). Don't add other key bindings.
- The spec follows the owner's decisions. Open questions stay open until the owner answers;
  implement the proposed default and make it easy to change (`Policy`).
- Ask before installing system-wide software, registering the TIP, or changing language or
  keyboard settings on the host. Test registration in a VM first.
- Measure, don't assume. Mark facts as documented, measured or inferred.
- Code style: `.clang-format` (Google-based, 4 spaces, 100 columns).
