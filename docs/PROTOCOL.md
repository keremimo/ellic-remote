# The Melodee Remote protocol, versions 1 and 2

How Melodee and an FM-1 running Melodee Remote talk. Melodee draws the remote's screen and lights its panel; the
remote sends every key, button, encoder and MASTER move back. The remote has no idea what a track or a sound is:
Melodee decides what each control does on each screen, so Melodee's features reach the remote without new firmware.

The same bytes work over Bluetooth MIDI (the remote is a peripheral advertised as **Melodee**, GAP name
**Melodee Remote**) and USB MIDI (USB product **Melodee Remote FM-1**). `app/mel_proto.h` is the firmware's copy of
every number below.

Version 1 (below, first) gives Melodee a generic screen: a header, a list, text or cells, K1..K4. Version 2
([below](#version-2)) mirrors Melodee FM-1's own pages: Melodee says which page shows and sends what the page draws
(tracks, steps, cards, rows, pictures), and the remote draws it with Melodee FM-1's drawing code. Everything in version 1
still works in version 2.

## Who is there

1. When a link comes up (a Bluetooth central subscribes, or a USB host configures the port), the remote sends its
   **HELLO** on that link, unasked.
2. Melodee sends **HELLO** on any port it suspects (a name with "Melodee" in it, or every new port). The remote
   answers with its own HELLO, then its MASTER position (CC 7 and 39).
3. The first protocol message from a port **claims** the remote: its screen is blank (as after CLEAR) and its MIDI
   now goes to that port alone. While one port has it, the other port's messages are ignored, except HELLO, which
   takes it over.
4. **BYE**, or the claiming link going (Bluetooth disconnected; USB with no host for a second), lets it go: the
   remote shows its own screen again, its keys play on its own channel, its MIDI goes to every port.

Melodee should send BYE when it stops driving the remote (the app backgrounded without audio, the remote turned off
in its settings) and HELLO plus a full screen when it comes back.

## The remote to Melodee

Plain MIDI, so it costs nothing to parse and goes out the instant a key moves.

| What | Message |
| --- | --- |
| A key, as a note (the default) | Note on / off on the **keys channel** (KEYS; the remote's own setting while no Melodee has it, default 1). Key 0 is F3 (53), plus 12 x octave. Velocity 1..127 from the keys' sensing. |
| A held key's pressure | Poly pressure on the keys channel, 0..127 (off with KEYS flag 4, or in the remote's SETUP). |
| A key, as a button (KEYS flag 1) | Note on / off on **channel 16**, note 32 + key (32..58), with its velocity. |
| A button | Note on (velocity 127) / note off on channel 16, note = button: 0 FX, 1 SEL, 2 ENV, 3 LFO, 4 EDIT, 5 GLO, 6 HOME, 7 SAVE, 8 ARP, 9 SEQ, 10 PLAY, 11 REC, 12 OCT-, 13 OCT+. |
| An encoder | CC 16 + encoder on channel 16: 16 SELECT, 17 ALGORITHM, 18 PRESETS, 19 K1, 20 K2, 21 K3, 22 K4. Relative, 64 + detents (65 one step clockwise, 63 one back; 1..127). |
| MASTER | CC 7 then CC 39 on channel 16: 14 bits, 0..16383. Sent when it moves by 64 or more, and at its ends. |

OCT- and OCT+ also move the keys' octave on the remote itself (-3..+3; both together: 0), unless KEYS flag 2 gives
them to Melodee. Holding both for 1.5 s opens the remote's SETUP page: its buttons, encoders and MASTER are the
remote's own until it closes (the keys still play, as notes, so their velocity can be tried).

## Melodee to the remote

SysEx: `F0 7D 4D 52 <command> <body> F7`. `7D` is the non-commercial id, `4D 52` "MR". Every body byte is seven
bits. A message that is malformed (cut short, a byte with its top bit set, an index out of range) changes nothing.

- **text**: a length byte, then that many printable ASCII characters (0x20..0x7E; others show as `?`). Longer than
  its field is cut. The screen's fonts have lower case.
- **colour**: three bytes, red, green, blue, 0..127 each. 0 0 0 means "the theme's colour".
- **bits**: a number seven bits a byte, the lowest first.

| Command | Body | What it does |
| --- | --- | --- |
| `01` HELLO | (version, optional) | The remote answers with its HELLO. |
| `02` BYE | | Lets the remote go (above). |
| `10` TITLE | colour, text (23) | The header's name. Its colour is the screen's accent: the rule under the header, selection marks, bars. |
| `11` INFO | transport, text (15) | The header's right side (tempo, position). Transport bits: 1 playing (a green triangle), 2 recording (a red dot), 4 count-in (a grey dot). |
| `12` BODY | kind | Which body shows: 0 text, 1 list, 2 cells. Each keeps its contents while another shows. |
| `13` ROW | index (0..5), flags, colour, text (23), value (9) | A list row. Flags: 1 selected (highlighted), 2 dim, 4 a square of its colour before the text, 8 its value in its colour. An empty text and value hides the row. |
| `14` LINE | index (0..2), text (23) | The text body: 0 big, 1 medium in the accent, 2 small. Empty lines close up; the lines are centred together. |
| `15` CELL | index (0..11), flags, position (0..127), colour, name (9), value (9) | 0..7: the cells body, two rows of four. 8..11: the foot, under K1..K4, always shown. Flags: 1 selected (framed), 2 dim, 4 bipolar (the bar from the middle), 8 no bar (the value alone). |
| `16` POPUP | tenths, text (23), text (23) | Two lines over the body, for tenths x 100 ms (0: until the next POPUP). Two empty lines take it down. |
| `17` CLEAR | | Everything blank (how the keys play stays). |
| `20` LIGHTS | buttons lit (bits, 3 bytes), buttons dim (3), keys lit (bits, 4 bytes), keys dim (4), flags | Bit b of the buttons is button b (as above); bit 14 is PLAY's green LED (bit 10 its red). Bit k of the keys is key k. Flags: bits 0-1 how bright the dim ones are (0 off, 1 low, 2 high), 4 held keys light too, 8 held buttons light too. Until the first LIGHTS the remote lights what is held. |
| `21` KEYS | channel (0..15), flags, octave | How the keys play while Melodee has the remote. Flags: 1 the keys are buttons (channel 16, notes 32 + key), 2 OCT- / OCT+ are Melodee's alone, 4 no poly pressure. Octave: 64 + octave (-3..+3), or 127 to leave it. |

### The remote's HELLO

`F0 7D 4D 52 01 <protocol> <major> <minor> <patch> <keys channel> <octave + 64> <keys flags> F7`

Protocol is 1. The version is the firmware's (0.0.0: a development build). The keys channel, octave and flags are
as the remote has them now.

## The screen

240 x 240. Header 32 rows (name, transport, info; the accent's rule under it), body 140 (six list rows, or three text
lines, or two rows of four cells), foot 68 (K1..K4). The remote redraws only the bands that changed, so Melodee may
send every message each time something changes; sending a message that changes nothing costs only its bytes.

Bluetooth MIDI carries a few kilobytes a second. A full screen (title, info, six rows, four knobs, lights) is about
300 bytes. Send what changed rather than the whole screen, and coalesce a knob's value while it turns (every 30 ms
is plenty).

## Examples

```
F0 7D 4D 52 01 F7                                          HELLO
F0 7D 4D 52 10 7F 40 00 05 44 72 75 6D 73 F7               TITLE "Drums", orange
F0 7D 4D 52 11 01 05 31 32 30 2E 30 F7                     INFO playing, "120.0"
F0 7D 4D 52 12 01 F7                                       BODY list
F0 7D 4D 52 13 00 05 00 7F 00 04 4B 69 63 6B 03 2D 33 64 F7 ROW 0 selected, green square, "Kick" "-3d"
F0 7D 4D 52 15 08 00 64 00 00 00 05 4C 45 56 45 4C 04 2D 33 2E 30 F7
                                                           CELL K1 "LEVEL" "-3.0", bar at 100
F0 7D 4D 52 20 00 08 01 00 00 00 01 00 00 00 00 00 00 00 05 F7
                                                           LIGHTS PLAY red and green, key 0; dim low, held keys
F0 7D 4D 52 02 F7                                          BYE
```

## Versions

A later version only adds commands and flags. The remote ignores commands it does not know; Melodee should look at
the HELLO's protocol number before using anything newer than version 1.


# Version 2

The remote draws Melodee FM-1's pages (its `firmware/src/ui_*.c`, ported to `app/ui/`): Stage, HOME's chord wheel, the
knob pages (TEMPO, CHORD, ARP, an FX slot, any page of four values), the list pages (SCL), SCALES, the FX racks, MIXER,
GLO's layer, PATTERNS, and over them an action sheet (SAVE's, a page's), a question, a knob's picker. Melodee says which
page shows and sends the parts it draws; the remote keeps them all (each until it changes) and draws a part again only
when what it shows changed. The remote is still a terminal: every control goes to Melodee as in version 1, and Melodee
decides what it does, with Melodee FM-1's grammar (SELECT turns the pages, ALGORITHM picks the track, OCT+ enters and
OCT- goes back, layers held, the knobs accelerated: all Melodee's). The remote does five things on its own, for speed:

- **The knobs' cards light** the moment K1..K4 turn (the card's value in its knob's colour, a bar under it, for 40
  frames: about 0.6 s), before Melodee's new value comes.
