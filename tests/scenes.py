#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Melodee's side of protocol version 2 played to the host simulator: each scene paints one of Melodee FM-1's pages
as Melodee would (docs/PROTOCOL.md), checks what the remote took, and shoots it, the same state as one of Melodee FM-1's
own renders (its tests/ui_render.c, build/ui_new/NIGHT/<render>.png) so the two can be put side by side
(tools/compare_shots.py). The encoders below are the protocol's, byte for byte: a reference for Melodee iOS.

  tests/scenes.py            writes app/tests/v2_<scene>.txt (tests/run_tests.sh runs them)
  tests/scenes.py --list     the scenes and the renders they match
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# ---------------------------------------------------------------- the encoders (docs/PROTOCOL.md) ---

HEAD = [0xF0, 0x7D, 0x4D, 0x52]


EXTRA = {"\xb7": 1, "\xb0": 2, "\u2026": 3, "\xa9": 4, "\xc4": 5, "\xd6": 6, "\xdc": 7, "\xe4": 8, "\xf6": 9,
         "\xfc": 10}   # the faces' letters beyond ASCII: codes 1..10 in a text


def text(s, n=None):
    b = [EXTRA.get(c, ord(c) if 32 <= ord(c) < 127 else ord("?")) for c in s]
    if n is not None:
        b = b[:n]
    return [len(b)] + b


def colour(rgb):
    """8-bit RGB to the protocol's three 7-bit components (0 0 0: the palette's)"""
    if rgb is None:
        return [0, 0, 0]
    return [c >> 1 for c in rgb]


def msg(cmd, body):
    assert all(0 <= b < 128 for b in body), (cmd, body)
    return HEAD + [cmd] + list(body) + [0xF7]


def hello(version=2):
    return msg(0x01, [version])


def ping():
    return msg(0x04, [])


def clear():
    return msg(0x17, [])


PAGES = {"generic": 0, "stage": 1, "wheel": 2, "knobs": 3, "list": 4, "scales": 5, "rack": 6, "mixer": 7, "glo": 8,
         "patterns": 9}


def page(name, flags=0):
    return msg(0x30, [PAGES[name], flags])


def head(bpm="124", chip="", status="", transport=1, rec=0, octave=0, row=0, dots=0, dot=0, flags=0):
    return msg(0x31, [transport, rec, 64 + octave, row, dots, dot, flags] + text(bpm) + text(chip) + text(status))


TF = {"muted": 1, "armed": 2, "waiting": 4, "drum": 8, "idle": 16, "solo": 32, "pending": 64}


def track(place, numeral, name, rgb=None, pattern="P1", next="", role="", extra="", flags=()):
    f = sum(TF[x] for x in flags)
    return msg(0x32, [place, numeral, f] + colour(rgb) + text(pattern) + text(next) + text(name) + text(role) +
               text(extra))


def steps(place, pattern):
    """pattern: a string, a character a step: '.' rest, 'x' a hit, '-' a tie"""
    v = {".": 0, "x": 1, "-": 2}
    st = [v[c] for c in pattern.replace(" ", "")]
    body = []
    for i in range(0, len(st), 3):
        b = 0
        for k, s in enumerate(st[i:i + 3]):
            b |= s << (2 * k)
        body.append(b)
    return msg(0x33, [place, len(st)] + body)


def play(*ph):
    ph = list(ph) + [127] * (4 - len(ph))
    return msg(0x34, ph)


def sel(place, shown):
    return msg(0x35, [place, shown])


def notes(*pairs):
    """(note, place) pairs"""
    body = [len(pairs)]
    for n, p in pairs:
        body += [n, p]
    return msg(0x36, body)


def meter(*lv):
    return msg(0x37, list(lv) + [0] * (4 - len(lv)))


def scope(samples):
    return msg(0x38, [len(samples)] + [max(0, min(127, 64 + s)) for s in samples])


CV = {"text": 0, "dim": 1, "accent": 2, "knob": 3}


