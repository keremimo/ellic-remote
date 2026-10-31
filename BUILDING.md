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

## The screen

`app/ui/` is Melodee FM-1's drawing, ported: its renderer (`gfx.c`: canvases sent where they changed, theme tokens,
Rubik in four roles, keycaps), its icons, and its pages (`ui_draw.c` the header, cards and sound bar; `ui_stage.c`
Stage; `ui_view.c` the chord wheel; `ui_pages.c` the knob pages, pictures, racks and MIXER; `ui_list.c`, `ui_scales.c`,
`ui_popup.c`, `ui_patterns.c`, `ui_layer.c`), reading the mirror Melodee fills (`mel_v2_t`) where Melodee FM-1 reads its
model. `ui_generic.c` draws protocol version 1's screen (and the remote's own) in the same look; `ui_frame.c` picks
the page. fm1-base's own `gfx.c` stays for its screens (boot, update, crash); the two share names, so the app takes
Melodee FM-1's under `ux_` names (`ui/gfx_names.h`).

## Tests

`tests/run_tests.sh` runs:

- `proto`: the protocol on its own (`tests/proto_test.c` on `app/mel_proto.c`): every message read, malformed ones
  changing nothing, SysEx gathered a byte at a time, HELLO packed as USB-MIDI.
- `host-*`: the app in fm1-base's host simulator, `app/tests/*.txt`. The app adds commands for the scripts: `sysex`
  (a frame from USB) and `blesysex` (from Bluetooth, a byte at a time), hex with `"TEXT"` for a text (its length, then
  its letters); `text WHERE WORDS` (a text on the screen: title, info, line0..2, row0..5, rowv0..5, cell0..11,
  cellv0..11, popup0..1, say0..1); `quiet` (no MIDI since the last `out`); and `expect` values (host, octave, setup,
  own, body, keys_ch, frames, popup, selected, set_*, theme, btn_lit, keys_lit). Screenshots land in
  `app/build/tests/`.
- `ble-midi`: the overlay's Bluetooth parser and sender, cut out of the build copy's `le_midi.c` and compiled against
  stand-ins for JieLi's stack (`tests/ble_midi_harness.c`): SysEx whole, split, with clock inside, cut short; sent and
  read back byte for byte at MTU 20 and 182.

- `v2_*` (in `host-*`): each of version 2's pages painted as Melodee would (`tests/scenes.py` writes them:
  `build/venv/bin/python tests/scenes.py`), its shot in `app/build/tests/v2_<scene>.png`; `session` (PING, STATE, HELLO
  again, a stray claim, silence, a link lost, a port taking it) and `scale_keys` (QNT WHITE, SNAP, ALL, the transpose).
- `tools/compare_shots.py`: each scene's shot beside Melodee FM-1's own render of the same state
  (`~/code/personal/Felucca/build/ui_new/NIGHT`, its `tests/ui_render.c`), in `app/build/compare/`.

None of this runs on an FM-1, and the Bluetooth stack itself is not in the tests. On an FM-1: a build with fm1-base's
screen mirror (`FM1B_MIRROR=1 ./build.sh`), installed, then `tools/play_scene.py` (Saadet's `build/venv`, which has
mido) plays the scenes to it over USB and saves its screen in `app/build/device/`. The mirror sends what fits its ring
and asks the app to draw again; rows that are not the screen's whole width are sent again whole each time, so a busy
page (Stage) comes back with rows missing: the mirror's, not the screen's.

## Installing

Installation is a separate, explicit step, with fm1-base's installer from the build copy:

```sh
build/venv/bin/python build/fm1-base-platform/tools/fm1_install.py app/build/melodee-remote.fwsc --port FM-1
```

A normal update keeps the device's bootloader (m-bootloader too, once installed). To go back to Saadet, install
Saadet's package the same way.