- **Stage's knobs drop in**: on the STAGE and WHEEL pages, a turn of K1..K4 brings the four cards down over the rows
  (and on STAGE, the picture under them, `PICTURE`) until 1.5 s after the last turn.
- **The digits roll**: a number that changes (the BPM, a card's value) rolls like Melodee FM-1's.
- **The keys play the scale**: QNT WHITE, SNAP and ALL (`SCALE`) map the keys on the remote, as Melodee FM-1's
  `seq.c kb_map` does. Melodee gets the notes already mapped (and transposed): it must not map them again.
- **Its own timing**: an armed track's border breathes while it waits and flashes on each beat while it records (the
  beat: its playhead on a beat's first step); pulses and meters fall about 2 dB a frame between `METER`s; the notes
  last played stay, faded, after they are let go.

## What changes from version 1

- **HELLO** (both ways) carries protocol `02`. Melodee says `F0 7D 4D 52 01 02 F7`; the remote's HELLO has three more
  bytes (below). A version 1 Melodee (Melodee iOS 9f36838) reads the first eight and goes on working.
- **The screen**: with `PAGE` 0 (generic, the default after a claim or `CLEAR`) version 1's TITLE, INFO, BODY, ROW,
  LINE and CELL are drawn as before, now in Melodee FM-1's look: TITLE as the header's chip in its colour, INFO where
  the BPM goes, K1..K4 (cells 8..11) as the strip of cards under the header, the list as its rows, the text centred.
  Any other `PAGE` draws that page from version 2's parts; version 1's body is then kept but not shown.
- **TITLE's colour** is the generic page's THEME. Version 2's pages take THEME from the selected place's track colour
  (`TRACK`, `SEL`).
- **POPUP** (version 1) shows as a card under the header on the generic page. Version 2's pages use `HEAD`'s status
  for messages (a card under the header when too long) and `SHEET` / `ASK` for decisions.
- **LIGHTS** and **KEYS** are unchanged and used by both. KEYS flag 1 (keys as buttons) is how PATTERNS' selection
  and GLO's keys work; flag 2 gives OCT- and OCT+ to Melodee (version 2's grammar: OCT+ enters, OCT- goes back).
