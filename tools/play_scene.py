#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Play tests/scenes.py's scenes to a real FM-1 running Melodee Remote over USB, as Melodee would, and save its screen
(a test build with fm1-base's screen mirror: FM1B_MIRROR=1 ./build.sh). Each shot beside the simulator's.

  tools/play_scene.py [SCENE ...] [--port FM-1] [--out app/build/device]

Needs mido with python-rtmidi (as fm1-base's tools/fm1_install.py; Saadet's build/venv has them). Lets the remote go
(BYE) at the end.
"""
import argparse
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
sys.path.insert(0, str(ROOT / "build/fm1-base-platform/tools"))
import scenes  # noqa: E402
import fm1_install as I  # noqa: E402
import fm1_backup as B  # noqa: E402
import fm1_mirror as M  # noqa: E402


def flat(msgs):
    for m in msgs:
        if isinstance(m[0], list):
            yield from m
        else:
            yield m


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scenes", nargs="*", help="the scenes (default: all)")
    ap.add_argument("--port", default="FM-1")
    ap.add_argument("--out", default=str(ROOT / "app/build/device"))
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    dev = B.find(I.MidoBackend(), a.port, B.ANY)
    link = dev.link
    try:
        for name in a.scenes or list(scenes.SCENES):
            msgs, _ = scenes.SCENES[name][2]()
            link.send(bytes(scenes.hello()))
            time.sleep(0.15)
            for m in flat(msgs):
                link.send(bytes(m))
                time.sleep(0.004)
            time.sleep(0.6)
            scr = M.grab(link, quiet=2.0, limit=30.0)   # (the mirror asks the app to draw again: give it time)
            scr.png(str(out / f"v2_{name}.png"))
            print(f"{name}: {scr.frames} frames, {scr.bad} bad")
        link.send(bytes(scenes.msg(0x02, [])))
    finally:
        link.close()


if __name__ == "__main__":
    main()
