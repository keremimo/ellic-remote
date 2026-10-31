#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Melodee Remote's pages beside Melodee FM-1's own renders of the same states (tests/scenes.py).

  tools/compare_shots.py [RENDERS] [SHOTS] [OUT]

RENDERS: Melodee FM-1's renders (default ~/code/personal/Felucca/build/ui_new/NIGHT, its tests/ui_render.c);
SHOTS: the simulator's shots (app/build/tests); OUT: the pairs (app/build/compare): <scene>.png, Melodee FM-1 | the
remote at twice the size, and all.png, every pair on one sheet. Needs Pillow.
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
import scenes  # noqa: E402


def main():
    a = sys.argv[1:]
    renders = Path(a[0]) if a else Path.home() / "code/personal/Felucca/build/ui_new/NIGHT"
    shots = Path(a[1]) if len(a) > 1 else ROOT / "app/build/tests"
    out = Path(a[2]) if len(a) > 2 else ROOT / "app/build/compare"
    out.mkdir(parents=True, exist_ok=True)
    pairs = []
    for name, (render, doc, _) in scenes.SCENES.items():
        ref, shot = renders / f"{render}.png", shots / f"v2_{name}.png"
        if not shot.exists():
            print(f"  missing {shot}")
            continue
        im = Image.new("RGB", (2 * 480 + 12, 480 + 24), (40, 40, 40))
        d = ImageDraw.Draw(im)
        if ref.exists():
            im.paste(Image.open(ref).convert("RGB").resize((480, 480), Image.NEAREST), (0, 24))
        d.text((4, 6), f"Melodee FM-1: {render}", fill=(230, 230, 230))
        im.paste(Image.open(shot).convert("RGB").resize((480, 480), Image.NEAREST), (492, 24))
        d.text((496, 6), f"Melodee Remote: {name}", fill=(230, 230, 230))
        im.save(out / f"{name}.png")
        pairs.append(im)
    if pairs:
        cols = 2
        rows = (len(pairs) + cols - 1) // cols
        w, h = pairs[0].size
        sheet = Image.new("RGB", (cols * w // 2, rows * h // 2), (20, 20, 20))
        for i, p in enumerate(pairs):
            sheet.paste(p.resize((w // 2, h // 2)), ((i % cols) * w // 2, (i // cols) * h // 2))
        sheet.save(out / "all.png")
    print(f"compare: {len(pairs)} pairs -> {out}")


if __name__ == "__main__":
    main()
