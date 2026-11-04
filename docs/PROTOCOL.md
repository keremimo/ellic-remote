# The Ellic Remote protocol, version 5

How Ellic Studio and an FM-1 running Ellic Remote talk. The remote is a pure remote: it runs Melodee FM-1's own pages
and controls (its navigation, lists, popups, sheets, knobs, LEDs, the keys' layouts), and nothing else. It keeps no
music, plays none, records none and stores none. What its pages show is a **mirror** of Ellic Studio's project, in Ellic Studio
FM-1's shapes, which Ellic Studio fills; what its controls do comes back to Ellic Studio as **changes** (a value turned, a step
written, a track selected, a sound chosen) and **requests** (play, record, a pattern copied, a track added, undo, save).
The keys play notes as MIDI. The two show the same place: the remote's page moves Ellic Studio's screen and Ellic Studio's
screen moves the remote's page (**PAGE**), whichever was touched last.

Version 5 (2026-11-04): the selected track's chain (**CHAIN**: its devices in order, each its kind, its switch, its
name and its first reading; the remote's CHAIN page, the first of FX, and requests to switch, move, remove, add, replace
and choose a device: **REQUEST DEVICE ON .. DEVICE CHOOSE**), what a chain can take told, not baked (**CATALOGUE**: note
effects, instruments, effects and plug-ins by name, so a new effect or a plug-in needs no firmware); a sampler's bank
as a grid (PAGES view PADS, its bank a told value, OCT- / OCT+ stepping it), a take's source and its meter (**METER**),
Ellic Studio's samples in their folders to load onto a pad (**SAMPLES**, PAGES view LIST, **REQUEST LOAD SAMPLE**, the row
scrolled onto auditioned on the pad). A LOOPER on a chain is a stub for now (one mirror cmd and requests 30..39 reserved,
nothing assigned: it is being reworked in Ellic Studio). A HELLO of another version claims nothing: the remote answers
with its own. `app/unit/link.c` is the firmware's side of everything below (`remote_chain.c` the chain and the lists);
`app/tests/link.txt` plays Ellic Studio's.

Version 4 (2026-11-01): the selected track's sound pages are Ellic Studio's (**PAGES**, **DESC**: each instrument's own
EDIT, ENV and LFO pages, a sampler's pad and sample, an audio track's clip and warp, the track's effects on FX), with a
waveform and its marks for a sample or a clip (**WAVE**, **MARKS**) and actions (**REQUEST ACTION**); the tracks'
names (**TRACKS**); the screens followed both ways (**PAGE**); the remote's preferences and Ellic Studio's sound settings
kept by Ellic Studio (**PREFS**); Ellic Studio's word on the remote's screen (**TOAST**). Gone: the projects (PROJECTS, LOAD,
NEW; SAVE saves the project open), Melodee FM-1's own ENV, LFO and FX pages and its eight engine parameters
(`P_E0..P_E7`).

The same bytes work over Bluetooth MIDI (the remote is a peripheral advertised as **Ellic**, GAP name
**Ellic Remote**) and USB MIDI (USB product **Ellic Remote FM-1**; CoreMIDI names its port "FM-1").

## Framing

Every message is SysEx: `F0 7D 4D 52 cmd body F7` (7D: non-commercial; "MR"). In bodies:

- **v14**: a value as two 7-bit bytes, low first, offset by 8192: `-8192..8191` (0 is `00 40`, 1 is `01 40`, -1 is
  `7F 3F`).
- **text**: ASCII 0x20..0x7E, ended by `00`.
- Parameter numbers below 4096 are Melodee FM-1's: per track `P_*`, global `G_*` (`fm1/src/core.h`); their ranges and
  formats are its descriptors (`fm1/src/params.c` `TP[]` and `GP[]`). At this firmware: `P_COUNT` 124, `G_COUNT` 45,
  `P_ROOT` 25, `P_SCALE` 26, `P_QUANT` 27, `G_BPM` 0. **4096 + k** is the selected track's told value k (0..119, below).
- Tracks are numbered 0..tmax-1, every one a part. The remote's pages show them four at a time, the selected track's
  four (Stage, MIXER, PATTERNS, SONG, GLO's layer: Melodee FM-1's groups), numbered 1..32.
- Messages keep their bodies to about 150 bytes (Ellic Studio splits longer ones: PAGES, SOUNDS, WAVE, CATALOGUE,
  SAMPLES).

## Who is there

1. When a link comes up (a Bluetooth central subscribes; USB at power-on and at each new host), the remote sends
   **HELLO**, and again 1, 2, 4, 8 and 16 s later while nobody has it.
2. Ellic Studio sends **HELLO** (version 5) to a port it suspects: that claims the remote, which answers **STATE**. Ellic Studio
   then tells it everything (the mirror below) and listens for its changes and requests.
3. Ellic Studio sends **PING** every 2 s; the remote answers **STATE**. A copy of Ellic Studio that has PINGed and then says nothing
   for 7 s lets the remote go, as **BYE** does, and as the claiming link going does.
4. The remote's **generation** (in HELLO and STATE) changes whenever it lost what Ellic Studio had told it (power-on,
   a link that went, another copy of Ellic Studio taking it, a frame of Ellic Studio's cut short on the way: bytes lost). A copy of Ellic Studio that sees a generation it did not last tell, or a state
   that is not "yours", tells it everything again.

Only the copy of Ellic Studio that has the remote is listened to; another port's messages are ignored except HELLO (which takes it
over) and PING (answered with STATE: "taken").

