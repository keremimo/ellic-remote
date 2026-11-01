# The Melodee Remote protocol, version 3

How Melodee and an FM-1 running Melodee Remote talk. The remote is a pure remote: it runs Melodee FM-1's own pages
and controls (its navigation, lists, popups, sheets, knobs, LEDs, the keys' layouts), and nothing else. It keeps no
music, plays none, records none and stores none. What its pages show is a **mirror** of Melodee's project, in Melodee
FM-1's shapes, which Melodee fills; what its controls do comes back to Melodee as **changes** (a value turned, a step
written, a track selected, a sound chosen) and **requests** (play, record, a pattern copied, a track added, undo).
The keys play notes as MIDI.

Version 3 (2026-11-01) makes every track a part and lifts the 8-track limit: the remote holds up to `tmax` tracks (32
at this firmware, in its HELLO and STATE), each with an instrument, its sequencer and 16 patterns; Melodee FM-1's
MIDI-only places 5..8 are gone. Versions 1 (Melodee drawing a generic screen) and 2 (8 tracks, 5..8 MIDI-only) are
retired: an older HELLO gets the remote's HELLO back (version 3) and nothing else. `app/unit/link.c` is the firmware's side of everything below; `app/tests/link.txt` plays Melodee's.

The same bytes work over Bluetooth MIDI (the remote is a peripheral advertised as **Melodee**, GAP name
**Melodee Remote**) and USB MIDI (USB product **Melodee Remote FM-1**; CoreMIDI names its port "FM-1").

## Framing

Every message is SysEx: `F0 7D 4D 52 cmd body F7` (7D: non-commercial; "MR"). In bodies:

- **v14**: a value as two 7-bit bytes, low first, offset by 8192: `-8192..8191` (0 is `00 40`, 1 is `01 40`, -1 is
  `7F 3F`).
- **text**: ASCII 0x20..0x7E, ended by `00`.
- Parameter numbers are Melodee FM-1's: per track `P_*`, global `G_*` (`fm1/src/core.h`); their ranges and formats
  are its descriptors (`fm1/src/params.c` `TP[]` and `GP[]`). At this firmware: `P_COUNT` 124, `G_COUNT` 45, the
  engine parameters `P_E0..P_E7` are 116..123, `P_ROOT` 25, `P_SCALE` 26, `P_QUANT` 27, `G_BPM` 0.
- Tracks are numbered 0..tmax-1, every one a part. The remote's pages show them four at a time, the selected track's
  four (Stage, MIXER, PATTERNS, SONG, GLO's layer: Melodee FM-1's groups), numbered 1..32.

## Who is there

1. When a link comes up (a Bluetooth central subscribes; USB at power-on and at each new host), the remote sends
   **HELLO**, and again 1, 2, 4, 8 and 16 s later while nobody has it.
2. Melodee sends **HELLO** (version 3) to a port it suspects: that claims the remote, which answers **STATE**. Melodee
   then tells it everything (the mirror below) and listens for its changes and requests.
3. Melodee sends **PING** every 2 s; the remote answers **STATE**. A Melodee that has PINGed and then says nothing
   for 7 s lets the remote go, as **BYE** does, and as the claiming link going does.
4. The remote's **generation** (in HELLO and STATE) changes whenever it lost what Melodee had told it (power-on,
   a link that went, another Melodee taking it). A Melodee that sees a generation it did not last tell, or a state
   that is not "yours", tells it everything again.

Only the Melodee that has the remote is listened to; another port's messages are ignored except HELLO (which takes it
over) and PING (answered with STATE: "taken").

| cmd | Name | Who | Body |
| --- | --- | --- | --- |
| 01 | HELLO | both | version (3); from the remote also: generation (2 x 7 bits, low first), state, tmax (the tracks it holds: 32) |
| 02 | BYE | Melodee | — |
| 03 | STATE | remote | generation (2 x 7 bits), state: 0 free, 1 yours, 2 taken; why the generation last changed: 0 power-on, 1 a link went or went silent, 2 taken by another port; tmax |
| 04 | PING | Melodee | — |