def card(k, label, value, unit="", cv="text", track=False, hot=False):
    return msg(0x39, [k, CV[cv] | (4 if track else 0) | (8 if hot else 0)] + text(label) + text(value) + text(unit))


def listpage(count, first, selected, picture=0):
    return msg(0x3A, [count, first, selected, picture])


def lrow(i, label, value, more=False, dim=False):
    return msg(0x3B, [i, (1 if more else 0) | (2 if dim else 0)] + text(label) + text(value))


QNT = {"off": 0, "snap": 1, "white": 2, "all": 3}


def scale(root, degrees, name, qnt="off", transpose=0):
    """degrees: semitones over the root in the scale"""
    mask = sum(1 << d for d in degrees)
    return msg(0x3C, [QNT[qnt], root, mask & 127, mask >> 7, 64 + transpose] + text(name))


PIC = {"none": 0, "scope": 1, "filter": 2, "env": 3, "arp": 4, "fx": 5, "chord": 6, "lines": 7, "steps": 8, "keys": 9}


def picture(kind, values=(), notes=None, texts=()):
    body = [PIC[kind]] + list(values)
    if notes is not None:
        body += [len(notes)] + list(notes)
    for t in texts:
        body += text(t)
    return msg(0x3D, body)


def foot(name, right="", kind=0):
    return msg(0x3E, [kind] + text(name) + text(right))


def sheet(title, rows, selected, sub="", shown=True):
    out = [msg(0x3F, [1 if shown else 0, len(rows), selected] + text(title) + text(sub))]
    for i, r in enumerate(rows):
        label, value, flags = (list(r) + [""] * (2 - len(r)) + [0])[:3]
        out.append(msg(0x40, [i, flags] + text(label) + text(value)))
    return out


def ask(question, detail="", no="", yes="", icon=0, shown=True):
    return msg(0x41, [1 if shown else 0, icon] + text(question) + text(detail) + text(no) + text(yes))


KF = {"selected": 1, "off": 2, "shared": 4, "return": 8, "split": 16, "noamount": 32}


def rack(row, tag, name, value="", amount=0, flags=()):
    return msg(0x42, [row, sum(KF[x] for x in flags), amount] + text(tag) + text(name) + text(value))


def mix(place, level, db, pan, muted=False, armed=False, hot=0):
    return msg(0x43, [place, (1 if muted else 0) | (2 if armed else 0) | hot << 4, level] + text(db) + text(pan))


GC = {".": 0, "u": 1, "p": 2}


def grid(place, cells, flags=0):
    """cells: 8 of (state, bars): state '.' empty, 'u' used, 'p' playing, + 'q' queued, + 't' Copy to's place"""
    body = [place, flags]
    for st, bars in cells:
        s = GC[st[0]] | (4 if "q" in st else 0) | (8 if "t" in st else 0)
        body += [s] + list(bars)
    return msg(0x44, body)


def gridtop(page_, pages, current, letters, jam=False):
    return msg(0x45, [page_, pages, current, 1 if jam else 0] + text(letters))


def scales_top(rank, total, family):
    return msg(0x46, [rank, total] + text(family))


def scrow(at, name, shade=0, detail="", star=False, words=False):
    return msg(0x47, [at, shade, (1 if star else 0) | (2 if words else 0)] + text(name) + text(detail))


def lanes(*names):
    body = [len(names)]
    for n in names:
        body += text(n)
    return msg(0x48, body)


def pick(knob, names, now, fams=None, fx=False, shown=True):
    body = [1 if shown else 0, knob, 1 if fx else 0, len(names), now]
    for i, n in enumerate(names):
        body += [fams[i] if fams else 0] + text(n)
    return msg(0x49, body)


def keys(channel=0, flags=2, octave=None):
    return msg(0x21, [channel, flags, 127 if octave is None else 64 + octave])


# ------------------------------------------------------------------------------- the scenes ---

CORAL, AMBER, MINT, LILAC = (255, 122, 92), (255, 193, 69), (70, 217, 176), (157, 140, 255)   # NIGHT's tracks


