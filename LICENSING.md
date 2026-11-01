# Melodee Remote licensing

Melodee Remote is licensed file by file; each file's `SPDX-License-Identifier` line is authoritative. Its own files
are MPL-2.0 (`LICENSE`), by Kerem Kilic (Ellic Studio).

| What | Licence |
| --- | --- |
| The app (`app/`), the build wrapper and overlay (`tools/`), the tests (`tests/`), the docs | MPL-2.0, Kerem Kilic (Ellic Studio) |
| Melodee FM-1's pages and controls, copied by `tools/sync_fm1.py` into `fm1/` (`fm1/src`, its font, icon, keycap, palette and table makers in `fm1/tools`, `fm1/assets/icons.json`, `fm1/assets/fonts/melodee_pixel.txt`) | MPL-2.0: Kerem Kilic (Ellic Studio), with Leo Kuroshita's Felucca code where the files say so, under his permission (below) |
| Rubik by The Rubik Project Authors (<https://github.com/googlefonts/rubik>): the screen's text and keycap labels, rasterised at build time (the generated tables are not offered as a font; the font declares no Reserved Font Name) | SIL OFL 1.1: `fm1/assets/fonts/Rubik[wght].ttf`, `LICENSES/OFL-Rubik.txt` |
| fm1-base and felucca-bsp, fetched at pinned revisions into `build/` (never committed) | Their own: MPL-2.0 file by file (Charles Vestal; Leo Kuroshita's Felucca code under his MPL-2.0 permission, hugelton/felucca-bsp#4), with their fonts (SIL OFL 1.1) and their third-party notices |
| JieLi's AC79 SDK and Bluetooth libraries, linked into the Bluetooth build from `$AC79_SDK` | JieLi's (Apache-2.0 for the SDK files fm1-base builds; see fm1-base's LICENSING.md) |

## From Melodee FM-1

The remote runs Melodee FM-1's pages and controls (Melodee's FM-1 firmware), copied into `fm1/` by
`tools/sync_fm1.py` from its repository at the commit `fm1/SOURCE` names. Melodee FM-1 is GPL-3.0-only; what is
copied is here under MPL-2.0 (each file's licence line rewritten, its copyright lines kept), as its authors allow:

- Kerem Kilic's own code (Melodee's UI and its design are his).
- Leo Kuroshita's Felucca code that Melodee FM-1 keeps (each file's header says where), and `fm1/assets/icons.json`,
  the icons' names, his: "Anyone may use the Felucca code for which I personally hold the copyright under MPL-2.0"
  (Leo Kuroshita, hugelton/felucca-bsp#4), with his name in the credits: Leo Kuroshita (@kurogedelic), Hügelton
  Instruments.
- No other contributor's lines: the sync checks every copied line (`git blame`) and refuses another author's unless
  `tools/fm1_overlay.py` rewrites it (the remote's own code: the USB HOST lines left out, the header's room for its
  pills, the knob acceleration, the nearest microtonal degree).
- No other people's code or data: the files that port other projects (FM6's Dexed and MSFA, SID's reSID, the 909's
  9W9, SLOOP's effects, CrispyZebra, DaisySP, Rings, MAME's uPD933) and the factory data (Casio's CZ-1 tones,
  Sequential's Prophet programs) are never copied, and `gen_tables.py` comes without its reSID and FM6 tables. The
  remote has no engine: where Melodee FM-1's pages name one, `app/unit/` answers that it is absent.

A few pure functions of files the remote leaves out (the keys' layout, the song's rows, a take's notes, the matrix's
names) are copied as they are into `fm1/src/excerpt_*.c`, under the same checks. Melodee FM-1's own copies stay
GPL-3.0-only there.

## From Saadet

The remote takes its platform from Saadet's fm1-base port, and only Kerem Kilic's own code from it:

- `tools/build_base.py` follows Saadet's `tools/build_base.py` (the pinned fetch, the disposable copy).
- `tools/base_overlay.py` carries, from Saadet's `tools/base_overlay.py`, its `replace` and `function` helpers and its
  peripheral-link changes to fm1-base (timer routing, fresh-link state, the subscription bit, GAP name reads, Bluetooth
  off while starting). The SysEx and naming changes are new.
- `app/app.h` keeps Saadet's splash colour and its panel-calibration store offset.

Nothing of Saadet's instrument is here: no engines, no factory sound data (Roland's, Sequential's, Casio's), no
Felucca-derived UI or editor code, and none of Saadet's GPL-3.0-only tools or web pages.

## Changes to fm1-base

The overlay changes fm1-base's MPL-2.0 files `ble/midi/le_midi.c`, `ble/shim/systimer.c`, `device/radio.c` and
felucca-bsp's `chip/usb.c` (USB's SysEx to the app a byte at a time) in the build copy. The changes are MPL-2.0 and
published here, in `tools/base_overlay.py`; the changed files keep their notices and add a line crediting the change.