- **CLEAR** blanks version 2's parts as well (`PAGE` 0, nothing in any part).
- **Texts** may hold ten letters beyond ASCII, as codes 1..10 (version 1 showed them as `?`): 1 `·` (middle dot),
  2 `°`, 3 `…`, 4 `©`, 5 `Ä`, 6 `Ö`, 7 `Ü`, 8 `ä`, 9 `ö`, 10 `ü`.
- **The 2 s full repaint** Melodee iOS does as a workaround goes: `PING` and the screen's generation replace it
  (below).

## Who is there, version 2

The remote counts its **screen generation**, 0..16383: it changes each time Melodee's screen goes without Melodee asking
(the remote powered on, its link lost, a stray message claiming it blank, BYE, another port taking it, Melodee gone
quiet). HELLO and CLEAR from the port that has it do not change it: Melodee paints after them anyway.

**The remote's HELLO**, version 2:

`F0 7D 4D 52 01 <protocol 02> <major> <minor> <patch> <keys channel> <octave + 64> <keys flags> <gen lo> <gen hi>
<state> F7`

gen: the generation, seven bits a byte, the lowest first. state: 0 free (the remote's own screen), 1 yours (the port
it is sent on has it), 2 taken (another port has it).

**STATE** (remote to Melodee): `F0 7D 4D 52 03 <gen lo> <gen hi> <state> <why> F7`

