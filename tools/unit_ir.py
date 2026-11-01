#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""fm1.toml's ir_pass: `unit_ir.py IN.ll OUT.ll [UNIT]` (fm1-base tools/build.py). The unity build passes as it is.
Melodee FM-1's unit (app/unit/unit.c) gets two changes:
- its zero-filled statics go to the pool (section .pool, which fm1-base zeroes at start): the FM-1's 96 KB of RAM
  is fm1-base's, and the tracks Melodee tells (core.h trk, the mirror) fill tens of KB;
- its functions are made for size (optsize), as the unit is built with -Os, which the build's IR route turns to -O2.
"""
import re
import sys

ZERO = re.compile(r"^(@[\w.$\"]+ = (?:internal |private )?(?:unnamed_addr |local_unnamed_addr )?global "
                  r"[^=]*?(?:zeroinitializer|\b0|null|false)),( align \d+.*)$")


def main():
    src, dst = sys.argv[1], sys.argv[2]
    text = open(src).read()
    if len(sys.argv) < 4:
        open(dst, "w").write(text)
        return
    lines = text.split("\n")
    groups = set()
    moved = 0
    for i, line in enumerate(lines):
        if line.startswith("define "):
            groups.update(re.findall(r"#(\d+)\s*(?:\{|$|!)", line))
        elif line.startswith("@") and " section " not in line and " constant " not in line and "thread_local" not in line:
            m = ZERO.match(line)
            if m:
                lines[i] = f'{m.group(1)}, section ".pool",{m.group(2)}'
                moved += 1
    for i, line in enumerate(lines):
        m = re.match(r"^attributes #(\d+) = \{ (.*) \}$", line)
        if m and m.group(1) in groups and "optsize" not in m.group(2) and "optnone" not in m.group(2):
            lines[i] = f"attributes #{m.group(1)} = {{ optsize {m.group(2)} }}"
    open(dst, "w").write("\n".join(lines))
    print(f"unit_ir: {sys.argv[3]}: {moved} zero-filled statics into the pool, {len(groups)} attribute groups for size")


if __name__ == "__main__":
    main()
