# Backspace sends the message (types Enter), stray `3`

**Status:** open, hypothesis (2026-10-17).

## Symptom

While typing in the Claude app, pressing Backspace sent the message as if Enter had been pressed,
and a stray `3` was typed in between. After a restart, Backspace worked normally again.

## What was checked

- FKeyIME's log: Claude was in **English** mode. In English, Backspace isn't in FKeyIME's key
  table and nothing is being composed, so FKeyIME passes it on untouched *(code and log)*.
- Injected keys gave no Enter in a Win32 Edit, a TSF RichEdit or Edge (Chromium): English `hu`
  Backspace `x` → `hx`; Backspace that empties a Korean, kana or kanji-conversion composition
  deletes cleanly; no `Enter` key event in Edge's key log *(measured with `fkeyime_probe.exe
  --typing` cases "EN backspace" and `--type-fg` on a test page)*.

## Hypothesis (owner's)

The HHKB rests on top of the laptop. Pressing Backspace (top right of the HHKB) may also press
the laptop's own keys underneath, such as the numeric keypad's Enter and `3`. That explains both
the Enter and the `3`, and why injected keys never show it.

## What to do

Move the HHKB off the laptop. If it happens again, record the keys with
`fkeyime_probe.exe --keylog 40` while reproducing it: the laptop's keys come from a different
keyboard than the HHKB and show up as extra key events.