The answer to `PING`, and sent unasked to the port that had the screen whenever the generation changes (when that
port can still hear it). why: why the generation last changed: 0 not since power-on, 1 power-on, 2 the link went,
3 a message other than HELLO or CLEAR claimed it (blank but for that message), 4 BYE, 5 another port took it,
6 Melodee went quiet (below).

**PING** (Melodee to the remote): `F0 7D 4D 52 04 F7`

Answered with STATE on the same port. PING never claims the remote. A port that has PINGed once is expected to go on:
if it has the remote and says nothing (no PING, nothing else of the protocol's) for 7 s, the remote lets it go (why 6)
and shows its own screen; its keys play to every port again.

**Unasked HELLOs**: when a link comes up (a Bluetooth central subscribes, a USB host configures), the remote says
HELLO at once and again 1, 2, 4, 8 and 16 s after, while no Melodee claims it (a HELLO sent before Melodee listened is
lost: one was, on an iPad over Bluetooth, and the remote waited for Melodee while its keys played). After Melodee goes
quiet the greetings start again; after BYE they stop until the link comes again.

What Melodee does:

1. HELLO 02 to a port it suspects; on the remote's HELLO: `CLEAR`, paint everything, note the generation.
2. PING every 2 s while it drives the remote.
3. On STATE (or an unasked HELLO): if state is not 1 (yours), or the generation is not the one it noted, claim the
   remote again (HELLO 02, or CLEAR) and paint everything; note the new generation.
4. BYE when it stops driving the remote, as in version 1.

USB and Bluetooth carry the same bytes. Over USB the remote now takes Melodee's SysEx a byte at a time, as over
Bluetooth: a burst of messages arrives whole (it used to keep one frame and drop the rest of a burst).

## Messages, version 2

All Melodee to the remote: `F0 7D 4D 52 <command> <body> F7`, seven-bit bytes, texts and colours as in version 1 (a
colour 0 0 0: the palette's). A message that is malformed (cut short, a byte with its top bit set, a value out of its
range) changes nothing; bytes after a message's body are ignored (a later version's). Each message sets its part
whole; sending what did not change costs only its bytes. Numbers in brackets are the longest texts kept (longer is
cut).

| Command | Body |
| --- | --- |
| `30` PAGE | page, flags |
| `31` HEAD | transport, rec, octave + 64, song row, dots, dot, flags, BPM (5), chip (19), status (39) |
| `32` TRACK | place, numeral, flags, colour, pattern (3), next (3), name (23), role (11), extra (39) |
| `33` STEPS | place, length (1..64), the steps: 2 bits a step, three steps a byte |
| `34` PLAY | four playheads |
| `35` SEL | the selected place, the places shown (1..4) |
| `36` NOTES | count (0..16), then count pairs: note, place |
| `37` METER | four levels |
| `38` SCOPE | count (2..120), samples |
| `39` CARD | knob (0..3), flags, label (11), value (15), unit (7) |
| `3A` LIST | count, first, selected, picture |
| `3B` LROW | row (0..4), flags, label (23), value (15) |
| `3C` SCALE | QNT, root (0..11), scale low 7 bits, high 5 bits, transpose + 64, name (31) |
| `3D` PICTURE | kind, then the kind's values, notes and texts |
| `3E` FOOT | kind, text (23), right (11) |
| `3F` SHEET | shown, count (0..12), selected, title (23), subtitle (19) |
| `40` SROW | row (0..11), flags, label (23), value (15) |
| `41` ASK | shown, icon, question (47), detail (47), no (11), yes (11) |
| `42` RACK | row (0..3), flags, amount (0..127), tag (3), name (31), value (11) |
| `43` MIX | place, flags, level (0..127), dB (7), pan (7) |
| `44` GRID | place, flags, eight cells: state, three bars (0..10) |
| `45` GRIDTOP | page, pages (1..2), section lit, flags, the sections' letters (6) |
| `46` SCALES | rank, total, family (15) |
| `47` SCROW | at (0..4), shade (0..3), flags, name (31), detail (23) |
| `48` LANES | count (0..8), then count names (4) |
| `49` PICK | shown, knob, flags, count (0..5), the one now, then count pairs: family, name (15) |

