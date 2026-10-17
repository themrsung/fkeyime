# Over Parsec from a Mac, only English works

**Status:** open (2026-10-18). Typing works; switching modes doesn't. Needs an owner decision.

## Setup

HHKB HYBRID Type-S JIS on a Mac mini, set up as hhkb-ekj-trilingual's macOS guide says
(Karabiner: the HHKB key → F18, 英数 and かな keys). Parsec client on the Mac, Parsec host on the
owner's Windows PC over LAN, FKeyIME (development install) on the Windows side.

## What arrives on Windows (measured, passive low-level hook + Raw Input, 2026-10-18)

Every key from Parsec is **injected** (`LLKHF_INJECTED`) with a virtual key and a scan code.

| Pressed on the Mac | Arrives on Windows |
| --- | --- |
| Letters, digits, Shift, Space, Backspace, Enter | Normal scan codes (US positions, which are also the JIS positions for these keys) |
| Shift+2, the `[` and `:` keys | scan 0x03 with Shift, 0x1B, 0x28 |
| ¥ | scan 0x7D (VK 0xFF), the JIS ¥ position |
| F18 (HHKB key), F19, F20, 英数, かな | **nothing** |
| Ctrl | **nothing** (owner: "control doesn't come through") |
| (something with Option) | Alt+Esc (scan 0x01 with Alt), three times. Alt+Esc switches windows on Windows, which may be the "acts really weird" part |

With FKeyIME EN active, the owner typed `12"[:¥`: the JIS symbols are right, because FKeyIME maps
scan codes with its own JIS table (spec section 8) and Parsec keeps the key positions.

## Two separate causes

1. **FKeyIME wasn't running at first.** After the 01:42 reboot nothing loaded FKeyIME
   (`sign-in-starts-on-microsoft-ime.md`), so the first Parsec test went through the Microsoft
   IMEs and US layout: US ANSI symbols. Picking FKeyIME ENG in the taskbar fixed that part.
2. **Parsec from macOS doesn't forward F18–F20, 英数, かな or Ctrl** *(measured: they never
   reach Windows)*. Why (Parsec's Mac key table, Karabiner's order, a Parsec setting) isn't
   known. FKeyIME can't act on keys Windows never receives.

## Options (owner's decision)

- **Karabiner rule for Parsec only** (no FKeyIME change): when Parsec is frontmost, send keys
  Parsec does forward. FKeyIME's escape hatch `CycleKeys = 1` turns Alt+` into "next mode"
  (EN → KO → JA), so Karabiner could map the HHKB key to Option+`. Costs: one cycling key
  instead of three one-way keys, and Option+` must actually arrive as Alt+` (not measured).
- **A new FKeyIME option: three forwardable combos as one-way mode keys** (for remote desktops),
  for example Ctrl+Alt+1/2/3, with Karabiner mapping F18/F19/F20 to them for Parsec. Keeps
  one-way keys, but adds key bindings (AGENTS.md: language input only; the owner decides).
- **Find out whether Parsec can forward F13–F24** (a Parsec setting or a newer client). Not
  tested.

Ctrl not arriving is a Parsec/Karabiner matter on the Mac, not FKeyIME's.