def wave(n=120, seed=7):
    """a scope's trace: a noisy wave, as a synth playing looks"""
    import math
    out, r = [], seed
    for i in range(n):
        r = (r * 1103515245 + 12345) & 0x7FFFFFFF
        out.append(int(28 * math.sin(i / n * 6.283 * 9) + 16 * math.sin(i / n * 6.283 * 23) + (r >> 16) % 13 - 6))
    return out


def stage_cards():
    return [card(0, "CUTOFF", "62", "%"), card(1, "RESO", "30", "%"), card(2, "ENV", "45", "%"), card(3, "DECAY", "40", "%")]


SCENES = {}


def scene(name, render, doc):
    def wrap(fn):
        SCENES[name] = (render, doc, fn)
        return fn
    return wrap


@scene("stage_3tracks", "stage_3tracks", "Stage, three tracks: one armed, one waiting for its next pattern")
def _():
    return [page("stage"), head(chip="PROPHET", rec=2), sel(0, 3),
            track(0, 1, "It's a Prophet", CORAL), steps(0, "." * 16),
            track(1, 2, "SOLID BASS", AMBER, next="P3", role="BASS"),
            steps(1, "x.xx.x.x x.x.x.xx .x.x x..x x.x. x.x."),
            track(2, 3, "SYNTH.LEAD 1", MINT, flags=["armed"]), steps(2, "." * 16),
            play(6, 6, 6), scope(wave()), *stage_cards()], \
        [("text", "trk1", "SOLID BASS"), ("expect", "page", 1), ("expect", "sel", 0)]


@scene("stage_4tracks", "home", "Stage, four tracks: a drum kit, the octave up, the selected track armed")
def _():
    return [page("stage"), head(chip="PROPHET", rec=1, octave=2), sel(0, 4),
            track(0, 1, "It's a Prophet", CORAL, flags=["armed"]), steps(0, "." * 16),
            track(1, 2, "SOLID BASS", AMBER, role="BASS"), steps(1, "x.x.x.x.x.x.x.x. .x.x.x.x.x.x.x.x"),
            track(2, 3, "SYNTH.LEAD 1", MINT, role="LEAD"), steps(2, "." * 16),
            track(3, 4, "808 KIT", LILAC, role="DRUMS", flags=["drum"]), steps(3, "." * 16),
            play(6, 6, 6, 6), scope(wave()), *stage_cards()], []


@scene("stage_2tracks", "stage_2tracks", "Stage, two tracks: the second selected, its role and pattern under its name")
def _():
    return [page("stage"), head(chip="FM6"), sel(1, 2),
            track(0, 1, "It's a Prophet", CORAL, role="T1"), steps(0, "." * 16),
            track(1, 2, "SOLID BASS", AMBER, role="BASS"), steps(1, "x.xx.x.x x.x.x.xx .x.x x..x x.x. x.x."),
            play(6, 6), notes((62, 1)), scope(wave())], []


@scene("stage_1track", "stage_1track", "Stage, one track: its card across the rows, the chord it played under it")
def _():
    return [page("stage"), head(chip="PROPHET"), sel(0, 1),
            track(0, 1, "It's a Prophet", CORAL, role="T1"), steps(0, "x.x.x.x.x.x.x.x.".replace("x", ".")),
            play(6), notes((57, 0), (60, 0), (64, 0)), scope(wave())], []


@scene("stage_knobs", "stage_filter", "Stage with a knob turning: the cards dropped in, the filter it shapes")
def _():
    return [page("stage"), head(chip="PROPHET"), sel(0, 3),
            track(0, 1, "It's a Prophet", CORAL), steps(0, "." * 16),
            track(1, 2, "SOLID BASS", AMBER), steps(1, "x.x.x.x.x.x.x.x."),
            track(2, 3, "SYNTH.LEAD 1", MINT), steps(2, "." * 16),
            *stage_cards(), picture("filter", [79, 38]), play(6, 6, 6), scope(wave())], \
        [("turn", "K1", 1), ("wait", 300), ("expect", "knobs_out", 1), ("expect", "hot", 0)]


