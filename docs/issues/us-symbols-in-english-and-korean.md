# Symbols in English and Korean are US, not JIS

**Status:** solved by design (decision 8 (b), commit fc127e4).

## Symptom

In M0, FKeyIME EN and KO typed US symbols (Shift+2 → `@`, ろ and ¥ nothing), only JA typed JIS
*(measured)*.

## Cause

Windows ignores the JIS substitute layout the profiles register: each language keeps its own
layout (en-US `kbdus`, Korean `kbd101a`).

## Solution

FKeyIME types every character from its own JIS table by scan code in all three modes
(`src/core/jis_layout.*`), macOS style: ¥ types ¥, ろ types `\` and `_`. Apps that read raw keys
(games) still see Windows' layout.
