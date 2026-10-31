# Melodee Remote licensing

Melodee Remote is licensed file by file; each file's `SPDX-License-Identifier` line is authoritative. Its own files
are MPL-2.0 (`LICENSE`), by Kerem Kilic (Ellic Studio).

| What | Licence |
| --- | --- |
| The app (`app/`), the build wrapper and overlay (`tools/`), the tests (`tests/`), the docs | MPL-2.0, Kerem Kilic (Ellic Studio) |
| The screen's drawing, ported from Melodee FM-1 (`app/ui/`; `tools/gen_aa_font.py`, `aa_raster.py`, `gen_icons.py`, `gen_ui_palettes.py`, `gen_aa_keycaps.py`; `assets/icons.json`, `assets/fonts/melodee_pixel.txt`) | MPL-2.0: Kerem Kilic (Ellic Studio), with Leo Kuroshita's Felucca code where the files say so, under his permission (below) |
| Rubik by The Rubik Project Authors (<https://github.com/googlefonts/rubik>): the screen's text and keycap labels, rasterised at build time (the generated tables are not offered as a font; the font declares no Reserved Font Name) | SIL OFL 1.1: `assets/fonts/Rubik[wght].ttf`, `LICENSES/OFL-Rubik.txt` |
| fm1-base and felucca-bsp, fetched at pinned revisions into `build/` (never committed) | Their own: MPL-2.0 file by file (Charles Vestal; Leo Kuroshita's Felucca code under his MPL-2.0 permission, hugelton/felucca-bsp#4), with their fonts (SIL OFL 1.1) and their third-party notices |
| JieLi's AC79 SDK and Bluetooth libraries, linked into the Bluetooth build from `$AC79_SDK` | JieLi's (Apache-2.0 for the SDK files fm1-base builds; see fm1-base's LICENSING.md) |

## From Melodee FM-1

The screen is Melodee FM-1's (Melodee's FM-1 firmware: its renderer `gfx.c`, its icons, its pages `ui_draw.c`,
`ui_stage.c`, `ui_view.c`, `ui_pages.c`, `ui_list.c`, `ui_scales.c`, `ui_popup.c`, `ui_patterns.c`, `ui_layer.c`, parts
of `ui_graph.c`, `ui.c` and `ui_menu.c`, and the tools that make its fonts, icons, keycaps and palettes), ported here.
Melodee FM-1 is GPL-3.0-only; the ported code is here under MPL-2.0, as its authors allow:

- Kerem Kilic's own code (Melodee's UI and its design are his).
- Leo Kuroshita's Felucca code that Melodee FM-1 keeps (in `app/ui/gfx.c`, `icons.c`, `ui_draw.c`, `ui_stage.c`,
  `ui_pages.c`, `ui_base.c` and the font, keycap and palette tools, each file's header saying so; and
  `assets/icons.json`, the icons' names, his): "Anyone may use the Felucca
  code for which I personally hold the copyright under MPL-2.0" (Leo Kuroshita, hugelton/felucca-bsp#4), with his
  name in the credits: Leo Kuroshita (@kurogedelic), Hügelton Instruments.
- No other contributor's code: Melodee FM-1's history was checked line by line (`git blame`) for what was ported; the
  header's octave pill takes Melodee FM-1's form from before another contributor's change to it.

Melodee FM-1's own copies stay GPL-3.0-only there.

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