@scene("wheel", "view", "HOME's second page: the chord wheel, the notes sounding on two tracks, the key")
def _():
    return [page("wheel"), head(chip="FM6", dots=2, dot=1), sel(0, 3),
            track(0, 1, "It's a Prophet", CORAL), track(1, 2, "SOLID BASS", AMBER), track(2, 3, "SYNTH.LEAD 1", MINT),
            scale(9, [0, 2, 3, 5, 7, 8, 10], "Natural minor"), notes((45, 0), (69, 1)), meter(90, 60, 30)], []


@scene("wheel_empty", "view_empty", "HOME's second page, nothing played yet: the key's name in the wheel")
def _():
    return [page("wheel"), head(chip="FM6", dots=2, dot=1), sel(0, 2),
            track(0, 1, "It's a Prophet", CORAL), track(1, 2, "SOLID BASS", AMBER),
            scale(9, [0, 2, 3, 5, 7, 8, 10], "Natural minor"), meter(127, 127)], []


@scene("scl", "scl", "SCL: the settings as a list, the octave with the scale lit under it")
def _():
    return [page("list"), head(chip="SCL", dots=2, dot=0), sel(0, 1), track(0, 1, "It's a Prophet", CORAL),
            listpage(4, 0, 0, 1), lrow(0, "Root", "C"), lrow(1, "Scale", "MIN", more=True), lrow(2, "Quantize", "OFF"),
            lrow(3, "Transpose", "0 st"), scale(0, [0, 2, 3, 5, 7, 8, 10], "Natural minor"),
            foot("It's a Prophet")], \
        [("text", "lrow1", "Scale"), ("expect", "page", 4)]


@scene("scales", "ref_scales", "SCL > SCALES: the family, the scales around the one picked, its card")
def _():
    return [page("scales"), head(chip="SCALES"), sel(1, 2), track(1, 2, "Pickle Pincher", AMBER),
            scales_top(23, 24, "EDO"), scrow(0, "36-tone equal", 1), scrow(1, "41-tone equal", 1),
            scrow(2, "53-tone equal", 1, "53 notes \xb7 octave", star=True), scrow(3, ""), scrow(4, "")], []


@scene("chord", "chord", "SCL's CHORD: its four knobs, the chord last played on the keys")
def _():
    return [page("knobs"), head(chip="CHORD", dots=2, dot=1), sel(0, 1), track(0, 1, "It's a Prophet", CORAL),
            card(0, "CHRD", "DIA7"), card(1, "VOIC", "Close"), card(2, "STRUM", "1/64"), card(3, "DIR", "Alt"),
            picture("chord", [59], [59, 62, 65, 69], ["Bm7b5", "LAST"]), foot("It's a Prophet")], []


@scene("tempo", "tempo", "SEQ > TEMPO: BPM, swing, the recording grid, the scope")
def _():
    return [page("knobs"), head(chip="TEMPO", dots=4, dot=1), sel(0, 1), track(0, 1, "It's a Prophet", CORAL),
            card(0, "BPM", "124"), card(1, "SWG", "12", "%"), card(2, "QNT", "OFF"), card(3, "", ""),
            picture("scope"), scope(wave()), foot("It's a Prophet")], \
        [("text", "kval0", "124")]


@scene("arp", "arp", "ARP: its knobs and the notes it plays")
def _():
    return [page("knobs"), head(chip="ARP", dots=2, dot=0), sel(0, 1), track(0, 1, "It's a Prophet", CORAL),
            card(0, "MODE", "OFF"), card(1, "RATE", "1/16"), card(2, "OCT", "1"), card(3, "GATE", "50", "%"),
            picture("arp", [0, 1], []), foot("It's a Prophet")], []


