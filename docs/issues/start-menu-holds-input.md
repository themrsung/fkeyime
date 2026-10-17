# Mouse, trackpad and Tab dead after a test

**Status:** solved. Caused by a development test, not by FKeyIME in normal use.

## Symptom

2026-10-17: the USB mouse, the pointing stick and the trackpad stopped responding, and Tab didn't
move focus, from any keyboard. Keys still reached the app in front.

## Cause *(measured)*

`fkeyime_probe --typing "TAP cycle with Right Win"` injected Right Win taps. They opened the Start
menu, and its process (`StartMenuExperienceHost`) stayed in front, covering the screen under the
cursor and holding the input, after the visible menu had been closed with Escape. No stuck
modifier keys, no cursor clipping. A Settings window was also hung, unrelated.

## Solution

```powershell
Stop-Process -Name StartMenuExperienceHost -Force
```

Windows starts it again at once; input came back. The Right Win test was removed from the probe.
Never inject Win keys on a real PC: test them in a VM.
