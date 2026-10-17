# Kanji conversion server crashes every 3 minutes

**Status:** solved (2026-10-17, commit 33209e2). Regression test: `tools/test-mozc-server.ps1`.

## Symptom

The event log showed `fkeyime_mozc_server.exe` crashing (exception `0xC0000409` in
`ucrtbase.dll`, offset `0xa527e`, which is `abort()`). Conversion kept working because the TIP
restarts the server on the next Space; it only showed as a short delay.

## Cause

Both crashes came 183–184 s after the server started *(measured from the event log)*. Every 180 s
Mozc's session watch dog connects to its own server with Mozc's client, which only accepts a
server at Mozc's install path. FKeyIME's server runs from its own folder, so the Cleanup and three
pings failed and the watch dog called `LOG(FATAL)`. Reproduced in 15 s with
`--watch_dog_interval=10`.

## Solution

`third_party/patches/mozc-fkeyime-server-path.patch`: when the process is the server, its own
folder is the server directory. The patched server survived 45 s of 10 s watch dog cycles and
still converts; the regression test fails on the unpatched server and passes on the patched one.

## Earlier server problems (also solved)

- **The server refused to start** (same exit code, at once): Mozc's server only runs inside
  Mozc's sandbox. Patch `mozc-fkeyime-unsandboxed-server.patch` lets it run without it, a known
  security gap to close before release (`../spec.md`, section 9).
- **The server ignored every key:** new Mozc sessions start with the IME off. The client turns it
  on (`TURN_ON_IME`) after creating a session.
