# Korean jamo come out one by one in classic apps

**Status:** solved (2026-10-17, commit fc127e4).

## Symptom

In a classic Win32 Edit control, Korean typed ㅇㅏㄴㄴㅕㅇ instead of 안녕: every jamo was committed
on its own. Japanese compositions in the same control worked.

## Cause

The log showed the app ending every Korean composition right after it started
(`OnCompositionTerminated` after each key) *(measured)*. Apps that reach TSF through the IMM
compatibility layer treat a Korean composition with a plain caret as finished.

## Solution

Like the Microsoft Korean IME, FKeyIME selects the composing syllable as an **interim
character** (a block cursor) instead of putting the caret after it.
