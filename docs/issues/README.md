# Issues found so far, and what solved them

Problems met while building and using FKeyIME on the owner's PC (Windows 11 Pro 25H2, HHKB Studio
JIS, keymap Profile 4), with the cause and the fix for each. Read the matching file before
"fixing" something again. Measured facts say *(measured)*; guesses say *(inferred)* or
*hypothesis*.

## Using FKeyIME

| Issue | Status |
| --- | --- |
| [Backspace sends the message (types Enter), stray `3`](backspace-sends-enter.md) | Open: hypothesis, the HHKB rests on the laptop's keys |
| [A Hanja pick also types the candidate's number](hanja-pick-types-a-digit.md) | Gone; not reproducible. Probably the same as above |
| [After a reboot, the F-keys go to the Microsoft IMEs](sign-in-starts-on-microsoft-ime.md) | Solved: default input method set to FKeyIME EN |
| [Korean and kana type English letters in some apps](apps-dont-load-fkeyime.md) | Solved: restart apps after installing FKeyIME |
| [AutoHotkey takes the F-keys away from FKeyIME](autohotkey-takes-the-f-keys.md) | Solved in the AutoHotkey script (Win+Space decides) |
| [Kanji conversion server crashes every 3 minutes](conversion-server-crashes.md) | Solved: Mozc patch |
| [Korean jamo come out one by one in classic apps](korean-jamo-not-composed.md) | Solved: interim-character selection |
| [A Korean syllable appears twice after an F-key](korean-syllable-doubled.md) | Solved: delayed profile switch |
| [Shift splits what's being typed](shift-splits-the-composition.md) | Solved |
| [Symbols in English and Korean are US, not JIS](us-symbols-in-english-and-korean.md) | Solved: FKeyIME's own key table |
| [USB mouse dead after a reboot](usb-mouse-after-reboot.md) | Not FKeyIME: the USB-C dock's hub port |

## Building and testing

[`development-notes.md`](development-notes.md): toolchain, Bazel, install script and test pitfalls.
