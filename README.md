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
- [Building and testing](BUILDING.md).
- [Licensing](LICENSING.md).

## On the FM-1

It starts on Melodee FM-1's Stage, its tracks empty, and says **MELODEE NOT HERE** to any control that asks Melodee
for something while none has it; the keys already play (notes on the selected track's channel). In Melodee, connect
to **Melodee Remote** (Bluetooth MIDI): once Melodee says HELLO (protocol version 3), its instruments, tracks (up to 32,
every one a part, shown four at a time as Melodee FM-1 shows them), steps and patterns fill the pages. MASTER is Melodee's master level.

Updates and recovery are fm1-base's: its installer (`build/fm1-base-platform/tools/fm1_install.py`), and
m-bootloader's recovery menu (hold PLAY while switching on) if it is installed. See BUILDING.md.

## Storage

The remote keeps fm1-base's records in the FM-1's flash: the panel's calibration at `0xA2000` (where Saadet keeps it,
so the two share it) and Bluetooth's address and pairing at `0xB0000`. Nothing of Melodee FM-1's (projects, sounds,
settings) is kept: the projects and sounds are Melodee's, and Melodee FM-1's menu settings last until power-off.

## Status

Melodee FM-1's UI and controls built as their own unit (`app/unit/`, from `fm1/`), 32 tracks: Bluetooth build 457 KB,
62 of 96 KB of RAM, 292 of 344 KB of the pool. Installed on an FM-1 over USB, which answers HELLO and PING; the pages and the
link tested in fm1-base's host simulator (`app/tests/`). Melodee iOS speaks version 2 (melodee-ios `0639f4b`); its
version 3 is being written.
