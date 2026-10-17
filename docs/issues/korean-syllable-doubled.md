# A Korean syllable appears twice after an F-key

**Status:** solved (2026-10-17, commit fc127e4).

## Symptom

In a classic Win32 Edit control: 가, then F19, then `a` gave `가a가`; 가 then F19 alone gave `가가`.
Committing with Enter first worked.

## What didn't help

Comparing contexts by COM identity, skipping a redundant `SetText`, unpreserving the F-keys
while composing, and switching the profile after a posted message: all still doubled
*(measured)*.

## Cause and solution

Switching the profile right after an F-key commits a Korean composition makes the IMM
compatibility layer deliver the syllable a second time. FKeyIME now switches only after the app
has processed the commit: on a 30 ms timer (`WM_TIMER` comes after queued messages) or at the next
key, whichever comes first. Keys typed meanwhile already follow the new mode. Tested with every
key queued at once as well.