### PAGE

| Page | What it draws | Its parts |
| --- | --- | --- |
| 0 generic | version 1's screen | TITLE, INFO, BODY, ROW, LINE, CELL, POPUP |
| 1 STAGE | HOME: a row per track, the band (the selected track's notes and chord, or its waveform; a drum track: its lanes); a knob turning: the cards dropped in, the picture under them | HEAD, TRACK, STEPS, PLAY, SEL, NOTES, SCOPE, LANES, CARD, PICTURE |
| 2 WHEEL | HOME's second page: the twelve notes in fifths, the notes sounding joined, the chord in the middle (else the key's name), the tracks' pulses in the corners, four octaves of keys | HEAD, TRACK, SEL, NOTES, METER, SCALE, CARD |
| 3 KNOBS | a page of four knobs: the cards, the picture under them, the sound bar (TEMPO, CHORD, ARP, an FX slot, an engine's page) | HEAD, CARD, PICTURE, FOOT, SCOPE, TRACK, SEL |
| 4 LIST | a list page: its rows, the picture under them, the sound bar (SCL) | HEAD, LIST, LROW, SCALE, FOOT |
| 5 SCALES | SCL > SCALES: the track's numeral and the family, the scale's place; two scales before, the picked one's card, two after | SCALES, SCROW, TRACK, SEL |
| 6 RACK | FX: the cards, four rack rows | HEAD, CARD, RACK, TRACK |
| 7 MIXER | GLO's MIXER: a strip a track | HEAD, TRACK, MIX, METER, SEL |
| 8 GLO | GLO held: the levels as cards (each in its track's colour), the mutes and solos as keys | HEAD (status `[GLO] SET`), CARD (flag 4), TRACK (flags muted, solo) |
| 9 PATTERNS | SEQ > PATTERNS: a row of eight patterns a track, the song's sections | HEAD, TRACK, GRID, GRIDTOP, PLAY, STEPS (lengths) |

PAGE flags: 1 GLO's Tap key dim (an outside clock). A new page (or new flags) draws everything again.

### HEAD: the header

transport: 0 stopped (a square), 1 playing (a triangle), 2 a song playing (its disc). rec: 0 none, 1 a filled REC mark
(the selected track armed or recording), 2 a ring (waiting, or another track armed). octave: 64 + the keys' octave
(an outlined pill when not 0). song row: 0 none, else "ROW n". dots, dot: the page's place in its family (a dot a
page, the page's a pill; 0 or 1 dots: none; too many for the room: "3/12"); STAGE and WHEEL: 2 and 0 or 1. flags:
1 the chip in MID (PATTERNS), 2 the BPM being turned (in the accent), 4 the status in REC's red (ERASING,
WAIT LOOP). BPM: its digits (rolling when they change). chip: where you are (Stage: the engine; a page: its title), in
the track's colour. status: in the chip's place: red with flag 4, else a message (a keycap first, `[GLO] SET`, draws
as a keycap: the names are `PRESETS SELECT ALGO OCT- OCT+ PLAY REC SAVE EDIT SEQ GLO HOME ARP KEYS K1 K2 K3 K4 MUTE ARM
HOLD ON OFF K1-4 SCL FX`); a message too long for the header is a card under it, wrapped. Empty: the chip.

### TRACK, STEPS, PLAY, SEL: the tracks

Pages show four **places** (Melodee FM-1's group of four tracks); Melodee picks which of its tracks are in them (the
selected track's group). numeral: the track's number as shown (1..99); 0: the place is free ("Add track", numbered by its place,
1..4). flags: 1 muted, 2 armed, 4 waiting (armed: stopped or waiting for the loop: its border breathes),
8 a drum track (its notes are lanes: NOTES, LANES), 16 idle (no pattern at all: one track's card shows no steps or
pattern), 32 soloed (GLO), 64 its sound being browsed (the name in the accent). colour: the track's colour (its knobs'
shades are turned from it on the remote: 0, +32, -32, +64 degrees of OKLCH hue, as Melodee FM-1's palettes; 0 0 0: the
palette's). pattern, next: the chips ("P1"; next: the one waiting for the bar, dashed; empty: none). name: the sound.
role: under or beside the name ("BASS", "DRUMS"; empty: "T" and the numeral). extra: one track's card only, what plays
besides ("ARP UP  HOLD  DIA7"), up to two lines.

STEPS: a place's pattern as Stage shows it, length steps (up to four bars), each 0 a rest, 1 a hit, 2 a tie (3 reads
as a rest), three steps a byte, the first in the lowest two bits: byte = s0 | s1 << 2 | s2 << 4. PLAY: each place's
step playing (0..63; 127 none), sent as it moves (Stage's dots, PATTERNS' progress, the REC flash). SEL: the selected
place (its colour is THEME) and how many places the page shows (Stage's rows: one 174 px card, two 86, three 56, four
42).

### NOTES, LANES, METER, SCOPE

NOTES: the notes sounding now on the places' tracks (keys held, steps' gates, the arp's), up to 16, each with its
place. STAGE's band shows the selected place's, WHEEL all but drum tracks'; let go (an empty NOTES), the last voicing
stays, faded, until new notes come. A drum track's notes are its lanes (0..7): each lights its lane's name on Stage's
band (LANES: the lanes' names, "BD" "SD" "CP" ...) for 10 frames.

METER: each place's output level, 0..127 over 60 dB (0: -60 dB or less, 127: full scale): WHEEL's pulses, MIXER's
meters. Send it about 15 times a second while those pages show.

SCOPE: the output's waveform, 2..120 samples across the screen, 64 its middle, 1..127: triggered at a rising zero
crossing and scaled so the larger of the window's peak and 1500 / 32768 of full scale reaches 1 or 127 (Melodee FM-1's
auto-scale). STAGE's band (no notes yet), STAGE's knobs (PICTURE 0 or 1), KNOBS with PICTURE 1 (TEMPO). Send it about
six times a second while a page shows a scope (120 samples: about 0.8 KB/s).

### CARD: the knobs' cards

The strip of four under the header (STAGE and WHEEL: when the knobs drop in): the label (capitals, in its knob's colour:
the track's shades), the value (rolling when its digits change; "ON" and "OFF" a badge; "[OCT+]" a keycap; a name in
sentence case, "SAW" shows "Saw"), the unit after it (lower case but dB, Hz, kHz; "STEP": "step"/"steps"). flags:
bits 0-1 the value's colour (0 text, 1 dim: the label too, 2 the accent, 3 its knob's colour: FX's family), 4 the label
in its place's track colour (GLO), 8 hot (the knob turning: also done by the remote itself). An empty label and value:
an empty card. Long labels take Melodee FM-1's short forms (CUTOFF "CUT", RELEASE "REL" ...).

### LIST, LROW, SCALE: a list page, the key

LIST: count (the rows in the list), first (the list's row in LROW 0), selected (the list's row picked), picture (0
none, 1 the octave with the scale: SCALE). The screen shows 5 rows, or 4 over a picture: Melodee keeps the picked row
in the window and sends LROW 0..4 for first .. first + 4. LROW flags: 1 more (a list behind it: ">"), 2 dim. Row k of
the list takes knob colour k mod 4 (its value, its tint when picked).

SCALE: the selected track's key. QNT: 0 OFF, 1 SNAP, 2 WHITE, 3 ALL. root: 0 C .. 11 B. scale: bit i = i semitones over
the root (bit 0 always set). transpose: 64 + semitones. name: "Natural minor" (WHEEL's middle when nothing is played:
"A minor"). The keys (key 0 is F3, 53; C4 is key 7), QNT not OFF and a Melodee driving: SNAP every key (with the
octave and the transpose) down to the scale; WHITE: the white keys walk the scale from C4 = the root (transposed), the
black keys are silent (no note sent); ALL: every key the next degree (C4 the root). OFF: the keys plus the transpose.
Melodee gets the notes mapped.

### PICTURE

The picture under the cards (KNOBS), or under Stage's dropped knobs (STAGE: 0 or 1 the scope, 2 a filter, 3 an
envelope).