## The mirror: Melodee to the remote

| cmd | Name | Body |
| --- | --- | --- |
| 40 | INSTRUMENT | n (0..11, Melodee's order), sounds v14, HOME's four knobs (four `P_*` numbers, 127 none), name, EDIT 1's title, EDIT 2's title |
| 41 | PARAM_DESC | n, k (0..7: `P_E0 + k`), format (`F_*`: 0 INT, 1 PCT, 2 BIPCT, 3 TIME, 4 LFOHZ, 5 CUTOFF, 6 DB, 7 SEMI, 8 ENUM, 9 BPM, 10 NOTE, 11 ONOFF, 12 OCT, 13 STEPS), min v14, max v14, default v14, names count, label, unit, then the value names (ENUM: one a value, at most 24) |
| 42 | SOUNDS | n, first v14, count, then count names (at most 128 an instrument) |
| 43 | INSTRUMENTS | count: the instruments told so far are the list (PRESETS browses them; a track's instrument is one of them) |
| 44 | TRACKS | count (1..tmax: the project's first tracks, in order), the selected one, then count x (instrument n (127 none), sound v14) |
| 45 | PARAM | (track, `P_*` v14, value v14) repeated: a track's values |
| 46 | GLOBAL | (`G_*`, value v14) repeated: the project's values (tempo, swing, ..) |
| 47 | STEPS | track, first step (0..63), count (at most 16), then count steps, 13 bytes each (below): the playing pattern's |
| 48 | PATTERNS | track, the pattern playing (0..15), the one queued (127 none), the patterns that hold something (16 bits as 3 x 7, low first), then up to 16 x (length in steps, division `N_DIV`) |
| 49 | PLAY | playing (0/1), armed tracks (5 bytes: 35 bits, 7 a byte, low first), tracks waiting to record (5 bytes), counting in (0/1), the song playing (0/1), the song's row, then for each track the step playing |
| 4A | SONG | rows (0..64), then rows x (pattern, repeats) |
| 4B | PROJECTS | the current project's name, then up to 32 x (used 0/1, name): SAVE > PROJECT's slots |
| 4C | SCOPE | samples, 7 bits each (64 the middle): HOME's oscilloscope, ~120 a message, ~6 a second while a page shows it |

A **step** is Melodee FM-1's `step_t` (`fm1/src/core.h`): 11 bytes (note 0..3, notes in use, time 0 NOTE 1 TIE 2 REST,
flags (1 accent, 2 slide, ..), velocity, hits (bit a drum lane), accents, chance) sent as 13: two bytes of high bits
(byte 0 bit i: byte i's bit 7 for i < 7; byte 1 for the rest), then each byte's low 7 bits.

Melodee tells the instruments first (INSTRUMENT, its PARAM_DESC x8, SOUNDS, then INSTRUMENTS), then TRACKS, every
track's PARAM, GLOBAL, each track's STEPS and PATTERNS, SONG, PROJECTS and PLAY. Afterwards it tells what changes as
it changes (an edit on the iPad: PARAM, STEPS; the music moving: PLAY, at least at each step).

## The remote to Melodee

### Changes

The remote sends what its controls changed in place, against the mirror as Melodee last told it, in the same messages:
**PARAM**, **GLOBAL**, **STEPS** and **TRACKS** (the track selected; a sound chosen in PRESETS: the track's instrument
and sound; Melodee then loads the sound and tells the track's PARAMs). Melodee applies them to its project; what it
tells back the remote takes as the new mirror. At most 24 messages a frame (~60 frames a second).

### Requests

`50 REQUEST`: what, a v14, b v14, c v14, name (text: a save's name, else empty).

| what | Request | a, b, c |
| --- | --- | --- |
| 1 | PLAY | — |
| 2 | STOP | — |
| 3 | PLAY from the top | — |
| 4 | ARM: record on a track | track, on (0/1) |
| 5 | PATTERN: play (queue) a pattern | track, pattern |
| 6 | COPY a pattern | source track, source pattern, destination track x 256 + destination pattern |
| 7 | CLEAR a pattern | track, pattern |
| 8 | DOUBLE a pattern | track, pattern |
| 9 | HALVE a pattern | track, pattern |
| 10 | ADD a track | the place (0..7) |
| 11 | REMOVE a track | track |
| 12 | UNDO | 0 undo, 1 redo |
| 13 | ERASE (EDIT held while recording) | track |
| 14 | REMOVE a recorded note | track, its index |
| 15 | CLEAR the recorded notes | track |
| 17 | SAVE the project | slot (0..31; 127 the current one), name |
| 18 | LOAD a project | slot |
| 19 | NEW song | — |

### The keys and MASTER

The keys play the selected track as Melodee FM-1's do: its layout (`QNT` OFF, SNAP, WHITE, ALL, MPC and the scale,
`fm1/src/excerpt_seq.c` `kb_map`), a chord key's chord (`CHRD`), the octave; the drum grid's lane keys their lanes'
notes; a layer's keys (GLO, SCL held) the layer's. Each note goes out as **note on / off on the track's channel**
(track k: channel (k mod 15) + 1; 16 is the remote's own, for MASTER), velocity 100. Melodee takes a note on channel c
as the track of those (c-1, c-1+15, ..) nearest the selected one (the keys only play the selected track); a release
goes where its press went. Melodee plays them on that track (and its
arpeggiator, its recording, as it would any keyboard's): the remote does not record or arpeggiate them.

MASTER goes out as CC 7 (high 7 bits) and CC 39 (low) on channel 16: Melodee's master level.

## Example

The test `app/tests/link.txt` (the bytes as sent, F0 .. F7):

```
F0 7D 4D 52 01 03 F7                                   HELLO, version 3: the remote is claimed (STATE back)
F0 7D 4D 52 40 00 02 40 74 75 76 77 42 52 4F 46 41 54 00 4F 53 43 00 46 49 4C 54 45 52 00 F7
                                                       INSTRUMENT 0: 2 sounds, knobs P_E0..P_E3, BROFAT, OSC, FILTER
F0 7D 4D 52 41 00 00 00 00 40 7F 40 40 40 00 44 52 49 56 45 00 00 F7
                                                       PARAM_DESC 0/0: INT 0..127, default 64, DRIVE
F0 7D 4D 52 42 00 00 40 02 42 72 6F 20 42 61 73 73 00 57 6F 62 62 6C 65 00 F7
                                                       SOUNDS 0: "Bro Bass", "Wobble"
F0 7D 4D 52 43 01 F7                                   INSTRUMENTS: 1
F0 7D 4D 52 44 02 00 00 00 40 00 01 40 F7             TRACKS: 2, track 0 selected, BROFAT's sounds 0 and 1
F0 7D 4D 52 45 00 74 40 64 40 F7                       PARAM: track 0, P_E0 (116) = 100
F0 7D 4D 52 46 00 62 40 F7                             GLOBAL: G_BPM = 98
F0 7D 4D 52 47 00 00 01 00 00 3C 00 00 00 01 00 00 64 00 00 00 F7
                                                       STEPS: track 0, step 0: C4 (60), one note, NOTE, vel 100
F0 7D 4D 52 49 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 F7
                                                       PLAY: playing, nothing armed, track 0 on step 0
```

Then K1 turned on Stage sends `45 00 74 40 ..` (PARAM: track 0's DRIVE), PLAY pressed sends `50 02 ..` (STOP).

## Not in version 3 (yet)

Motion (parameter lanes), a pattern's own sound, effects as Melodee FM-1's FX rack shows them beyond the track's
`P_FX*` values, the menu's settings kept on the remote: the pages show none of these until the protocol carries them.