| cmd | Name | Who | Body |
| --- | --- | --- | --- |
| 01 | HELLO | both | version (5); from the remote also: generation (2 x 7 bits, low first), state, tmax (the tracks it holds: 32) |
| 02 | BYE | Ellic Studio | — |
| 03 | STATE | remote | generation (2 x 7 bits), state: 0 free, 1 yours, 2 taken; why the generation last changed: 0 power-on, 1 a link went or went silent, 2 taken by another port, 3 bytes lost (a frame of Ellic Studio's cut short on the way); tmax |
| 04 | PING | Ellic Studio | — |
| 05 | PAGE | both | screen, track, family, page (127 none), title (below) |
| 06 | PREFS | both | (id, value v14) repeated (below) |
| 07 | TOAST | Ellic Studio | a line, a second line (text each): shown on the remote a moment, as its own messages are |

## The mirror: Ellic Studio to the remote

| cmd | Name | Body |
| --- | --- | --- |
| 40 | INSTRUMENT | n (0..11, Ellic Studio's order), sounds v14, name |
| 41 | DESC | k (0..119), format (`F_*`: 0 INT, 1 PCT, 2 BIPCT, 3 TIME, 4 LFOHZ, 5 CUTOFF, 6 DB, 7 SEMI, 8 ENUM, 9 BPM, 10 NOTE, 11 ONOFF, 12 OCT, 13 STEPS; 14 ACTION), min v14, max v14, default v14, names count, label, unit, then the value names (ENUM: one a value, at most 24): the selected track's told value k |
| 42 | SOUNDS | n, first v14, count, then count names (at most 128 an instrument) |
| 43 | INSTRUMENTS | count: the instruments told so far are the list (PRESETS browses them; a track's instrument is one of them) |
| 44 | TRACKS | count (1..tmax: the project's first tracks, in order), the selected one, first, then the tracks from first on as fit (each: instrument n (127 none), sound v14, name): Ellic Studio splits them into messages of ~150 bytes |
| 45 | PARAM | (track, id v14, value v14) repeated: a track's values (`P_*`; 4096 + k the selected track's told value k) |
| 46 | GLOBAL | (`G_*`, value v14) repeated: the project's values (tempo, swing, ..) |
| 47 | STEPS | track, first step (0..63), count (at most 16), then count steps, 13 bytes each (below): the playing pattern's |
| 48 | PATTERNS | track, the pattern playing (0..15), the one queued (127 none), the patterns that hold something (16 bits as 3 x 7, low first), then up to 16 x (length in steps, division `N_DIV`) |
| 49 | PLAY | playing (0/1), armed tracks (5 bytes: 35 bits, 7 a byte, low first), tracks waiting to record (5 bytes), counting in (0/1), the song playing (0/1), the song's row, then for each track the step playing |
| 4A | SONG | rows (0..64), then rows x (pattern, repeats) |
| 4C | SCOPE | samples, 7 bits each (64 the middle): HOME's oscilloscope, ~120 a message, ~6 a second while a page shows it |
| 4D | PAGES | track, keys (0 the layout, 1 pads), the pads' first note, first, count, total, then count x (family, view, k0, k1, k2, k3 (127 none), title) |
| 4E | WAVE | track, gen v14, first column, count, then count x (low, high: 0..127, 64 the middle): the sample or clip shown, 224 columns |
| 4F | MARKS | track, gen v14, count, then count x (kind, position v14 (0..8191 over the wave), value v14) |
| 51 | CHAIN | track, the device chosen (127 none), count, then count x (kind: 0 note effect, 1 MIDI effect, 2 instrument, 3 effect, 4 LOOPER, 5 plug-in; on (0/1); name (at most 12 letters); reading (text, at most 8: its first value as it reads, an effect's MIX, ARP's RATE)): the track's devices in order |
| 52 | CATALOGUE | group (0 note effects, 1 instruments, 2 effects, 3 plug-ins), first v14, count, then count names (at most 128 a group): what a chain can take, in Ellic Studio's groups |
| 53 | SAMPLES | folder (0.., Ellic Studio's order), its name (text: on the message with first 0, empty after), first v14, count, then count names (at most 128 a folder): Ellic Studio's samples (its factory kinds, Recordings, Loops) |
| 54 | METER | track, the source's level (0..127): what a take would record, ~6 a second while a page shows it |
| 55 | (reserved) | the LOOPER's mirror, to come (below) |

A **step** is Melodee FM-1's `step_t` (`fm1/src/core.h`): 11 bytes (note 0..3, notes in use, time 0 NOTE 1 TIE 2 REST,
flags (1 accent, 2 slide, ..), velocity, hits (bit a drum lane), accents, chance) sent as 13: two bytes of high bits
(byte 0 bit i: byte i's bit 7 for i < 7; byte 1 for the rest), then each byte's low 7 bits.

Ellic Studio tells the instruments first (INSTRUMENT, SOUNDS, then INSTRUMENTS), CATALOGUE and SAMPLES, then TRACKS, the
selected track's CHAIN, PAGES, DESCs and told values (PARAM 4096 + k), every track's PARAM, GLOBAL, each track's STEPS
and PATTERNS, SONG, PREFS and PLAY. Afterwards it tells what changes as it changes (an edit on the iPad: PARAM, STEPS;
the music moving: PLAY, at least at each step; another track selected: its CHAIN, PAGES, DESCs and told values; a
device switched, moved, removed, added, replaced or chosen, on the iPad or by the remote: CHAIN, and the track's PAGES,
DESCs and told values when its pages changed with it).

### The selected track's pages

The sound's pages are Ellic Studio's, for the selected track as it is: its instrument's own (a synth's oscillators and
filter on EDIT, its envelopes on ENV, its LFOs and modulation on LFO), a sampler's (ENV the selected pad: its pitch,
level, pan, decay, filter, how it plays; LFO its sample: start, end, loop, trim and chop), an audio track's (ENV the
selected clip: start, end, gain, fades; LFO its warp: tempo, stretch, pitch), and the track's chain (FX: CHAIN, the
remote's own, then each device's pages in the chain's order), and every track's MIDI IN on EDIT (SOURCE: Any and Ellic
Studio's sources by name; CHANNEL: Any, 1..16). **PAGES** lists them in order:

- **family**: 0 HOME (Stage's four knobs: one page), 1 EDIT, 2 ENV, 3 LFO, 4 FX. A button of the family steps through
  its pages; EDIT's last page is PRESETS (the sound browser, the remote's own); FX's first is CHAIN (the chain, the
  remote's own: PAGE says it as family 4, page 127), and a device's pages carry the chain as chips in their footer, the
  device's own filled.
- **view**: 0 the cards and the scope, 1 the wave (WAVE and MARKS) under the cards, 2 an envelope (k0..k3 as attack,
  decay, sustain, release, drawn as a curve), 3 PADS (a sampler's bank as a 4 x 4 grid under the cards, the pad chosen
  ringed), 4 LIST (a list under the cards, scrolled with PRESETS as the sound browser is: LOAD's samples, folder by
  folder from SAMPLES, the row scrolled onto a told value).
- **k0..k3**: the told values the page's knobs turn (127: none); **title** at most 12 letters.

Each told value k has its **DESC** (its range, how it reads; ACTION: a column that OCT+ does, `REQUEST ACTION`). A
value belongs to the track as Ellic Studio maps it: a sampler's pad values to the pad selected, a clip's to the clip shown;
when that changes Ellic Studio tells the values again. Up to 32 pages, 120 values.

Some told values are the remote's to know by name, as ACTION is; they need no message of their own:

| Told value | Where | How it reads |
| --- | --- | --- |
| BANK | a sampler's PADS page | ENUM A, B, C, D: the bank on the keys and the grid (OCT- / OCT+ step it in pads mode, in place of the octave) |
| SOURCE | a sampler's TAKE page | ENUM IN 1, IN 2, IN 1+2, MIX, THIS TRACK: what a take records; METER is its level |
| ROW | a sampler's LOAD page | INT: the row scrolled onto, folder x 128 + index into SAMPLES. While LOAD shows, the pad's key (a note on, as ever) plays that row's sample on the pad, auditioned by Ellic Studio; the pad's own sound stays until LOAD SAMPLE |

**keys**: 0 the keys play the selected track's layout (below); 1 pads: the 16 white keys, left to right, play the
pads' first note .. + 15 (the black keys nothing), as Melodee FM-1's kits do; Ellic Studio chooses a pad as it is struck.

### The wave and its marks

A sample's or a clip's sound (view 1) as 224 columns of peaks (WAVE: low and high, 64 the middle) and its marks
(MARKS: each a kind and a place over the whole wave, 0..8191), told again when they change; **gen** changes with the
sound shown (marks with another gen than the wave's wait for it). Kinds (bit 6 set: the mark selected):

| kind | Mark | value |
| --- | --- | --- |
| 1 | START (what plays starts) | — |
| 2 | END | — |
| 3 | LOOP start | — |
| 4 | LOOP end | — |
| 5 | CUT (a slice's start, a chop) | its pad (0..63) |
| 6 | WARP marker | its beat x 16 |
| 7 | GAIN point | its gain in dB x 10 |
| 8 | FADE IN's end | — |
| 9 | FADE OUT's start | — |
| 10 | PLAYING (where it plays now) | — |

### The chain

**CHAIN** is the selected track's devices as Ellic Studio has them, in order: each its kind, whether it is on, its name
and its first value as it reads (text, the remote shows it as is). The device chosen is Ellic Studio's chosen device: its
SOUND knobs follow it, and the iPad's Sound screen shows it (PAGE). Ellic Studio tells CHAIN again whenever the chain
changes, by its own hand or the remote's, and the track's PAGES, DESCs and told values with it when the pages changed
(a device added, removed or replaced). **CATALOGUE** is what a chain can take, in Ellic Studio's groups (note effects,
instruments, effects, plug-ins), told once on claim: the remote's Add and Replace lists are its names, and a device
asked for is a group and an index into it.

### LOOPER (to come)

A LOOPER on a chain (CHAIN kind 4) will have a mirror of its loops' state (each loop: empty, waiting, recording,
playing, overdubbing, stopped, its bars and its place around, told each step while a page shows it, as PLAY is),
requests for its footswitches (the remote's REC on its pages, its loops' keys on PATTERNS) and for sending a loop
elsewhere (a pad, a clip, the song, a new track), and its pages as its device's on FX. The looper is being reworked
in Ellic Studio, so none of it is assigned yet: cmd 55 and requests 30..39 are reserved for it, their bodies and the
pages' content to come.

## The remote to Ellic Studio

### Changes

The remote sends what its controls changed in place, against the mirror as Ellic Studio last told it, in the same messages:
**PARAM** (told values too), **GLOBAL**, **STEPS** and **TRACKS** (the track selected, with no tracks after it when only
that changed; a sound chosen in PRESETS: that track alone, its instrument and sound, its name as told: the remote names
nothing; Ellic Studio then loads the sound and tells the track's values). Ellic Studio applies them to its project; what it tells back the remote takes as the new mirror. At most 24
messages a frame (~60 frames a second).

### Requests

`50 REQUEST`: what, a v14, b v14, c v14, then an empty text (`00`).

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
| 10 | ADD a track (the New track sheet) | the place + the MIDI source x 32 (its place in the selected track's MIDI IN SOURCE as told: 0 Any), what it plays (an instrument n; 126 audio; 127 nothing yet), the sound + the channel x 128 (0 Any, 1..16) |
| 11 | REMOVE a track | track |
| 12 | UNDO | 0 undo, 1 redo |
| 13 | ERASE (EDIT held while recording) | track |
| 14 | REMOVE a recorded note | track, its index |
| 15 | CLEAR the recorded notes | track |
| 17 | SAVE: Ellic Studio saves the project open (SAVE pressed); TOAST says how it went | — |
| 20 | ACTION: a told ACTION value done (OCT+ on its column) | k, track |
| 21 | DEVICE ON: switch a device | track, device (its index in CHAIN), on (0/1) |
| 22 | DEVICE MOVE: move a device along the chain | track, device, +1 or -1 (v14); Ellic Studio keeps a note effect among its own, an instrument and the effects move together |
| 23 | DEVICE REMOVE | track, device: the remote has asked first (as it asks before removing a track); UNDO still has it |
| 24 | DEVICE ADD | track, after device (127: at the end), group x 128 + index into CATALOGUE; the new device is chosen, CHAIN and the track's PAGES told again |
| 25 | DEVICE REPLACE (an instrument's Change) | track, device, group x 128 + index into CATALOGUE |
| 26 | DEVICE CHOOSE: the row picked | track, device: Ellic Studio's chosen device, its knobs and its Sound screen follow |
| 27 | LOAD SAMPLE (OCT+ on LOAD's row) | track, pad, folder x 128 + index into SAMPLES |
| 30..39 | (reserved) | the LOOPER's: its footswitches, a loop sent elsewhere (above) |

### The chain and the pads on the remote

FX from anywhere lands on CHAIN: the track's devices a row each, as the old FX rack was a row a slot, the chosen one
tinted, the last row **+ Add**. FX again steps into the chosen device's pages, on through the chain's devices, then
back to CHAIN. On CHAIN, SELECT or K1 picks a row (`DEVICE CHOOSE`), K2 switches it (`DEVICE ON`), K3 moves it a detent
(`DEVICE MOVE`), K4 turns the chosen device's first knob as its first page lists it (PARAM 4096 + k; the row's reading
shows it as Ellic Studio tells it back). OCT+ opens the row's pages (PAGE), OCT- goes back; OCT+ held opens the row's
sheet, like the New track sheet: **Replace…** (CATALOGUE's names, `DEVICE REPLACE`), **Remove** (the remote asks first
in its own dialog, the device named and what it is, OCT+ Yes, OCT- No, then `DEVICE REMOVE`; the toast says it is gone)
and **Add after…**; + Add and Add after… list CATALOGUE in its groups, scrolled with PRESETS, and OCT+ sends
`DEVICE ADD`.

A sampler in pads mode (PAGES keys 1) has its bank on the white keys as before; OCT- / OCT+ step the bank (the told
value BANK) in place of the octave, and the PADS page shows it as a grid. On LOAD, PRESETS scrolls the rows (the told
value ROW), the pad's key plays the row's sample on the pad (Ellic Studio auditions it), and OCT+ loads it (`LOAD
SAMPLE`). The LOOPER's controls (REC as its footswitch, its loops' keys on PATTERNS) are the stub's, to come.

### PAGE: one screen

Whenever the remote's page changes by its own controls it sends **PAGE**; whenever Ellic Studio's screen changes by the
iPad's it sends **PAGE** too, and the other side goes there (the remote not while its menu or a dialog is open). Neither
answers a PAGE with one.

| screen | The remote | Ellic Studio | family, page |
| --- | --- | --- | --- |
| 0 | HOME (Stage, the chord view) | Stage | — |
| 1 | a sound page: EDIT, ENV, LFO, FX, PRESETS, CHAIN | Sound, at the told page's section (CHAIN: the chain; a device's page: its panel) | family 1..4 (5 PRESETS), the page's index in PAGES' set; family 4, page 127: CHAIN. A LOOPER's pages are its device's, on FX |
| 2 | SCL: SCALES, SCL, CHORD, MPC | Sound, the key | — |
| 3 | ARP | Sound, the arpeggiator | — |
| 4 | SEQ: NOTES, PATTERN | Pattern | — |
| 5 | SEQ: PATTERNS | Stage | — |
| 6 | SEQ: SONG | Song | — |
| 7 | GLO: MIXER | Mix | — |
| 8 | SEQ: TEMPO | (stays) | — |
| 9 | the menu | (stays) | — |

The title is the page's (for Ellic Studio's section on Sound).

### PREFS: what Ellic Studio keeps

The remote keeps nothing: its menu's settings are Ellic Studio's to keep. Ellic Studio tells them on claim (PREFS); the remote
tells each one changed in its menu. Ids 0..15 are the remote's own, 32.. Ellic Studio's settings the menu shows.

| id | Setting | values |
| --- | --- | --- |
| 0 | COLOR | the palette (0..) |
| 1 | FONT | 0 RUBIK, 1 PIXEL |
| 2 | HOLD | 0..3 (0.3 .. 0.6 s) |
| 3 | LIGHTS | 0 OFF, 1 LOW, 2 MID, 3 HIGH, 4 FULL |
| 4 | PICKER POPUPS | 0 off, 1 on |
| 5 | KNOB ACCEL | 0 on, 1 off |
| 6 | HOME SELECT | Melodee FM-1's `HS_*` |
| 7 | GLO SELECT | the same |
| 8 | UNUSED TRACKS | 0 HIDE, 1 SHOW |
| 9 | SCREEN OFF | 0 NEVER, 1 5 MIN, 2 15 MIN, 3 30 MIN, 4 60 MIN |
| 32 | AUDIO CLICK (Ellic Studio's metronome) | 0 OFF, 1 while recording, 2 ON |
| 33 | CLICK LEVEL | 0..100 % |
| 34 | COUNT-IN | 0 OFF, 1 one bar, 2 two bars |
| 35 | NOTE PREVIEW (a step chosen sounds) | 0 off, 1 on |
| 36 | CHORD ENTRY (a held chord's notes add on to a step) | 0 HOLD, 1 ADD |

### The keys and MASTER

The keys play the selected track as Melodee FM-1's do: its layout (`QNT` OFF, SNAP, WHITE, ALL, MPC and the scale,
`fm1/src/excerpt_seq.c` `kb_map`), a chord key's chord (`CHRD`), the octave, or its pads (PAGES keys 1); the drum
grid's lane keys their lanes' notes; a layer's keys (GLO, SCL held) the layer's. Each note goes out as **note on / off
on the track's channel** (track k: channel (k mod 15) + 1; 16 is the remote's own, for MASTER), velocity 100. Ellic Studio
takes a note on channel c as the track of those (c-1, c-1+15, ..) nearest the selected one (the keys only play the
selected track); a release goes where its press went. Ellic Studio plays them on that track (and its arpeggiator, its
recording, as it would any keyboard's): the remote does not record or arpeggiate them.

MASTER goes out as CC 7 (high 7 bits) and CC 39 (low) on channel 16: Ellic Studio's master level.

## Example

The test `app/tests/link.txt` (the bytes as sent, F0 .. F7):

```
F0 7D 4D 52 01 05 F7                                   HELLO, version 5: the remote is claimed (STATE back)
F0 7D 4D 52 40 00 02 40 42 52 4F 46 41 54 00 F7        INSTRUMENT 0: 2 sounds, BROFAT
F0 7D 4D 52 42 00 00 40 02 42 72 6F 20 42 61 73 73 00 57 6F 62 62 6C 65 00 F7
                                                       SOUNDS 0: "Bro Bass", "Wobble"
F0 7D 4D 52 43 01 F7                                   INSTRUMENTS: 1
F0 7D 4D 52 44 02 00 00 00 00 40 42 41 53 53 00 00 01 40 4C 45 41 44 00 F7
                                                       TRACKS: 2, track 0 selected, from 0: BASS (BROFAT's sound
                                                       0), LEAD (its sound 1)
F0 7D 4D 52 4D 00 00 24 00 02 02 00 00 00 01 02 03 53 54 41 47 45 00 02 00 00 01 02 03 41 4D 50 00 F7
                                                       PAGES: track 0, the keys' layout, 2 of 2: HOME's knobs
                                                       k0..k3 (STAGE), ENV k0..k3 (AMP)
F0 7D 4D 52 41 00 00 00 40 7F 40 40 40 00 44 52 49 56 45 00 00 F7
                                                       DESC 0: INT 0..127, default 64, DRIVE
F0 7D 4D 52 45 00 00 60 64 40 F7                       PARAM: track 0, told value 0 (4096) = 100
F0 7D 4D 52 46 00 62 40 F7                             GLOBAL: G_BPM = 98
F0 7D 4D 52 49 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 F7
                                                       PLAY: playing, nothing armed, track 0 on step 0
```

Then K1 turned on Stage sends `45 00 00 60 ..` (PARAM: track 0's told value 0), PLAY pressed sends `50 02 ..` (STOP),
SAVE sends `50 11 ..` (SAVE) and ENV sends `05 01 00 02 01 41 4D 50 00` (PAGE: a sound page, track 0, ENV, the
set's page 1: AMP).

## Not in version 5 (yet)

The LOOPER's messages (its loops' state, its footswitches, a loop sent elsewhere, its pages' content: the stub above),
which pads hold a sound (PADS draws every pad alike and the keys' LEDs light as before), motion (parameter lanes), a
pattern's own sound, the song's sections as Ellic Studio has them (the remote's SONG is rows of patterns), a Song clip's
audio (the pages edit an audio track's Stage clip), audio to notes, the preset favourites and the scales' favourites
(the remote's, until power-off).
