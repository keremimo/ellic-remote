# Melodee Remote

Firmware that turns an M-VAVE FM-1 into a remote for Melodee on iPhone and iPad, over
Bluetooth MIDI (or a USB cable). Melodee draws the FM-1's screen and lights its keys and buttons; every key, button,
encoder and the MASTER knob go back to Melodee. The FM-1 makes no sound of its own.

It is built on the same platform as [Saadet](https://github.com/keremimo/saadet): fm1-base and felucca-bsp at
Saadet's pinned revisions, with Saadet's fixes to the Bluetooth peripheral link, plus SysEx over Bluetooth MIDI in
both directions, which the remote's screen needs and the base does not carry.

- [The protocol](docs/PROTOCOL.md): what Melodee sends and what it gets back.
- [Building and testing](BUILDING.md).
- [Licensing](LICENSING.md).

## On the FM-1

With no Melodee connected, the screen says **WAITING FOR MELODEE** and how the link stands. The keys already play
(notes on channel 1, with velocity and pressure), so the FM-1 works as a plain Bluetooth MIDI keyboard and controller
meanwhile: its buttons are notes 0..13 and its encoders relative CCs 16..22 on channel 16.

In Melodee, connect to **Melodee Remote** (Bluetooth MIDI). Once Melodee says HELLO, the screen and lights are
Melodee's.

The FM-1 keeps three things to itself:

| Control | What it does |
| --- | --- |
| OCT- / OCT+ | Shift the keys an octave (-3..+3); both together: back to 0. They still go to Melodee as buttons, and Melodee can take them over. |
| OCT- + OCT+ held 1.5 s | SETUP: K1 the keys' channel, K2 the velocity curve, K3 poly pressure on or off, K4 Bluetooth on or off, PRESETS the screen's theme. HOME (or the same hold) closes it and saves. |
| Bluetooth off | SETUP's K4, kept across power-offs. USB still works. |

Updates and recovery are fm1-base's: its installer (`build/fm1-base-platform/tools/fm1_install.py`), and
m-bootloader's recovery menu (hold PLAY while switching on) if it is installed. See BUILDING.md.

## Storage

The remote keeps three small records in the FM-1's flash: the panel's calibration at `0xA2000` (where Saadet keeps
it, so the two share it), Bluetooth's address and pairing at `0xB0000`, and its settings at `0xB2000` (both apart from
Saadet's). Installing the remote leaves Saadet's sounds and settings alone; those sectors were part of
Melodee FM-1's (the nightly firmware's) project area, so a project stored there before can be overwritten.

## Status

Built for the FM-1 (Bluetooth: 236 KB, 66 of 96 KB of RAM) and tested in fm1-base's host simulator, with the
Bluetooth SysEx code tested on its own against stand-ins for the radio stack. **Not yet run on an FM-1**, and no
Melodee speaks the protocol yet: Melodee's side is the next step.
