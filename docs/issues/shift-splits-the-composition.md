# Shift splits what's being typed

**Status:** solved (2026-10-17, commit 7091f80).

## Symptom

わたしは… and とうきょう… converted wrongly: Mozc received only ょうにいく, because とうき had already
been committed when Shift was pressed for ょ *(measured with temporary logging)*. In Korean,
있 (ㅇ ㅣ Shift+ㅆ) would have committed 이 first.

## Cause

A modifier key on its own counted as "another key", which ends the composition.

## Solution

Shift, Ctrl, Alt, Win and Caps Lock pressed on their own pass through without touching the
composition. Probe cases "KO Shift inside a syllable" (있) and "CONV Shift doesn't split the
kana".
