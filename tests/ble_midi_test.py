#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Cut the patched Bluetooth MIDI parser and sender out of the build copy's le_midi.c (any ./build.sh run makes it),
compile them with tests/ble_midi_harness.c and run it."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "build/fm1-base-platform/ble/midi/le_midi.c"
OUT = ROOT / "build/tests"


def function_end(s, start):
    opening = s.index("{", start)
    level, end = 1, opening + 1
    while level:
        level += {"{": 1, "}": -1}.get(s[end], 0)
        end += 1
    return end


s = SRC.read_text()
a = s.index("static int midi_len(u8 st)")
b = function_end(s, s.index("static void midi_parse(const u8 *b, u16 len)"))
c = s.index("static u8 sx_out;")
d = function_end(s, s.index("int ble_midi_send_many("))
OUT.mkdir(parents=True, exist_ok=True)
(OUT / "le_midi_cut.c").write_text("/* cut from " + str(SRC) + " */\n" + s[a:b] + "\n" + s[c:d] + "\n")
exe = OUT / "ble_midi_test"
subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wno-unused-function", "-I", str(OUT), "-o", str(exe),
                str(ROOT / "tests/ble_midi_harness.c")], check=True)
sys.exit(subprocess.run([str(exe)]).returncode)
