#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Copy Melodee FM-1's own sources into fm1/, as they are at a commit of its repository (Felucca, branch nightly):
its pages, controls, model and sequencer, which the remote runs whole (app/unit/unit.c).

  tools/sync_fm1.py [--felucca ~/code/personal/Felucca] [--commit REV]

Only Kerem's and Leo Kuroshita's lines come: every file is checked line by line (git blame) and by what it says it
ports. Other people's lines are rewritten by tools/fm1_overlay.py (checked: a changed upstream fails it), and the
files that port other people's code (FM6's Dexed, SID's reSID, the 909's 9W9, SLOOP's effects, CrispyZebra, DaisySP,
Rings, MAME's uPD933) or carry others' data (Casio's CZ-1 tones, Sequential's Prophet programs) never come: the
remote has stand-ins of its own under the same names (app/unit/stand_in/), as it plays none of those engines.
The licence line becomes MPL-2.0 (LICENSING.md: both authors' grant); the copyright lines stay. fm1/SOURCE records
the commit and the files.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "fm1"
sys.path.insert(0, str(ROOT / "tools"))
import fm1_overlay  # noqa: E402

KEREM = {"git@keremk.be", "nyaa@live.com"}
LEO = {"kurogedelic@gmail.com"}

# firmware/src: what app/unit/unit.c compiles, and what those include: the pages and the controls (the model's types
# and the pages' tables, core.h and params.c); no engine, voice, mixer, effect, sequencer, pattern store, take, song,
# project or preset of Melodee FM-1's: Melodee's (app/unit/remote_*.c), a few pure functions excerpted below
SRC = """
build_options.h performance_options.h audio_profile.h libc.c gfx.c core.h dsp.c eng_midi.c params.c
recording_preferences.h midi_knobs.h step_snapshot.h recorded_note.h chord.c chord_auto.c chord_melody.c panel.c ui.c
favorites.c screen.c scale_picker.c scales.h microtonal.h tuning.h browse.c seq_edit.c seq_notes.c seq_undo.c
note_tools.c sysex_library.c ui_song.c ui_song_sound.c ui_boot.c icons.c ui_graph.c ui_draw.c ui_sections.c
ui_stage.c ui_view.c ui_browser.c ui_patterns.c ui_pages.c ui_list.c ui_popup.c ui_slots.c ui_motion.c ui_scales.c
ui_menu.c ui_input.c ui_name.c ui_new.c ui_midi_knobs.c ui_layer.c wire_bits.h
""".split()
# firmware/hal: what those include of it
HAL = []
# tools/ and assets/: the generated headers' makers (fm1.toml gen) and what they read
TOOLS = "gen_tables.py gen_aa_font.py aa_raster.py gen_icons.py gen_aa_keycaps.py gen_ui_palettes.py".split()
ASSETS = ["assets/fonts/Rubik[wght].ttf", "assets/fonts/melodee_pixel.txt", "assets/icons.json"]

# never: other people's code or data (the reason each)
NEVER = {
    "fm6_core.c": "Dexed (GPL-3.0-or-later) and MSFA restated", "eng_fm6.c": "Dexed", "eng_fm6_rom.h": "DX7 ROM",
    "eng_sid.c": "reSID restated", "drum_sid.c": "eng_sid.c's reSID model",
    "x0x/drum909.c": "9W9 (Charles Vestal) and ER-99", "x0x/drum909.h": "9W9", "x0x/drum909_dsp.h": "9W9",
    "x0x/x0x_param.h": "9W9", "x0x/fastmath.h": "9W9's maths",
    "punch.c": "SLOOP (isod89)", "fx_sloop_insert.h": "SLOOP", "fx_sloop_reverb.h": "SLOOP",
    "scratch.h": "SLOOP (Andre Rezemini)", "eng_phase.c": "CrispyZebra", "phys_dsp.c": "DaisySP",
    "phys_symp.c": "Rings", "cz_native.c": "MAME uPD933", "eng_phys.c": "DaisySP and Rings",
}
# what a port names (case as written: "rings" and "resident" are Melodee's own words); dsp.c only unpacks tables
PORTS = re.compile(r"Dexed|MSFA|reSID(?!'s waveforms, CZ-1)|9W9|ER-99|SLOOP|isod89|CrispyZebra|DaisySP|Rings by|uPD933|MAME")
TRIVIAL = re.compile(r"^\s*(#\s*(endif|else)\b.*|[{}();,\s]*|)$")   # nobody's: no expression in it


# excerpts: the few definitions of Melodee FM-1's model files its pages and keys use as they are (pure functions of the
# model: the keyboard's layout and mapping, the song's rows, the matrix's names), from files the remote leaves out
# (the sequencer, the song chain, the voices), each excerpt its own file with the source's header
EXCERPTS = {
    "src/excerpt_seq.c": ("seq.c", "the keys, the scale, the tracks' channels (seq.c)",
                          ["KB_SILENT", "trk_index", "trk_midi_ch", "drum_track", "key_black", "key_place", "GK_ACC",
                           "scale_mask", "scale_count", "mpc_degree", "scale_degree_map", "scale_map", "kb_map",
                           "rec_on", "rec_any", "step_samples", "recording_quant_position"]),
    "src/excerpt_recording.c": ("recording.c", "a take's notes as the pages read them (recording.c)",
                                ["recording_owner", "recording_view", "recording_present", "recording_active",
                                 "recording_prefix", "recording_loop", "recording_raw_on", "recording_grid_step"]),
    "src/excerpt_song_chain.c": ("song_chain.c", "the song's rows as the pages read them (song_chain.c)",
                                 ["chain_defaults", "chain_origin", "chain_bank", "chain_block_end", "chain_fx_count",
                                  "seq_steps"]),
    "src/excerpt_voice.c": ("voice.c", "the LFO's shape and the voice mode, as the pages draw them (voice.c)",
                            ["lfo_wave", "trk_vmode"]),
    "src/excerpt_mod.c": ("mod.c", "the modulation matrix's names (mod.c)",
                          ["MS_OFF", "NMSLOT", "MD_OFF", "MD_ENGINE", "mod_dst_name"]),
    "src/excerpt_midi_track.c": ("midi_track.c", "a MIDI Track's channel (midi_track.c)", ["midi_track_channel"]),
}


def excerpt(text, name):
    """the top-level definition of name in text (a function, a table or variable, an enum, a #define): (start, end)"""
    lines = text.split("\n")
    starts = [0]
    for line in lines:
        starts.append(starts[-1] + len(line) + 1)
    for i, line in enumerate(lines):
        if re.match(r"#define\s+" + name + r"\b", line):
            j = i
            while lines[j].endswith("\\"):
                j += 1
            return starts[i], starts[j + 1]
        if not line or line[0] in " \t#/*}":
            continue
        if line.startswith("enum"):
            if not re.search(r"\b" + name + r"\b", text[starts[i]:text.find("}", starts[i])]):
                continue
            function = False
        elif re.search(r"\b" + name + r"\s*\(", line) and line.startswith("static"):
            function = not re.search(r"\b" + name + r"\s*\([^)]*\)\s*;", line)   # (not a declaration)
            if not function:
                continue
        elif re.search(r"\b" + name + r"\b\s*(\[|=|;|,)", line) and line.startswith("static"):
            function = False
        else:
            continue
        k, depth = starts[i], 0
        while k < len(text):                     # a function: to its closing brace; the rest: to its semicolon
            c = text[k]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0 and function:
                    k += 1
                    break
            elif c == ";" and depth == 0 and not function:
                k += 1
                break
            k += 1
        if text[k:k + 1] == "\n":
            k += 1
        a = i                                    # (a whole comment just above, of its own, comes with it)
        if a > 0 and lines[a - 1].rstrip().endswith("*/"):
            b = a - 1
            while b >= 0 and not lines[b].startswith("/*"):
                b -= 1
            if b >= 0 and all(lines[j].startswith((" *", "/*")) for j in range(b, a)):
                a = b
        return starts[a], k
    raise SystemExit(f"sync: excerpt {name}: not found")


def git(repo, *args):
    return subprocess.run(["git", "-C", str(repo), *args], check=True, capture_output=True, text=True).stdout


def foreign_lines(repo, commit, path):
    """the lines of path at commit by anyone but Kerem and Leo: [(n, text)]"""
    out, n, mail, lines = git(repo, "blame", "--line-porcelain", commit, "--", path), 0, "", []
    for line in out.splitlines():
        if line.startswith("author-mail "):
            mail = line[13:-1]
        elif line.startswith("\t"):
            n += 1
            if mail not in KEREM | LEO and not TRIVIAL.match(line[1:]):
                lines.append((n, line[1:]))
    return lines


def mpl(text, path):
    """the licence line MPL-2.0, and where the file came from"""
    old = "SPDX-License-Identifier: GPL-3.0-only"
    if old not in "\n".join(text.split("\n")[:4]) and not path.endswith(".ttf"):
        raise SystemExit(f"sync: {path}: no GPL-3.0-only SPDX line where expected")
    return text.replace(old, "SPDX-License-Identifier: MPL-2.0", 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--felucca", default=str(Path.home() / "code/personal/Felucca"))
    ap.add_argument("--commit", default="nightly")
    a = ap.parse_args()
    repo = Path(a.felucca)
    commit = git(repo, "rev-parse", a.commit).strip()
    files = [("firmware/src/" + f, "src/" + f) for f in SRC] + [("firmware/hal/" + f, "hal/" + f) for f in HAL] + \
            [("tools/" + f, "tools/" + f) for f in TOOLS] + [(f, f) for f in ASSETS]
    problems, written = [], []
    for src, dst in files:
        if src.split("firmware/src/")[-1] in NEVER:
            raise SystemExit(f"sync: {src} is never copied: {NEVER[src.split('firmware/src/')[-1]]}")
        data = subprocess.run(["git", "-C", str(repo), "show", f"{commit}:{src}"], check=True,
                              capture_output=True).stdout
        if dst.endswith(".ttf"):
            (OUT / dst).parent.mkdir(parents=True, exist_ok=True)
            (OUT / dst).write_bytes(data)
            written.append(dst)
            continue
        text = data.decode()
        head = "\n".join(text.split("\n")[:12])
        if PORTS.search(head):
            problems.append(f"{src}: says it ports others' code: {PORTS.search(head).group(0)}")
        starts = [0]
        for line in text.split("\n"):
            starts.append(starts[-1] + len(line) + 1)
        new, spans = fm1_overlay.apply(dst, text)
        for n, line in foreign_lines(repo, commit, src):
            if not any(a <= starts[n - 1] and starts[n] - 1 <= b for a, b in spans):
                problems.append(f"{src}:{n}: not Kerem's or Leo's: {line.strip()[:90]}")
        if dst == "tools/gen_tables.py" and re.search(r"reSID|Dexed|MSFA|sid_|fm6_", new):
            problems.append(f"{src}: still makes others' tables")
        text = new if dst.startswith("assets/") else mpl(new, src)
        (OUT / dst).parent.mkdir(parents=True, exist_ok=True)
        (OUT / dst).write_text(text)
        written.append(dst)
    for dst, (src, what, names) in EXCERPTS.items():
        path = "firmware/src/" + src
        text = git(repo, "show", f"{commit}:{path}")
        starts = [0]
        for line in text.split("\n"):
            starts.append(starts[-1] + len(line) + 1)
        foreign = [starts[n - 1] for n, _ in foreign_lines(repo, commit, path)]
        head = text[:text.index("*/") + 2]
        body = []
        for name in names:
            x0, x1 = excerpt(text, name)
            if any(x0 <= f < x1 for f in foreign):
                problems.append(f"{path}: excerpt {name} holds another author's lines")
            body.append(text[x0:x1].rstrip("\n"))
        out = mpl(head, path) + f"\n/* Excerpts of Melodee FM-1's {src} as they are (tools/sync_fm1.py): {what}. */\n" + \
              "\n".join(body) + "\n"
        (OUT / dst).write_text(out)
        written.append(dst)
    if problems:
        raise SystemExit("sync: refused\n  " + "\n  ".join(problems))
    (OUT / "SOURCE").write_text(
        f"Melodee FM-1 (Felucca, {a.commit}) at {commit}, copied by tools/sync_fm1.py: Kerem Kilic's and Leo\n"
        f"Kuroshita's lines only, under MPL-2.0 (LICENSING.md); tools/fm1_overlay.py's rewrites in place of others'.\n\n"
        + "\n".join(written) + "\n")
    print(f"sync: {len(written)} files from {commit[:10]} into {OUT.relative_to(ROOT)}/")


if __name__ == "__main__":
    main()