@scene("arp_up", "ref_arp", "ARP playing up over two octaves")
def _():
    return [page("knobs"), head(chip="ARP", transport=0, dots=2, dot=0), sel(1, 2), track(1, 2, "SAW BASS", AMBER),
            card(0, "MODE", "Up"), card(1, "RATE", "1/16"), card(2, "OCT", "2"), card(3, "GATE", "50", "%"),
            picture("arp", [1, 2], [48, 52, 55]), foot("SAW BASS")], []


@scene("fx_rack", "fx_rack", "FX: SHARED's rack, its B slot picked")
def _():
    return [page("rack"), head(chip="SHARED", dots=5, dot=2), sel(0, 1), track(0, 1, "It's a Prophet", CORAL),
            card(0, "FX", "Space", cv="knob"), card(1, "SLOT", "B"), card(2, "ROUTE", "Chain"), card(3, "MIX", "40", "%"),
            rack(0, "A", "CHORUS", "100 %", 127), rack(1, "B", "ROOM", "40 %", 51, ["selected"]),
            rack(2, "C", "", "100 %", 127, ["off"]), rack(3, "", "Return", "79 %", 100, ["return"])], []


@scene("fx_room", "fx_room", "FX: a slot's page, ROOM's tail and its TONE")
def _():
    return [page("knobs"), head(chip="ROOM", dots=5, dot=3), sel(0, 1),
            card(0, "MIX", "66", "%"), card(1, "SIZE", "47", "%"), card(2, "DECAY", "57", "%"), card(3, "TONE", "Full"),
            picture("fx", [7, 84, 60, 72, 64, 0]), foot("", kind=1)], []


@scene("glo", "layer_glo_active", "GLO held: the levels in each track's colour, the mutes and solos as keys")
def _():
    return [page("glo"), head(chip="", status="[GLO] SET"), sel(2, 4),
            track(0, 1, "It's a Prophet", CORAL), track(1, 2, "SOLID BASS", AMBER, flags=["muted"]),
            track(2, 3, "SYNTH.LEAD 1", MINT, flags=["solo"]), track(3, 4, "808 KIT", LILAC),
            card(0, "LEVEL", "-11.0", cv="dim", track=True), card(1, "LEVEL", "-4.0", cv="dim", track=True),
            card(2, "LEVEL", "-4.0", track=True), card(3, "LEVEL", "-4.0", cv="dim", track=True)], []


@scene("mixer", "mixer", "GLO's MIXER: a track a strip, its level, meter and pan")
def _():
    return [page("mixer"), head(chip="MIXER", rec=2), sel(0, 4),
            track(0, 1, "It's a Prophet", CORAL), track(1, 2, "SOLID BASS", AMBER, flags=["armed"]),
            track(2, 3, "SYNTH.LEAD 1", MINT, flags=["muted"]), track(3, 4, "808 KIT", LILAC, flags=["muted", "armed"]),
            mix(0, 110, "-6", "L20"), mix(1, 80, "-14", "R24", armed=True), mix(2, 60, "-24", "", muted=True),
            mix(3, 120, "-1", "", muted=True, armed=True), meter(80, 50, 0, 0)], []


@scene("patterns", "patterns", "SEQ > PATTERNS: four tracks' patterns, one queued, the song's sections")
def _():
    e = (".", (0, 0, 0))
    return [page("patterns"), head(chip="PATTERNS", transport=2, dots=5, dot=3, flags=1), sel(1, 4),
            track(0, 1, "It's a Prophet", CORAL), track(1, 2, "SOLID BASS", AMBER), track(2, 3, "SYNTH.LEAD 1", MINT),
            track(3, 4, "808 KIT", LILAC),
            grid(0, [("p", (0, 0, 0)), ("u", (8, 10, 6)), ("u", (10, 6, 8))] + [e] * 5),
            grid(1, [("p", (0, 0, 0)), e, e, e, ("uq", (8, 6, 10))] + [e] * 3),
            grid(2, [("p", (0, 0, 0))] + [e] * 7), grid(3, [("p", (0, 0, 0))] + [e] * 7),
            gridtop(0, 1, 1, "ABAC"), steps(0, "." * 16), steps(1, "." * 16), steps(2, "." * 16), steps(3, "." * 16),
            play(0, 0, 0, 0)], []


