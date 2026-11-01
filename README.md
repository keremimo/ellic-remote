# Melodee Remote

Firmware that turns an M-VAVE FM-1 into a remote for Melodee on iPhone and iPad, over Bluetooth MIDI (or a USB
cable). It runs Melodee FM-1's own pages and controls (Melodee's FM-1 firmware: Stage, the chord view, SCL and SCALES,
CHORD, the sequencer's NOTES, PATTERNS and SONG, TEMPO, ARP, the FX racks, MIXER, GLO's layers, SAVE, the menu), with
its navigation, lists, popups, knobs and lights, over a mirror of Melodee's project that Melodee fills. It is a pure
remote: it keeps no music, plays none, records none and stores none. What its controls change and ask (a value, a
step, play, record, a pattern copied, a track added, undo) goes to Melodee; its keys play Melodee's selected track as
MIDI, laid out as Melodee FM-1 lays them (QNT, the scale, chord keys).

It is built on the same platform as [Saadet](https://github.com/keremimo/saadet): fm1-base and felucca-bsp at
Saadet's pinned revisions, with Saadet's fixes to the Bluetooth peripheral link, plus SysEx over Bluetooth MIDI in
both directions, which the link with Melodee needs and the base does not carry.

- [The protocol](docs/PROTOCOL.md): what Melodee sends and what it gets back.
- [The web installer](web/install/): installs it over USB from Chrome or Edge.
- [Building and testing](BUILDING.md).
- [Licensing](LICENSING.md).

## On the FM-1

It starts on Melodee FM-1's Stage, its tracks empty, and says **MELODEE NOT HERE** to any control that asks Melodee
for something while none has it; the keys already play (notes on the selected track's channel). In Melodee, connect
to **Melodee Remote** (Bluetooth MIDI): once Melodee says HELLO (protocol version 4), its instruments, tracks (up to 32,
every one a part, shown four at a time as Melodee FM-1 shows them), the selected track's sound pages, steps and
patterns fill the pages. MASTER is Melodee's master level.

It installs from the browser with the web installer (`web/install/`, Saadet's, in Chrome or Edge over a USB cable),
which also finishes an install that stopped, puts back M-VAVE's firmware, adds m-bootloader and reads a crash
report; or from a computer with fm1-base's installer (`build/fm1-base-platform/tools/fm1_install.py`). Recovery is
m-bootloader's menu (hold PLAY while switching on) if it is installed. See BUILDING.md.

## Storage

The remote keeps fm1-base's records in the FM-1's flash: the panel's calibration at `0xA2000` (where Saadet keeps it,
so the two share it) and Bluetooth's address and pairing at `0xB0000`. Nothing of Melodee FM-1's (projects, sounds,
settings) is kept: the projects and sounds are Melodee's, and so are the menu's settings, which Melodee keeps and tells
the remote when it claims it (PREFS in the protocol).

## Status

Melodee FM-1's UI and controls built as their own unit (`app/unit/`, from `fm1/`, forked and cut down to a remote),
32 tracks, protocol version 4 (the selected track's sound pages Melodee's, waves for samples and clips, one screen with
Melodee's, preferences kept by Melodee): Bluetooth build 427 KB, 62 of 96 KB of RAM, 298 of 344 KB of the pool.
Installed on an FM-1 over USB, which answers PING; the pages, the link and the lights tested in fm1-base's host
simulator (`app/tests/`), and Melodee's own messages read there (melodee-ios `scripts/remote-screens.sh`). Melodee iOS
speaks version 4 (melodee-ios `b9273c8`).
