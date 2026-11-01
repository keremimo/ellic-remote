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
    "src/core.h": [
        ("the tracks after the parts: MIDI Tracks by their place unless a build says otherwise (the remote: every track a part)",
         """#define NTRK (NPART + NMIDI)     /* tracks: the parts, then the MIDI-only ones (a project's first NPART on its FUN wire) */""",
         """#define NTRK (NPART + NMIDI)     /* tracks: the parts, then the MIDI-only ones (a project's first NPART on its FUN wire) */
#ifndef MELODEE_MIDI_PLACES
#define MELODEE_MIDI_PLACES 1    /* the tracks after the parts are MIDI Tracks by their place (0: every track a part) */
#endif
#ifndef MELODEE_TAKES
#define MELODEE_TAKES 1          /* takes are recorded here, their notes naming their pattern in seven bits */
#endif"""),
        ("every track's patterns as a build says (the remote: 16 each)",
         """#define trk_npat(k) ((uint32_t)(k) < NPART ? NPAT : NPAT_MIDI)   /* the patterns track k has */""",
         """#if MELODEE_MIDI_PLACES
#define trk_npat(k) ((uint32_t)(k) < NPART ? NPAT : NPAT_MIDI)   /* the patterns track k has */
#else
#define trk_npat(k) ((void)(k), NPAT)
#endif"""),
        ("seven bits name a take's pattern only where takes are recorded",
         """_Static_assert(NSTEP <= 128u && NTRK * NPAT <= 128u, "recorded-note identifiers (seven bits)");""",
         """_Static_assert(NSTEP <= 128u && (NTRK * NPAT <= 128u || !MELODEE_TAKES), "recorded-note identifiers (seven bits)");"""),
        ("a bit per track for up to 32 tracks",
         """    uint8_t rec;                 /* live recording armed: bit per track */
    uint8_t rec_wait, rec_pass;   /* waiting for loop zero, or recording since pattern start */
    uint8_t rec_latch;           /* explicit REC stays armed across pattern ends: bit per track */""",
         """    uint32_t rec;                /* live recording armed: bit per track */
    uint32_t rec_wait, rec_pass; /* waiting for loop zero, or recording since pattern start */
    uint32_t rec_latch;          /* explicit REC stays armed across pattern ends: bit per track */"""),
        ("a MIDI slot by its place only where tracks are MIDI Tracks by place",
         """static inline int trk_midi_slot(uint32_t k) { return k >= NPART && k < NTRK; }""",
         """static inline int trk_midi_slot(uint32_t k) { return MELODEE_MIDI_PLACES && k >= NPART && k < NTRK; }"""),
        ("a bit per track for up to 32 tracks",
         """static volatile uint8_t track_solo;""", """static volatile uint32_t track_solo;"""),
    ],
    "src/eng_midi.c": [
        ("a MIDI Track by its place only where tracks are MIDI Tracks by place",
         """(t >= &trk[NPART] && t < &trk[NTRK]); }""", """(MELODEE_MIDI_PLACES && t >= &trk[NPART] && t < &trk[NTRK]); }"""),
    ],
    "src/ui.c": [
        ("a MIDI Track by its place only where tracks are MIDI Tracks by place",
         """    if (t >= &trk[NPART] && t < &trk[NTRK]) { ei = ENGI_MIDI; pi = 0; }""",
         """    if (MELODEE_MIDI_PLACES && t >= &trk[NPART] && t < &trk[NTRK]) { ei = ENGI_MIDI; pi = 0; }"""),
    ],
    "src/ui_layer.c": [
        ("a bit per track for up to 32 tracks", """    track_solo = (uint8_t)s;""", """    track_solo = s;"""),
    ],
    "src/ui_input.c": [
        ("a bit per track for up to 32 tracks (REC)", """static void rec_toggle(uint32_t latch)
{
    uint8_t bit = (uint8_t)(1u << song.sel);""", """static void rec_toggle(uint32_t latch)
{
    uint32_t bit = 1u << song.sel;"""),
        ("a bit per track for up to 32 tracks (REC + PLAY)", """static void rec_play(void)
{
    uint8_t bit = (uint8_t)(1u << song.sel);""", """static void rec_play(void)
{
    uint32_t bit = 1u << song.sel;"""),
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

TRKNUM = "a track's number in one or two digits (the remote: up to 32 tracks)"
TRACK_NUMBERS = {
    "src/ui.c": [
        (TRKNUM, """    b[0] = (char)('0' + p); b[1] = 0; return 1;
}""", """    b[0] = (char)('0' + p); b[1] = 0; return 1;
}
/* track k's number, 1..32, into b (3 bytes); its length (as pat_num: the remote's tracks go past 9) */
static uint32_t trk_num(char *b, uint32_t k)
{
    k = k % NTRK + 1u;
    if (k > 9u) { b[0] = (char)('0' + k / 10u); b[1] = (char)('0' + k % 10u); b[2] = 0; return 2; }
    b[0] = (char)('0' + k); b[1] = 0; return 1;
}"""),
        (TRKNUM, """    b[1] = (char)('1' + trk_index(t));
    ui_say(redo ? "REDO " : "UNDO ", b);""", """    trk_num(b + 1, trk_index(t));
    ui_say(redo ? "REDO " : "UNDO ", b);"""),
    ],
    "src/ui_stage.c": [
        (TRKNUM, """d[3] = {(char)('1' + k), 0, 0}, role[16];""", """d[3] = {0}, role[16];
    trk_num(d, k);"""),
        (TRKNUM, """        char tn[3] = {'T', (char)('1' + k), 0};""", """        char tn[4] = {'T', 0, 0, 0};
        trk_num(tn + 1, k);"""),
    ],
    "src/ui_browser.c": [
        (TRKNUM, """    char d[2] = {(char)('1' + song.sel % NTRK), 0}, r[20];""", """    char d[3], r[20];
    trk_num(d, song.sel);"""),
    ],
    "src/ui_draw.c": [
        (TRKNUM, """        str_cpy(b + str_len(b), ui.ptc_to != ui.ptc_trk ? " to T1 " : " to ", 8);
        if (ui.ptc_to != ui.ptc_trk)
            b[str_len(b) - 2] = (char)('1' + ui.ptc_to % NTRK);""", """        str_cpy(b + str_len(b), ui.ptc_to != ui.ptc_trk ? " to T" : " to ", 8);
        if (ui.ptc_to != ui.ptc_trk) {
            trk_num(b + str_len(b), ui.ptc_to);
            str_cpy(b + str_len(b), " ", 2);
        }"""),
        (TRKNUM, """        char d[2] = {(char)('1' + song.sel % NTRK), 0};
        cv_rrect(4, H_FOOT / 2 - 11, 22, 22, 6, T_THEME, T_BG);""", """        char d[3];
        trk_num(d, song.sel);
        cv_rrect(4, H_FOOT / 2 - 11, 22, 22, 6, T_THEME, T_BG);"""),
        (TRKNUM, """        str_cpy(a, "CLEAR TRACK 1?", 24);
        a[12] = (char)('1' + k % NTRK);""", """        str_cpy(a, "CLEAR TRACK ", 24);
        str_cpy(a + 12 + trk_num(a + 12, k), "?", 2);"""),
        (TRKNUM, """        str_cpy(b, "Track 1 \\xB7 notes and motion", 32);
        b[6] = (char)('1' + k % NTRK);""", """        str_cpy(b, "Track ", 32);
        str_cpy(b + 6 + trk_num(b + 6, k), " \\xB7 notes and motion", 24);"""),
        (TRKNUM, """        str_cpy(b, ui.ptc_to != ui.ptc_trk ? "Track 1 \\xB7 with T1 pattern " : "Track 1 \\xB7 with pattern ", 32);
        b[6] = (char)('1' + ui.ptc_to % NTRK);
        if (ui.ptc_to != ui.ptc_trk)
            b[16] = (char)('1' + ui.ptc_trk % NTRK);""", """        str_cpy(b, "Track ", 32);
        trk_num(b + 6, ui.ptc_to);
        str_cpy(b + str_len(b), ui.ptc_to != ui.ptc_trk ? " \\xB7 with T" : " \\xB7 with pattern ", 20);
        if (ui.ptc_to != ui.ptc_trk) {
            trk_num(b + str_len(b), ui.ptc_trk);
            str_cpy(b + str_len(b), " pattern ", 10);
        }"""),
        (TRKNUM, """        str_cpy(a, "CLEAR T1 MOTION?", 24); a[7] = (char)('1' + k % NTRK);""",
         """        str_cpy(a, "CLEAR T", 24); str_cpy(a + 7 + trk_num(a + 7, k), " MOTION?", 9);"""),
        (TRKNUM, """        str_cpy(a, "REMOVE TRACK 1?", 24);
        a[13] = (char)('1' + k % NTRK);""", """        str_cpy(a, "REMOVE TRACK ", 24);
        str_cpy(a + 13 + trk_num(a + 13, k), "?", 2);"""),
        (TRKNUM, """        str_cpy(a, "CLEAR T1 SEQUENCE?", 24);     /* the header (T1..T4) is hidden */
        a[7] = (char)('1' + k % NTRK);""", """        str_cpy(a, "CLEAR T", 24);                 /* the header (T1..T4) is hidden */
        str_cpy(a + 7 + trk_num(a + 7, k), " SEQUENCE?", 11);"""),
    ],
    "src/ui_layer.c": [
        (TRKNUM, """        char d[2] = {(char)('1' + g + i), 0};
        cv_rrect(x, wy, kw, h - wy - 6, 4, f, T_PANEL);""", """        char d[3];
        trk_num(d, g + i);
        cv_rrect(x, wy, kw, h - wy - 6, 4, f, T_PANEL);"""),
        (TRKNUM, """        char n[2] = {(char)('1' + g + i), 0};
        if (g + i >= trk_n()) {""", """        char n[3];
        trk_num(n, g + i);
        if (g + i >= trk_n()) {"""),
    ],
    "src/ui_input.c": [
        (TRKNUM, """        char b[12] = "1 CLEARED";
        b[0] = (char)('1' + k % NTRK);""", """        char b[12];
        str_cpy(b + trk_num(b, k), " CLEARED", 9);"""),
        (TRKNUM, """        str_cpy(b, "T1 ", sizeof b);
        b[1] = (char)('1' + song.sel % NTRK);
        str_cpy(b + 3, HS_NAME[hs], sizeof b - 3u);""", """        str_cpy(b, "T", sizeof b);
        str_cpy(b + 1 + trk_num(b + 1, song.sel), " ", 2);
        str_cpy(b + str_len(b), HS_NAME[hs], sizeof b - str_len(b));"""),
    ],
    "src/ui_patterns.c": [
        (TRKNUM, """    char n[2] = {(char)('1' + k), 0};
    h = fnv1(""", """    char n[3];
    trk_num(n, k);
    h = fnv1("""),
    ],
    "src/ui_popup.c": [
        (TRKNUM, """    b[0] = 'T';
    b[1] = (char)('1' + k % NTRK);
    b[2] = 0;""", """    b[0] = 'T';
    trk_num(b + 1, k);"""),
        (TRKNUM, """    char b[20] = "T1 / Section "; b[1] = (char)('1' + song.sel);""",
         """    char b[20] = "T"; str_cpy(b + 1 + trk_num(b + 1, song.sel), " / Section ", 12);"""),
        (TRKNUM, """        str_cpy(nm, "T1", sizeof nm);
        nm[1] = (char)('1' + song.sel % NTRK);
        sheet_open("Pattern", nm, SHEET_PATTERN, NELEM(SHEET_PATTERN));""", """        str_cpy(nm, "T", sizeof nm);
        trk_num(nm + 1, song.sel);
        sheet_open("Pattern", nm, SHEET_PATTERN, NELEM(SHEET_PATTERN));"""),
        (TRKNUM, """        str_cpy(nm, "T1", sizeof nm);
        nm[1] = (char)('1' + song.sel % NTRK);
        const song_sound_t *sound""", """        str_cpy(nm, "T", sizeof nm);
        trk_num(nm + 1, song.sel);
        const song_sound_t *sound"""),
        (TRKNUM, """title[] = "T1 PRESET";""", """title[12] = "T";"""),
        (TRKNUM, """    title[1] = (char)('1' + song.sel);""", """    str_cpy(title + 1 + trk_num(title + 1, song.sel), " PRESET", 8);"""),
    ],
}
for f, entries in TRACK_NUMBERS.items():
    REWRITES.setdefault(f, []).extend(entries)


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
