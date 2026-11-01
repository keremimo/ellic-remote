# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The remote's rewrites of the few lines in Melodee FM-1's own files that other people wrote (tools/sync_fm1.py
copies the rest as it is). Each replaces an exact passage, so a changed upstream fails the sync rather than slip past;
the sync checks that every other author's line falls inside one. The remote has no USB host (recursive's lines go
with it); the header's room for its pills, the knob acceleration and the nearest microtonal degree are written here
anew."""

ROOM = "236 - cw - (low ? 22 : 0) - hw"


class CUT(tuple):
    """a passage from start up to (not with) end, whatever is between"""
    def __new__(cls, start, end):
        return super().__new__(cls, (start, end))

REWRITES = {
    "src/ui_draw.c": [
        ("no USB host", """#if MELODEE_USB_HOST
    sig += uh.on ? (1u + uh.run) * 6151u : 0u;          /* the HOST tag: waiting, or a controller connected */
#endif
""", ""),
        ("no USB host", """#if MELODEE_USB_HOST
        if (uh.on) tag = "HOST";
#endif
""", ""),
        ("the octave pill where there is room left of the tags",
         f"""        if (octave) {{                                   /* selected MPC bank's octave, else the shared octave */
            int32_t w;
            b[0] = octave > 0 ? '+' : '-';
            fmt_int(b + 1, octave < 0 ? -octave : octave);
            w = text_w(TY_LB, b) + 12;
            if (x + 2 + w + 8 > {ROOM} - 6)
                octave = 0;                             /* the HOST tag is status: the optional pill gives way */
        }}
        if (octave) {{
            int32_t w;
            b[0] = octave > 0 ? '+' : '-';
            fmt_int(b + 1, octave < 0 ? -octave : octave);
            w = text_w(TY_LB, b) + 12;
            cv_rrect(x + 2, 4, w, 22, 6, T_MID, T_BG);""",
         f"""        int32_t room = {ROOM} - 6;   /* (the right end of what the pills may take: the tags' left) */
        if (octave) {{                                   /* selected MPC bank's octave, else the shared octave */
            int32_t w;
            b[0] = octave > 0 ? '+' : '-';
            fmt_int(b + 1, octave < 0 ? -octave : octave);
            w = text_w(TY_LB, b) + 12;
            if (x + w + 10 > room)                      /* (no room: no pill) */
                w = 0;
            if (w) {{
            cv_rrect(x + 2, 4, w, 22, 6, T_MID, T_BG);"""),
        ("the octave pill where there is room left of the tags (its end)",
         """            x += w + 8;
        }
        if (chain.running) {""",
         """            x += w + 8;
            }
        }
        if (chain.running) {"""),
        ("the song's row where there is room",
         f"""            if (x + 2 + text_w(TY_LB, b) + 6 <= {ROOM} - 6)""",
         """            if (text_w(TY_LB, b) + 8 <= room - x)"""),
        ("HOME's page dots where there is room",
         f"""            head_pages(x + 6, {ROOM});
        }} else if (x + 6 + 28 <= {ROOM} - 6) {{   /* HOME: its two pages (Stage, the chord view),""",
         """            head_pages(x + 6, room + 6);
        } else if (room - x >= 34) {                   /* HOME: its two pages (Stage, the chord view),"""),
        ("no USB host", """#if MELODEE_USB_HOST
        if (uh.on && !uh.run) tag_color = T_MID;
#endif
""", ""),
    ],
    "src/ui_menu.c": [
        ("no USB host", """ * USB HOST (the USB-C port hosts a USB-MIDI controller instead of talking to a computer; not saved; usb_host.c),
""", ""),
        ("no USB host", """#if MELODEE_USB_HOST
       MI_USB_HOST,
#endif
""", ""),
        ("no USB host", """#if MELODEE_USB_HOST
                                              "USB HOST",
#endif
""", ""),
        ("no USB host", """#if MELODEE_USB_HOST
                if (i == MI_USB_HOST) value = uh.want ? "ON" : "OFF";
#endif
""", ""),
        ("no USB host", """#if MELODEE_USB_HOST
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_USB_HOST) {
        /* KNOB 1 right ON, left OFF, OCT+ toggles. Not saved: the next boot is a device again (usb_host.c) */
        uh.want = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !uh.want);
        ui.force = 1; ok = 0;                           /* (the row is not part of the menu's signature) */
    }
#endif
""", ""),
    ],
    "src/ui_input.c": [
        ("the knob acceleration, the remote's own", """/* Knob acceleration, after Felucca 1.4 (#52), with fm1-x0x's curve (tuned on the hardware): a slow turn is one step a
 * detent, and a few clicks are one step each however quick. Only a spin speeds up: from the fourth read of a turn,
 * each under ACC_GAP ms a detent after the one before in the same direction, 3 steps a detent under 45 ms, 5 under
 * 25 ms and 8 under 12 ms, so a quick half turn sweeps 0..127. cap: the most for the value (accel, list_accel); a cap
 * over 8 (the tempo, the long lists) doubles them. MENU > KNOB ACCEL OFF (PREF_ACCEL_OFF) keeps every detent one step.
 * The main loop reads the knobs many times a frame (main.c), so a read holds one detent as a rule; a read of several is
 * timed per detent and counts once (a few detents read at once are not a spin). A pause or a reversal starts a new
 * turn, and the sign is always the detents'. *fast (if asked): this detent followed the previous one within ACC_GAP ms,
 * in the same direction (the browser waits for such a turn to rest).
 * ui.enc_t[role]: bits 0..23 the ms of its last read, bit 24 its direction (+1), 25..31 the reads of its turn */
#define ACC_GAP 60u
#define ACC_SPIN 4u
static int32_t accel_by(uint32_t role, int32_t s, uint32_t cap, uint32_t *fast)
{
    uint32_t now = fm1_ms & 0xFFFFFFu, st = ui.enc_t[role % NE], up = s > 0, run = st >> 25, a, dt, m = 1;
    if (fast)
        *fast = 0;
    if ((PREF_BITS & PREF_ACCEL_OFF) || (midi_panel_mask & (1u << (role % NE))) || !s)
        return s;
    a = (uint32_t)(s < 0 ? -s : s);
    dt = ((now - st) & 0xFFFFFFu) / a;                  /* ms per detent of this read */
    if (!st || ((st >> 24) & 1u) != up || dt >= ACC_GAP) {
        run = 1;                                        /* a new turn, or reversed */
    } else {
        if (fast)
            *fast = 1;
        run++;
        if (run >= ACC_SPIN)
            m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
        if (cap > 8u && m > 1u)
            m *= 2u;
        m = m > cap ? cap : m;
    }
    ui.enc_t[role % NE] = now | up << 24 | (run > 127u ? 127u : run) << 25;
    return s * (int32_t)m;
}
/* a value of range steps: none up to 24 (and a list of names passes 0), x3 at most under 100, x8, x16 over 150 */
static int32_t accel(uint32_t role, int32_t s, int32_t range)
{
    return range <= 24 ? s : accel_by(role, s, range < 100 ? 3u : range > 150 ? 16u : 8u, 0);
}""", """/* Knob acceleration (the remote's own): a knob turned slowly moves its value a step a detent, and so do its first few
 * quick detents; kept spinning the same way, a detent moves it more the sooner it follows the last one (1 + the ms
 * short of KNOB_QUICK, an eighth of them), up to cap steps (accel, list_accel), twice that over a long range (cap over
 * 8). A knob read with several detents at once is timed by the detent. MENU > KNOB ACCEL OFF, and a knob MIDI has,
 * keep a step a detent. *fast (if asked): the detent came within KNOB_QUICK ms of the last, the same way (the browser
 * waits for such a turn to stop). ui.enc_t[role]: its last read's ms (bits 0..23), its way (bit 24), the quick
 * detents so far (25..31). */
#define KNOB_QUICK 60u                                  /* ms a detent: quicker is a spin */
#define KNOB_WARM 3u                                    /* quick detents before it speeds up */
static int32_t accel_by(uint32_t role, int32_t s, uint32_t cap, uint32_t *fast)
{
    uint32_t k = role % NE, last = ui.enc_t[k], ms = fm1_ms & 0xFFFFFFu, way = s > 0 ? 1u : 0u, quick = last >> 25;
    uint32_t per, gain = 1;
    if (fast)
        *fast = 0;
    if (!s || (PREF_BITS & PREF_ACCEL_OFF) || ((midi_panel_mask >> k) & 1u))
        return s;
    per = ((ms - last) & 0xFFFFFFu) / (uint32_t)(s > 0 ? s : -s);
    if (last && (last >> 24 & 1u) == way && per < KNOB_QUICK) {
        if (fast)
            *fast = 1;
        if (++quick > KNOB_WARM) {
            gain = 1u + (KNOB_QUICK - per) / 8u;
            if (cap > 8u)
                gain += gain - 1u;
            if (gain > cap)
                gain = cap;
        }
    } else {
        quick = 0;
    }
    ui.enc_t[k] = ms | way << 24 | (quick < 127u ? quick : 127u) << 25;
    return s * (int32_t)gain;
}
/* a value's range: a step a detent up to 24 (a list of names gives 0); the most a detent 3 under 100, 16 over 150,
 * else 8 */
static int32_t accel(uint32_t role, int32_t s, int32_t range)
{
    if (range <= 24)
        return s;
    return accel_by(role, s, range > 150 ? 16u : range >= 100 ? 8u : 3u, 0);
}"""),
    ],
    "tools/gen_tables.py": [
        ("FM6's, SID's and the SID kit's tables: Dexed's, MSFA's and reSID's, not the remote's",
         """    L += fm6_tables()
    L += sid_tables()
    L += sid_kit_tables()
""", ""),
        ("FM6's, SID's and the SID kit's tables (their makers)", CUT("SID_CLOCK = 985248", 'if __name__ == "__main__":'), ""),
    ],
    "assets/fonts/melodee_pixel.txt": [
        ("its licence here", "drawn for Melodee, GPL-3.0-only like\n# the firmware.", "drawn for Melodee, MPL-2.0 here\n# (LICENSING.md)."),
    ],
    "src/microtonal.h": [
        ("the nearer degree", """    if (s->pitch[degree + 1] - relative < relative - s->pitch[degree])
        degree++;""", """    degree += 2 * relative > s->pitch[degree] + s->pitch[degree + 1];   /* past the middle: the next one */"""),
    ],
}


def apply(path, text):
    """path's rewrites applied: (the text, the spans of the original they replaced)"""
    spans = []
    for why, old, new in REWRITES.get(path, []):
        if isinstance(old, CUT):
            at, end = text.find(old[0]), text.find(old[1])
            if at < 0 or end < at or text.find(old[0], at + 1) >= 0:
                raise SystemExit(f"fm1_overlay: {path}: the passage for '{why}' is not there once: upstream changed")
            old = text[at:end]
        at = text.find(old)
        if at < 0 or text.find(old, at + 1) >= 0:
            raise SystemExit(f"fm1_overlay: {path}: the passage for '{why}' is not there once: upstream changed")
        spans.append((at, at + len(old)))
        text = text[:at] + "\0" * len(old) + text[at + len(old):]   # (later passages keep their offsets)
    for (a, b), (why, old, new) in sorted(zip(spans, REWRITES.get(path, [])), key=lambda e: e[0], reverse=True):
        text = text[:a] + new + text[b:]
    return text, spans
