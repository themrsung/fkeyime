# A Hanja pick also types the candidate's number

**Status:** gone, not reproducible (2026-10-17).

## Symptom

In Notepad, with the **Microsoft Korean IME** (the AutoHotkey path: F18, then F17 for 한자),
picking a Hanja with Down and Enter also typed the candidate's number.

## What was checked

- FKeyIME wasn't involved: it never loaded in that Notepad *(log, loaded modules)*.
- Injected keys picked correctly every time, with the digit 1, Enter, Down+Enter and Down+2, in a
  Win32 Edit and in a Notepad scratch tab, over two rounds *(`fkeyime_probe.exe --hanja`,
  `--hanja-fg`)*.

## Likely cause

Probably the same as [Backspace sends the message](backspace-sends-enter.md): the HHKB resting on
the laptop's keyboard, pressing a laptop key such as a keypad digit *(hypothesis)*.
