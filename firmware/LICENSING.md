# Melodee Remote licensing

Melodee Remote is licensed file by file; each file's `SPDX-License-Identifier` line is authoritative. Its own files
are MPL-2.0 (`LICENSE`), by Kerem Kilic (Ellic Studio).

| What | Licence |
| --- | --- |
| The app (`app/`), the build wrapper and overlay, the release site's tools (`tools/`), the tests (`tests/`, `web/test_install.mjs`), the docs | MPL-2.0, Kerem Kilic (Ellic Studio) |
| The web installer (`web/install/`), Saadet's | MPL-2.0: Kerem Kilic (Ellic Studio), with fm1-base's installer code (Charles Vestal; Leo Kuroshita's Felucca update protocol) where the files say so; `fm1ota.js` and `fm1pkg.js` are fm1-base's, unchanged |
| Melodee FM-1's pages and controls, forked into `fm1/` (`fm1/src`, its font, icon, keycap, palette and table makers in `fm1/tools`, `fm1/assets/icons.json`, `fm1/assets/fonts/melodee_pixel.txt`) | MPL-2.0: Kerem Kilic (Ellic Studio), with Leo Kuroshita's Felucca code where the files say so, under his permission (below) |
| Rubik by The Rubik Project Authors (<https://github.com/googlefonts/rubik>): the screen's text and keycap labels, rasterised at build time (the generated tables are not offered as a font; the font declares no Reserved Font Name) | SIL OFL 1.1: `fm1/assets/fonts/Rubik[wght].ttf`, `LICENSES/OFL-Rubik.txt` |
| fm1-base and felucca-bsp, fetched at pinned revisions into `build/` (never committed) | Their own: MPL-2.0 file by file (Charles Vestal; Leo Kuroshita's Felucca code under his MPL-2.0 permission, hugelton/felucca-bsp#4), with their fonts (SIL OFL 1.1) and their third-party notices |
| JieLi's AC79 SDK and Bluetooth libraries, linked into the Bluetooth build from `$AC79_SDK` | JieLi's (Apache-2.0 for the SDK files fm1-base builds; see fm1-base's LICENSING.md) |
| m-bootloader 1.2 by Julius Vitkauskas (<https://github.com/jvitkauskas/m-bootloader>), offered by the release site from the pinned fm1-base's `web/m-bootloader` | MIT: its `LICENSE` published beside it |

## From Melodee FM-1

The remote runs Melodee FM-1's pages and controls (Melodee's FM-1 firmware), forked into `fm1/` from its repository
at the commit `fm1/SOURCE` names (copied then by a sync tool, since retired: `fm1/` is the remote's own and edited
here). Melodee FM-1 is GPL-3.0-only; what was copied is here under MPL-2.0 (each file's licence line rewritten, its
copyright lines kept), as its authors allow:

- Kerem Kilic's own code (Melodee's UI and its design are his).
- Leo Kuroshita's Felucca code that Melodee FM-1 keeps (each file's header says where), and `fm1/assets/icons.json`,
  the icons' names, his: "Anyone may use the Felucca code for which I personally hold the copyright under MPL-2.0"
  (Leo Kuroshita, hugelton/felucca-bsp#4), with his name in the credits: Leo Kuroshita (@kurogedelic), Hügelton
  Instruments.
- No other contributor's lines: the sync checked every copied line (`git blame`) and rewrote another author's (the
  remote's own code: the USB HOST lines left out, the header's room for its pills, the knob acceleration, the nearest
  microtonal degree). Every change since is Kerem Kilic's.
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
- `web/install/` is Saadet's web installer (MPL-2.0 there too) with Melodee Remote's words: Kerem Kilic's code, with
  the portions it keeps from fm1-base's `web/installer.html` and `fm1ota.js`/`fm1pkg.js` (Charles Vestal, Leo
  Kuroshita) under MPL-2.0 as fm1-base gives them; `web/test_install.mjs` is its test.
- `tools/make_site.py`, `tools/release_support.py`, `tools/prepare_public_release.py` and `tests/site_test.py` follow
  Saadet's. Saadet's `make_site.py` and `site_test.py` are GPL-3.0-only there; they are Kerem Kilic's alone, and here
  he licenses them MPL-2.0.

Nothing of Saadet's instrument is here: no engines, no factory sound data (Roland's, Sequential's, Casio's), no
Felucca-derived UI or editor code, and none of Saadet's GPL-3.0-only web pages (its editor and manual).

## Changes to fm1-base

The overlay changes fm1-base's MPL-2.0 files `ble/midi/le_midi.c`, `ble/shim/systimer.c`, `device/radio.c`,
`device/plat_fm1.c`, `host/plat_host.c`, `include/plat.h` and felucca-bsp's `chip/usb.c` (USB's SysEx to the app a
byte at a time) and `boards/fm1/fm1_input.h` (the light levels) in the build copy. The changes are MPL-2.0 and
published here, in `tools/base_overlay.py`; the changed files keep their notices and add a line crediting the change.