@scene("sheet", "sheet_stage", "Stage's sheet over it, dimmed: the sound's actions, Add and Remove track")
def _():
    return [page("stage"), head(chip="PROPHET"), sel(0, 2),
            track(0, 1, "It's a Prophet", CORAL), steps(0, "." * 16), track(1, 2, "SOLID BASS", AMBER),
            steps(1, "x.x.x.x.x.x.x.x."),
            *sheet("It's a Prophet", [("Favourite", "Off"), ("Undo last load", ""), ("Sound designer", ""),
                                      ("Add track", "T3"), ("Remove track", "T1", 1)], 3, "PROPHET")], \
        [("expect", "sheet", 1), ("text", "srow3", "Add track")]


@scene("save", "ref_sheet_project", "SAVE: a slot's sheet, Save here picked")
def _():
    return [page("stage"), head(chip="USER"), sel(0, 1), track(0, 1, "It's a Prophet", CORAL), steps(0, "." * 16),
            *sheet("01 \xb7 NIGHT DRIVE", [("Load", ""), ("Save here", ""), ("Rename", ""), ("Boot", "On"),
                                            ("Erase", "", 1)], 1, "saved")], []


@scene("ask", "confirm_user", "A question over the page: Overwrite?")
def _():
    return [page("stage"), head(chip="PROPHET"), sel(0, 2),
            track(0, 1, "It's a Prophet", CORAL), steps(0, "." * 16), track(1, 2, "SOLID BASS", AMBER),
            steps(1, "x.x.x.x.x.x.x.x."), ask("Overwrite u007?", "A VERY LONG")], \
        [("text", "ask", "Overwrite u007?")]


@scene("pick", "fx_rack_pick", "FX: KNOB 1 turning, the effects' picker under its card")
def _():
    return [page("rack"), head(chip="SHARED", dots=6, dot=2), sel(0, 1),
            card(0, "FX", "Colour", cv="knob", hot=True), card(1, "SLOT", "C"), card(2, "ROUTE", "Chain"),
            card(3, "MIX", "100", "%"), rack(0, "A", "CHORUS", "100 %", 127), rack(1, "B", "ROOM", "40 %", 51),
            rack(2, "C", "TAPE", "100 %", 127, ["selected"]), rack(3, "", "Return", "79 %", 100, ["return"]),
            pick(0, ["DRIVE", "CRUSH", "FORMANT", "TAPE", "SPIT"], 3, [3, 3, 4, 4, 4], fx=True)], []


def script(name):
    render, doc, fn = SCENES[name]
    msgs, checks = fn()
    out = [f"# {doc} (Melodee FM-1's render: {render}.png). Written by tests/scenes.py: change it there.",
           "wait 1300", "sysex " + " ".join(f"{b:02X}" for b in hello()), "wait 30", "expect host 1"]
    for m in msgs:
        for one in (m if isinstance(m[0], list) else [m]):
            out.append("sysex " + " ".join(f"{b:02X}" for b in one))
    out.append("wait 120")
    for c in checks:
        if c[0] == "text":
            out.append(f"text {c[1]} {c[2]}")
        elif c[0] == "expect":
            out.append(f"expect {c[1]} {c[2]}")
        elif c[0] == "turn":
            out.append(f"turn {c[1]} {c[2]}")
        elif c[0] == "wait":
            out.append(f"wait {c[1]}")
    out.append(f"shot v2_{name}.png")
    return "\n".join(out) + "\n"


def main():
    if "--list" in sys.argv:
        for n, (r, d, _) in SCENES.items():
            print(f"{n:16} {r:20} {d}")
        return
    for n in SCENES:
        (ROOT / "app" / "tests" / f"v2_{n}.txt").write_text(script(n))
    print(f"scenes: {len(SCENES)} -> app/tests/v2_*.txt")


if __name__ == "__main__":
    main()