| Kind | Body after the kind | Draws |
| --- | --- | --- |
| 0 none, 1 scope | | the waveform (SCOPE) |
| 2 filter | cutoff, resonance (0..127) | a low-pass response, its cutoff dashed |
| 3 envelope | attack, decay, sustain, release (0..127) | an ADSR |
| 4 arp | mode, octaves (1..4), count (0..16), the keys held | ARP's notes as blocks, each octave a colour; mode 0 off (dim), 1 up, 2 down, 3 up-down, 4 random, 5 as played, 6 repeat; no keys: C3 E3 G3 |
| 5 effect | effect, mix, rate, character, tone + 64, flags | the effect's shape (Melodee FM-1's numbers: 1 PHASER, 2 WIDTH, 3 FORMANT, 4 TAPE, 5 SPIT, 7 ROOM, 8 REVERB, 9 CHORUS, 10 NOTE TAIL, 11 NOTE ECHO, 12 DRIVE SOFT, 13 HARD, 14 FOLD, 15 FUZZ, 16 CRUSH, 17 FLANGER, 18 PEDAL PHASER, 19 PEDAL CHORUS; others draw nothing); ROOM and REVERB with TONE's strip, DRIVE with its tone; flags 1: "MEMORY FULL" |
| 6 chord | the accented note, count (1..16), the notes, name (23), caption (23) | CHORD: the name ("Bm7b5"), its caption ("LAST", "ON ROOT"), its notes on two or three octaves of keys, the accented one (the root, a melody's top) in the accent |
| 7 lines | three texts (23) | words in the panel ("CHORD KEYS OFF", "[K1] DIA3: IN-KEY CHORDS": a keycap first) |
| 8 steps | | the selected place's 64 steps as PATTERN shows them, the playhead white |

### FOOT: the sound bar

The foot of KNOBS and LIST pages: kind 0 the selected place's numeral (filled in its colour) and the text (the sound's
name), the right text at the right ("1/2") in the track's colour; 1 the four tracks' colours and the text ("All
tracks" when empty: a shared FX slot); 2 none.

### SHEET, SROW, ASK, PICK: over the page

SHEET: an action sheet, a card from the bottom over the page dimmed: the title (the subtitle at its right, or under it
when both do not fit), up to five rows around the picked one. shown 0 takes it down (the page whole again). SROW
flags: 1 red (it destroys something), 2 a dot (the value now). Melodee moves the pick and acts.

ASK: a question over the page dimmed: an icon (0 a warning, 1 motion cleared, 2 none), the question (wrapped, up to
three lines), the detail under it, the buttons No (OCT-) and Yes (OCT+, in the track's colour); empty: "No", "Yes".
The header stays live above it. ASK wins over SHEET while both are shown.

PICK: a knob's picker (a knob whose value is a name from a list, turning: a wave, an effect, a source), under its card,
over the page as it is: up to five names around its value, the one now tinted. flags 1: the effects' picker (wider,
each name its family's square, a rule between families: 1 Space, 2 Motion, 3 Dirt, 4 Colour, 5 Notes or Dynamics).
Melodee takes it down (shown 0) 1.5 s after the knob's last turn.

### RACK: an FX rack's rows

Four rows under the cards: FX's (the track's three slots, then the shared FX) or SHARED's (its three slots, then
RETURN). flags: 1 selected (outlined in KNOB 2's colour, its effect in KNOB 1's, its amount in KNOB 4's), 2 off (an
empty slot: "--", dim), 4 the shared row (an arrow, "Shared", then the name: the shared effects, comma-separated, as
a chain's arrows or with 16 split's commas), 8 RETURN (the four tracks' colours; name "Return"), 32 no amount. amount:
the bar (0..127); value: its words at the bar's end ("40 %"). tag: the row's number or letter ("1", "A").

### MIX: MIXER's strips

Per place: flags 1 muted ("Mute" in red, dim), 2 armed (a REC dot), bits 4-6 the control turning (1 LEVEL, 2 PAN,
4 MUTE: in text); level: the bar (0 "Off"); dB: the level's words ("-6"); pan: "L20", "C", "R24". The name: TRACK's.

### GRID, GRIDTOP: PATTERNS

GRID: a place's eight patterns on the page shown: state bits 0-1 (0 empty: an outline, 1 used: three bars, 2 playing:
filled, its progress from PLAY and STEPS' length), 4 queued (dashed), 8 Copy to's place (framed; empty: "+"); the bars:
the pattern's thirds' notes, 0 none (a dot) .. 10. flags: 1 its pattern on the other page, a tick at the left; 2 at the
right. GRIDTOP: the page (0, 1) of pages (1, 2: dots over the numerals), the section lit (127 none), flags 1 the jam's
rows ("JAM", else "SONG"), the sections' letters ("ABAC", none: "Empty").

### SCALES, SCROW: SCL > SCALES

SCALES: the picked scale's rank in the family, the family's total ("24/24"), the family's name. SCROW: at 0..4 is two
before the picked one .. two after (2: the card); shade: its family's knob colour (0..3); flags 1 a star (the card:
a favourite), 2 the card's place is words alone ("No favourites", "Not in this family"); name; detail (the card:
"53 notes · octave"). An empty name: no scale there.

