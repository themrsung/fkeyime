# USB mouse dead after a reboot

**Status:** not FKeyIME; hardware (2026-10-17).

## Symptom

After a reboot the USB mouse didn't work. The laptop's touchpad and the HHKB's pointing stick over
Bluetooth did.

## Cause

Windows listed no USB mouse at all, but an "Unknown USB Device (Device Descriptor Request
Failed)" on port 4 of a VIA Labs hub (VID 2109), the USB-C dock's hub chain behind the laptop's
USB controller *(measured with Get-PnpDevice)*. The device didn't answer when Windows asked
what it is: a cable, port, hub or dock problem. FKeyIME handles keyboard text input only and
touches no device drivers.

## What to do

Re-plug the mouse into another dock port, unplug the dock for about 10 s, or plug the mouse
straight into the laptop.
