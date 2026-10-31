# The Melodee Remote protocol, version 1

How Melodee and an FM-1 running Melodee Remote talk. Melodee draws the remote's screen and lights its panel; the
remote sends every key, button, encoder and MASTER move back. The remote has no idea what a track or a sound is:
Melodee decides what each control does on each screen, so Melodee's features reach the remote without new firmware.

The same bytes work over Bluetooth MIDI (the remote is a peripheral advertised as **Melodee**, GAP name
**Melodee Remote**) and USB MIDI (USB product **Melodee Remote FM-1**). `app/mel_proto.h` is the firmware's copy of
every number below.

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