## Bandwidth

A full Stage (PAGE, HEAD, four TRACKs, four STEPS, SEL, PLAY, four CARDs, SCALE) is about 650 bytes; a step of the
playhead 9 (PLAY); a knob turning, its CARD (25..40) every 30 ms at most. Send what changed. PING is 6 bytes every 2 s.

## Examples, version 2

```
F0 7D 4D 52 01 02 F7                                        HELLO, version 2
F0 7D 4D 52 30 01 00 F7                                     PAGE STAGE
F0 7D 4D 52 31 01 02 42 00 00 00 00 03 31 32 34 07 50 52 4F 50 48 45 54 00 F7
                                                            HEAD playing, a REC ring, octave +2, "124", "PROPHET"
F0 7D 4D 52 35 00 03 F7                                     SEL place 0, three places
F0 7D 4D 52 32 01 02 00 7F 60 22 02 50 31 02 50 33 0A 53 4F 4C 49 44 20 42 41 53 53 04 42 41 53 53 00 F7
                                                            TRACK place 1: 2, amber, P1, P3 next, "SOLID BASS", "BASS"
F0 7D 4D 52 33 01 10 11 11 14 04 11 01 F7                   STEPS place 1, 16: x.xx.x.x x.x.x.xx
F0 7D 4D 52 34 06 06 06 7F F7                               PLAY step 6 on three places
F0 7D 4D 52 3C 02 09 2D 0B 40 0D 4E 61 74 75 72 61 6C 20 6D 69 6E 6F 72 F7
                                                            SCALE WHITE, A minor
F0 7D 4D 52 04 F7                                           PING
F0 7D 4D 52 03 34 12 01 00 F7                               STATE (remote): generation 0x0934, yours
```

`tests/scenes.py` encodes every message (Python): its encoders are the protocol byte for byte, and its scenes paint
each page as Melodee would (`app/tests/v2_*.txt`, played to the simulator; `tools/play_scene.py` plays them to an
FM-1 over USB).
