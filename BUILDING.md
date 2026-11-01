# Building Melodee Remote

The same tools as Saadet: the JieLi pi32v2 Linux toolchain and the AC79 V1.2.1 SDK, with Docker or OrbStack running
on macOS for the Linux compiler, and a Python with Pillow and fontTools for the fonts (fm1-base's, and Melodee FM-1's
Rubik faces, icons, keycaps and palettes: fm1.toml `gen`, tools/). The simulator needs only a C compiler.

```sh
python3 -m venv build/venv
build/venv/bin/pip install Pillow fonttools
PYTHON="$PWD/build/venv/bin/python" ./build.sh            # app/build/melodee-remote.fwsc, Bluetooth
PYTHON="$PWD/build/venv/bin/python" ./build.sh --no-ble   # app/build/melodee-remote-usb.fwsc, USB only
PYTHON="$PWD/build/venv/bin/python" ./build.sh --host     # app/build/host/melodee-remote_host, the simulator
PYTHON="$PWD/build/venv/bin/python" tests/run_tests.sh
```

`./build.sh` runs `tools/build_base.py`: it fetches fm1-base at Saadet's pin (`6c0b47e4bfc5`, felucca-bsp
`0f08fcaa6610`) into `build/deps/`, refuses a checkout that is elsewhere or changed, copies it to
`build/fm1-base-platform/`, applies `tools/base_overlay.py` there and runs fm1-base's `tools/build.py` on `app/`.
`FM1_BASE_MIRROR=PATH` fetches from a local clone (Saadet's `build/deps/fm1-base-6c0b47e4bfc5`) instead of GitHub.
`JIELI_TOOLCHAIN` (default `~/.jieli/toolchain`) and `AC79_SDK` (default `~/fw-AC79_AIoT_SDK`) as for Saadet.
Builds run one at a time: they share the disposable copy.

The package identity is `FM-1_1400` for development builds and `FM-1_14XXYYZZ` for releases
(`./build.sh --release 0.1.0`). Each fm1-base firmware has its own prefix: Saadet's is `FM-1_9`.

## The overlay

`tools/base_overlay.py` changes the copy, never the pinned checkout, and fails if a line it changes is not there:

- USB MIDI SysEx in: felucca-bsp's `chip/usb.c` hands each SysEx byte to the app as it comes
  (`app_usb_sysex_byte`, app.h `APP_USB_SYSEX_BYTE`); it keeps one frame, so a burst of Melodee's lost all but its
  first.
- Bluetooth MIDI SysEx in: `midi_parse` (ble/midi/le_midi.c) passes SysEx bytes to the app (`mel_ble_sysex`) across
  packets, with real time allowed in the middle; the base skipped SysEx.
- Bluetooth MIDI SysEx out: `ble_midi_send_many` packs USB-MIDI SysEx packets (CIN 4..7) as BLE MIDI SysEx, F0 and F7
  each after a timestamp, continuing straight after the next packet's header; the base sent them as garbage.
- The name: "Melodee" in the advertisement (eight letters fit beside the MIDI service), "Melodee Remote" in the scan
  response and the GAP name.
- Saadet's peripheral fixes: a fresh link negotiates its own subscription and MTU, the subscription bit is read
  alone, GAP name reads past the end are answered, BLE-only builds route the SDK's `sys_timer` timers to the main
  loop, and Bluetooth switched off while starting stays off.
- Melodee FM-1's light levels: felucca-bsp's scan gets three planes, each lit as a lit LED on the frames its mask
  leaves (full, 1/2, 1/4, 1/8), set by `plat_lights_planes` (plat.h): LIGHTS LOW .. FULL, the idle buttons' quarter,
  the breathing. fm1-base's short glow pulse is not used.

## The unit

`app/unit/unit.c` is Melodee FM-1's pages and controls as one compilation unit of their own (fm1.toml `o2`), its
files from `fm1/` in Melodee FM-1's order, built for size, with 32 tracks (core.h `NMIDI` 28, none MIDI-only). On the
FM-1 its zero-filled statics go to fm1-base's pool and its functions are built for size by `tools/unit_ir.py` (fm1.toml
`ir_pass`, a pass over its LLVM IR): the FM-1's RAM is fm1-base's. Beside them, the remote's own sections where Melodee FM-1 has
hardware or a model of its own: `remote_hal.c` (the panel, LEDs and LCD by fm1-base, through `unit.h`),
`remote_sounds.c` (the engine table: Melodee's instruments), `remote_model.c` (the sequencer, patterns, takes and
song: Melodee's, mirrored), `remote_project.c` (projects and tracks: requests), `remote_keys.c` (the keys as MIDI),
`link.c` (the protocol), `remote_told.h`/`remote_told.c` (the selected track's sound pages as Melodee tells them: the
page table's told slots, their values and actions, the wave), `remote_follow.c` (one screen with Melodee's: PAGE; the
menu's settings kept by Melodee: PREFS), `remote_main.c` (the start and the loop). `upreset.c`, `absent_engines.h` and
`absent_sound.h` answer what the pages still ask of Melodee FM-1's sounds and engines: absent.

`fm1/` is the remote's own, forked from Melodee FM-1 (`fm1/SOURCE` names the commit) and edited here: what a remote
has no use for is cut (Melodee FM-1's engine pages, user presets, projects and slots, NAME, NEW SONG, MIDI knobs,
MOTION, the polyphony budget's pages, autosave), and SAVE only asks Melodee to save. It is never synced again.

## Tests

`tests/run_tests.sh` (about 10 s) runs:

- `host-*`: the app in fm1-base's host simulator, `app/tests/*.txt`, each at most a minute. `pages` walks the pages
  with the controls; `link` plays Melodee (version 4): it claims the remote, tells it an instrument, named tracks, the
  selected track's sound pages (an envelope, a wave and an action), a step and the transport, and checks what the
  controls send back, the pages followed both ways, the preferences, a toast and the pads on the keys; `lights` the
  LEDs' levels and SAVE. The app adds script commands: `sysex HEX..` (a frame from USB),
  `blesysex HEX..` (from Bluetooth), `text page TITLE` (`text nameK NAME`, `text toast WORDS`: the rest of the line),
  and `expect` values (home, page, menu, pop, confirm, layer, sel, playing, rec, bpm, octave, tracks, list_row,
  list_first, host, frames_in, frames_out, last_req, last_cmd, instruments, pattern, keyN (a key's light), lights,
  lights_mask, told_n, toldK, keys, act). Screenshots land in `app/build/tests/`.
- `ble-midi`: the overlay's Bluetooth parser and sender, cut out of the build copy's `le_midi.c` and compiled against
  stand-ins for JieLi's stack (`tests/ble_midi_harness.c`): SysEx whole, split, with clock inside, cut short; sent and
  read back byte for byte at MTU 20 and 182.

The Bluetooth stack itself is not in the tests.

## Installing

Installation is a separate, explicit step, with fm1-base's installer from the build copy:

```sh
~/code/personal/saadet/build/venv/bin/python build/fm1-base-platform/tools/fm1_install.py app/build/melodee-remote.fwsc --port FM-1
```

(a Python with mido and python-rtmidi: Saadet's `build/venv` has them).

A normal update keeps the device's bootloader (m-bootloader too, once installed). To go back to Saadet, install
Saadet's package the same way.
