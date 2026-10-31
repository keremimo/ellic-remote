# Melodee Remote licensing

Melodee Remote is licensed file by file; each file's `SPDX-License-Identifier` line is authoritative. Its own files
are MPL-2.0 (`LICENSE`), by Kerem Kilic (Ellic Studio).

| What | Licence |
| --- | --- |
| The app (`app/`), the build wrapper and overlay (`tools/`), the tests (`tests/`), the docs | MPL-2.0, Kerem Kilic (Ellic Studio) |
| fm1-base and felucca-bsp, fetched at pinned revisions into `build/` (never committed) | Their own: MPL-2.0 file by file (Charles Vestal; Leo Kuroshita's Felucca code under his MPL-2.0 permission, hugelton/felucca-bsp#4), with their fonts (SIL OFL 1.1) and their third-party notices |
| JieLi's AC79 SDK and Bluetooth libraries, linked into the Bluetooth build from `$AC79_SDK` | JieLi's (Apache-2.0 for the SDK files fm1-base builds; see fm1-base's LICENSING.md) |

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

The overlay changes fm1-base's MPL-2.0 files `ble/midi/le_midi.c`, `ble/shim/systimer.c` and `device/radio.c` in the
build copy. The changes are MPL-2.0 and published here, in `tools/base_overlay.py`; the changed files keep their
notices and add a line crediting the change.
