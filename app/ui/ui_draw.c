/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments: his Felucca code, used under MPL-2.0
 * with his permission (hugelton/felucca-bsp#4) */
/* Melodee FM-1's UI drawing (its firmware/src/ui_draw.c), ported to Melodee Remote: the header, the four knobs' cells
 * (the strip), the sound bar and the toast, with their rolling digits. What they show is Melodee's (the mirror,
 * ui_base.c V) where Melodee FM-1 reads its model; the drawing is Melodee FM-1's. The layout: SURF cells and panels on
 * BG, rounded corners, no rules, colours from the theme tokens only (gfx.c T_*); THEME is the selected track's
 * colour (labels, gauges, curves), values in TEXT. Type: the large type's roles (gfx.c TY_*). A value too wide for its
 * cell is set a role smaller (never under the label face); free text (names, messages) is ellipsised. Part of app.c */

#define COL_W CARD_W                                  /* a knob's cell: 57 x 48 at x 2 + 60 c, y 32 */
#define COL_H CARD_H

/* ---------------------------------------------------- rolling digits --- */
/* A number that changes rolls its changed digits like a slot machine; only the drawing: the value, the gauge,
 * the hot colour and the sound change at once. Up: the old digit leaves upward and the new one comes from below;
 * down: the reverse. ROLL_FRAMES UI frames on an integer ease-out (ROLL_EASE: cubic, over half-way in a quarter of the
 * time, no overshoot); the ones digit first, each place one frame later, all ending together on a frame equal to the
 * static render. Clipped to the value strip, which alone is redrawn while it rolls (a card: rows ROLL_Y.., the header:
 * the BPM's columns). Characters other than digits never roll. A change during a roll retargets it: it restarts from
 * the value it was going to. Shown at once (no roll): a change within ROLL_SNAP frames of the last one, another shape
 * (length, sign, a non-digit, a name), another label or unit, a track or palette change (ui.roll[].sig), ui.force (a
 * page change), a card's first draw. */
#define ROLL_FRAMES 9u
#define ROLL_SNAP 3u                                    /* frames (~45 ms) */
#define ROLL_BPM 4u                                     /* ui.roll[]: the four cards, then the header BPM */
#define ROLL_Y 20                                       /* a cell's value strip: rows 20..43 (the text face) */
#define ROLL_H 24
#define BPM_X 26
#define BPM_W 42                                        /* the header's BPM strip: 42 columns, every row (the text face) */
static uint16_t roll_bg;                                /* what a card's value lies on */
static const uint8_t ROLL_EASE[17] = {0, 45, 84, 118, 147, 172, 193, 210, 223, 234, 242, 247, 251, 253, 254, 255, 255};

static int roll_digit(char c) { return c >= '0' && c <= '9'; }
/* +1 / -1: b rolls in from a (same length, digits where a has digits, the rest equal and one of + - .),
 * the sign of b - a; 0: b is shown at once */
static int roll_dir(const char *a, const char *b)
{
    uint32_t i;
    int d = 0, neg = 0;
    for (i = 0; a[i] || b[i]; i++) {
        if (i + 1u >= sizeof ui.roll[0].from || roll_digit(a[i]) != roll_digit(b[i]))
            return 0;
        if (!roll_digit(a[i]) && (a[i] != b[i] || (a[i] != '-' && a[i] != '+' && a[i] != '.')))
            return 0;
        neg |= a[i] == '-';
        if (!d && a[i] != b[i])
            d = b[i] > a[i] ? 1 : -1;
    }
    return neg ? -d : d;
}
/* field k now shows b instead of a: roll (a retarget mid-roll), or snap */
static void roll_note(uint32_t k, const char *a, const char *b, int snap)
{
    uint32_t el = (uint8_t)(ui.frame - ui.roll[k].t0);
    int d = roll_dir(a, b);
    ui.roll[k].t0 = (uint8_t)ui.frame;
    ui.roll[k].from[0] = 0;
    if (snap || !d || el < ROLL_SNAP)
        return;
    str_cpy(ui.roll[k].from, a, sizeof ui.roll[k].from);
    ui.roll[k].dir = (int8_t)d;
}
/* s (the text face) at x, y with field k's roll: each character at its pen in s (the digits are tabular and never
 * kerned, so the old value has the same pens); a rolling digit has moved o of the strip's h rows (ROLL_EASE, one frame
 * later per place from the right), clipped to the strip. The roll's last frame clears it: the static text. */
static int32_t roll_text(uint32_t k, int32_t x, int32_t y, const char *s, uint16_t fg)
{
    const aafont_t *f = TY_TX;
    uint32_t e = (uint8_t)(ui.frame - ui.roll[k].t0) + 1u, i, p = 0;
    const char *a = ui.roll[k].from;
    int32_t cy = k < ROLL_BPM ? y : 0, h = k < ROLL_BPM ? ROLL_H : H_HEAD;   /* (a card's value: at ROLL_Y) */
    uint16_t bg = k < ROLL_BPM ? roll_bg : T_BG;
    char t[8], ch[2] = {0, 0};
    if (e >= ROLL_FRAMES)
        ui.roll[k].from[0] = 0;
    if (!a[0])
        return cv_text_on(x, y, f, s, fg, bg);
    str_cpy(t, s, sizeof t);
    for (i = 0; s[i]; i++)
        p += (uint32_t)roll_digit(s[i]);
    cv_cy0 = (int16_t)(cy + cv_oy);                     /* the strip (the static characters lie inside it) */
    cv_cy1 = (int16_t)(cy + cv_oy + h);
    cv_scroll = 1;
    for (i = 0; s[i]; i++) {
        int32_t px, ny = y;
        t[i] = 0;
        px = x + text_w(f, t);                          /* (its pen: no digit kerns) */
        t[i] = s[i];
        p -= (uint32_t)roll_digit(s[i]);
        if (s[i] != a[i]) {                             /* p: the places right of it */
            int32_t o = 0, dir = ui.roll[k].dir;
            if (e > p)                                  /* (e < ROLL_FRAMES: the LUT index stays in 1..16) */
                o = (int32_t)((ROLL_EASE[((e - p) * 32u / (ROLL_FRAMES - p) + 1u) >> 1] * (uint32_t)h + 128u) >> 8);
            ch[0] = a[i];
            cv_text_on(px, y - dir * o, f, ch, fg, bg);
            ny = y + dir * (h - o);
        }
        ch[0] = s[i];
        cv_text_on(px, ny, f, ch, fg, bg);
    }
    cv_cy0 = 0;
    cv_cy1 = (int16_t)cv_h;
    cv_scroll = 0;
    return x + text_w(f, s);
}

/* the header (large type, 30 px): the transport (16 px; a song playing: its disc) at x 5, the BPM (the text face,
 * rolling) at x 26, the REC mark (filled: the selected track armed; a ring: another one), the octave shift as an
 * outlined pill ("+2") and the song's row, then the page's dots; at the right the chip (the label face when long)
 * naming where you are (Stage: the engine; a page: its title), in the track's colour, or a status or message in its
 * place (a layer: its name, "[GLO] SET"). All of it Melodee's (MEL_HEAD) */
/* the message (or the layer's name) does not fit the header from x: it goes under it as a toast (draw_toast) */
static const char *head_msg(void) { return ui.msg[0] ? ui.msg : !(V->head_flags & MEL_HF_STATUS_RED) ? V->status : ""; }
static int head_toast(int32_t x)
{
    const char *m = head_msg();
    uint32_t n;
    int32_t w = text_w(TY_TX, m), id = kc_tag(m, &n);
    if (id >= 0)                                        /* (a keycap first, "[FX] MEMORY": its pill, then the words) */
        w = kc_w((uint32_t)id) + KH_GAP + text_w(TY_TX, m + n + (m[n] == ' '));
    else if (m[0] && str_len(m) > 6u && m[str_len(m) - 1u] == ']')
        w += 40;                                        /* (a keycap: its pill wider than its word) */
    return m[0] && w > 236 - x;
}
/* the page's place in its family: a dot a page, the page's a pill in the track's colour (no room: "3/12") */
static void head_pages(int32_t x, int32_t x1, uint32_t n, uint32_t k)
{
    uint32_t i;
    if (n < 2u)
        return;
    if (x + (int32_t)n * 12 + 10 > x1 - 6) {            /* (no room for the dots; none for that either: nothing) */
        char b[8];
        fmt_int(b, (int32_t)k + 1);
        str_cpy(b + str_len(b), "/", 2);
        fmt_int(b + str_len(b), (int32_t)n);
        if (x + text_w(TY_LB, b) <= x1 - 4)
            cv_text_on(x, ty_y(H_HEAD / 2, TY_LB), TY_LB, b, T_MID, T_BG);
        return;
    }
    for (i = 0; i < n; i++) {                           /* a dot a page, the page's a pill */
        int32_t w = i == k ? 16 : 6;
        cv_rrect(x, H_HEAD / 2 - 3, w, 6, 3, i == k ? T_THEME : T_LINE, T_BG);
        x += w + 6;
    }
}
static void draw_head(void)
{
    char b[16], chip[MEL_CHIP_N + 1];
    int32_t x, octave = (int32_t)V->octave - 64;
    uint32_t msg = head_msg()[0] != 0, sig, moved = 0, red = (V->head_flags & MEL_HF_STATUS_RED) && V->status[0];
    const char *bpm = V->bpm;
    str_cpy(chip, V->chip, sizeof chip);
    sig = str_hash(V->transport * 3u + V->rec * 5u + (uint32_t)(octave + 8) * 11u + V->sel * 13131u + V->head_flags * 7u +
                   V->row * 104729u + V->dots * 31u + V->dot * 977u + ux.gen * 977u + V->page * 2654435761u +
                   red * 8191u, msg || red ? (red ? V->status : head_msg()) : chip);
    if (!str_eq(bpm, ui.roll_bpm)) {
        roll_note(ROLL_BPM, ui.roll_bpm, bpm, ui.force);
        str_cpy(ui.roll_bpm, bpm, sizeof ui.roll_bpm);
        moved = 1;
    } else if (ui.force) {
        ui.roll[ROLL_BPM].from[0] = 0;
    }
    if (!ui.force && sig == ui.head_sig) {
        if (ui.roll[ROLL_BPM].from[0] || moved) {       /* rolling, or snapped to the new value: the BPM's strip only */
            cv_begin(BPM_W, H_HEAD, T_BG);
            roll_text(ROLL_BPM, 0, ty_y(H_HEAD / 2, TY_TX), bpm, (V->head_flags & MEL_HF_BPM_HOT) ? T_ACCENT : T_TEXT);
            cv_blit(BPM_X, Y_HEAD);
        }
        return;
    }
    ui.head_sig = sig;
    cv_begin(240, H_HEAD, T_BG);
    cv_icon_mid(5, H_HEAD / 2, 16, V->transport == MEL_TR_SONG ? ICON_X_SONG : V->transport ? ICON_X_PLAY : ICON_X_STOP,
                V->transport ? T_TEXT : T_MID, T_BG);
    roll_text(ROLL_BPM, BPM_X, ty_y(H_HEAD / 2, TY_TX), bpm, (V->head_flags & MEL_HF_BPM_HOT) ? T_ACCENT : T_TEXT);
    x = BPM_X + BPM_W;                                  /* (past the widest BPM) */
    if (V->rec) {                                       /* (16 px, as the transport) */
        cv_icon_mid(x, H_HEAD / 2, 16, V->rec == MEL_REC_ON ? ICON_X_REC : ICON_X_REC_O, T_REC, T_BG);
        x += 18;
    }
    if (msg && head_toast(x + 6))                       /* (too long for the header: a toast under it, draw_toast; the
                                                         * header as it is) */
        msg = 0;
    if (red && !ui.msg[0]) {                            /* ERASING, WAIT LOOP */
        cv_text_r(236, ty_y(H_HEAD / 2, TY_TX), TY_TX, V->status, T_REC, T_BG);
    } else if (msg) {                                   /* a message, or the layer's name (may hold a keycap) */
        cv_free_hint(x + 6, ty_y(H_HEAD / 2, TY_TX), head_msg(), T_TEXT, T_BG, 236 - x - 6);
    } else {
        const aafont_t *cf = TY_TX;                     /* (where you are: plain words in the track's colour;
                                                         * PATTERNS plain, the grid is in colour) */
        int32_t cw = text_w(cf, chip) + 4;
        if (x + 6 + cw > 236) {                         /* (a long page name: the label face) */
            cf = TY_LB;
            cw = text_w(cf, chip) + 4;
        }
        if (octave) {                                   /* the keys' octave, an outlined pill */
            int32_t w;
            b[0] = octave > 0 ? '+' : '-';
            fmt_int(b + 1, octave < 0 ? -octave : octave);
            w = text_w(TY_LB, b) + 12;
            cv_rrect(x + 2, 4, w, 22, 6, T_MID, T_BG);
            cv_rrect(x + 3, 5, w - 2, 20, 5, T_BG, T_MID);
            cv_text_c(x + 2 + w / 2, ty_y(H_HEAD / 2, TY_LB), TY_LB, b, T_TEXT, T_BG);
            x += w + 8;
        }
        if (V->row) {                                   /* the song's row (where there is room) */
            str_cpy(b, "ROW ", 8);
            fmt_int(b + 4, (int32_t)V->row);
            if (x + 2 + text_w(TY_LB, b) + 6 <= 236 - cw - 6)
                x = cv_text_on(x + 2, ty_y(H_HEAD / 2, TY_LB), TY_LB, b, T_MID, T_BG) + 6;
        }
        head_pages(x + 6, 236 - cw, V->dots, V->dot);   /* a page: where it is in its family; HOME: its two pages */
        cv_text_r(236, ty_y(H_HEAD / 2, cf), cf, chip, (V->head_flags & MEL_HF_CHIP_DIM) ? T_MID : T_THEME, T_BG);
    }
    cv_blit(0, Y_HEAD);
}
/* full redraw: the strips (header, cards, panel, footer; Stage: its panel and lanes) cover the rest; only the BG
 * between them is filled */
static void draw_frame(int stage)
{
    uint32_t i;
    if (stage) {                                        /* Stage, HOME's second page: their parts cover what they draw */
        lcd_fill(0, H_HEAD, 240, 240u - H_HEAD, T_BG);
        return;
    }
    lcd_fill(0, H_HEAD, 240, Y_LABEL - H_HEAD, T_BG);
    lcd_fill(0, Y_SEP_END, 240, Y_GRAPH - Y_SEP_END, T_BG);
    lcd_fill(0, Y_GRAPH + H_GRAPH, 240, Y_FOOT - Y_GRAPH - H_GRAPH, T_BG);
    lcd_fill(0, Y_LABEL, (uint32_t)CARD_X(0), CARD_H, T_BG);
    for (i = 0; i < 4u; i++)                            /* right of each card */
        lcd_fill((uint32_t)(CARD_X(i) + CARD_W), Y_LABEL, i < 3u ? (uint32_t)(CARD_X(i + 1u) - CARD_X(i) - CARD_W) :
                 240u - (uint32_t)(CARD_X(i) + CARD_W), CARD_H, T_BG);
}

/* ------------------------------------------------------------- the cards --- */
/* one card = one knob: LABEL (its knob's colour) / value unit, redrawn only when it changed. The value is the text face,
 * or the label face when too wide (engine names, long names); the unit (the label face) after it. The knob just turned
 * (hot): the value in its knob's colour, a bar under it. vc: the value's colour (T_TEXT, T_DIM inactive, T_ACCENT) */
/* A binary value remains readable without touching its encoder. */
static int binary_value(const char *val)
{
    return str_eq(val, "ON") || str_eq(val, "On") ? 1 : str_eq(val, "OFF") || str_eq(val, "Off") ? 0 : -1;
}
static void binary_badge(int32_t xc, int32_t y, int32_t h, const char *val, uint16_t bg)
{
    int on = binary_value(val);
    int32_t w = text_w(TY_LB, on ? "ON" : "OFF") + 12;
    uint16_t fill = on ? T_THEME : T_SURF;
    cv_rrect(xc - w / 2, y, w, h, 6, on ? T_THEME : T_LINE, bg);
    if (!on) cv_rrect(xc - w / 2 + 1, y + 1, w - 2, h - 2, 5, fill, T_LINE);
    cv_text_c(xc, ty_y(y + h / 2, TY_LB), TY_LB, on ? "ON" : "OFF", on ? T_INK : T_MID, fill);
}
static uint8_t stage_drop_rows;                         /* Stage's knobs dropping in: the cards' bottom rows only, 0 all */
static void col_old_value(uint32_t c, char *ov)          /* the value card c drew before (in its cache key), 16 bytes */
{
    const char *k = ui.col[c];
    uint32_t i = 0;
    while (*k && *k != '|')
        k++;
    while (*k && k[1] && k[1] != '|' && i + 1u < 16u)
        ov[i++] = *++k;
    ov[i] = 0;
}
static int32_t card_y = Y_LABEL;                        /* the strip's row */
static uint16_t card_tc[4];                             /* a cell's label colour (0: the knob's; the GLO layer: each's) */
/* a knob's words as the mockups write them (ui_pages.c pv_words): a name value in sentence case ("SAW" -> "Saw";
 * numbers and notes as they are), a unit in lower case ("STEP" -> "steps"); its label as the page has it */
static void pv_words(const char *label, const char *val, const char *unit, char *lb, char *v, char *u)
{
    uint32_t i, word = val[0] >= 'A' && val[0] <= 'Z' && val[1];
    str_cpy(lb, label, 12);
    for (i = 0; val[i] && word; i++)                    /* (a name: letters, spaces and signs only) */
        word = !(val[i] >= '0' && val[i] <= '9');
    if (word && binary_value(val) < 0) menu_words(v, val, 16);
    else str_cpy(v, val, 16);
    str_cpy(u, unit, 8);
    if (str_eq(unit, "STEP")) {
        str_cpy(u, str_eq(val, "1") ? "step" : "steps", 8);
    } else if (unit[0] && unit[1] && !str_eq(unit, "dB") && !str_eq(unit, "Hz") && !str_eq(unit, "kHz")) {
        for (i = 0; u[i]; i++)
            if (u[i] >= 'A' && u[i] <= 'Z') u[i] = (char)(u[i] + 32);
    }
}
/* a value as a list or a card writes it (ui_draw.c pv_words): a name in sentence case ("SAW" -> "Saw"), numbers, units
 * and notes as they are */
static void gen_words(char *d, const char *v, uint32_t n)
{
    uint32_t i, word = v[0] >= 'A' && v[0] <= 'Z' && v[1] && binary_value(v) < 0;
    for (i = 0; v[i] && word; i++)
        word = !(v[i] >= '0' && v[i] <= '9');
    if (word)
        menu_words(d, v, n);
    else
        str_cpy(d, v, n);
}
/* a label as its cell shows it: in full when it fits (55 px), else its short form */
static const struct { const char *k, *s; } CELL_SHORT[] = {
    {"STROKE", "DIR"}, {"PRIORITY", "PRIO"}, {"VEL AMP", "V.AMP"}, {"VEL FLT", "V.FLT"}, {"RELEASE", "REL"}, {"ENV AMT", "ENV"},
    {"VINTAGE", "VINT"}, {"PULSE A", "PLS A"}, {"PULSE B", "PLS B"}, {"WIDTH A", "WID A"}, {"WIDTH B", "WID B"},
    {"INITIAL", "INIT"}, {"SQUARE", "SQR"}, {"VOICES", "VOICE"}, {"BUDGET", "BUDG"}, {"RETURN", "RET"},
    {"DETUNE", "DTUNE"}, {"CUTOFF", "CUT"}, {"UNISON", "UNI"}, {"FREQ A", "FRQ A"}, {"FREQ B", "FRQ B"},
    {"AT LFO", "ATLFO"}, {"AT FLT", "ATFLT"}, {"FILTER", "FILT"}, {"FINE B", "FINEB"}, {"GLMOD", "GLMD"},
    {"SECTION", "SECT"}, {"LENGTH", "LEN"}, {"REPEAT", "REP"}, {"JITTER", "JITT"}, {"VIBRATO", "VIB"},
    {"TEMPO", "BPM"}, {"SNAPPY", "SNAP"}, {"SEND 1", "FX 1"}, {"SEND 2", "FX 2"}, {"SEND 3", "FX 3"},
    {"DIFFUSE", "DIFF"}};
static const char *cell_label(const char *label, int32_t room)
{
    uint32_t i;
    if (text_w(TY_LB, label) <= room)
        return label;
    for (i = 0; i < NELEM(CELL_SHORT); i++)
        if (str_eq(label, CELL_SHORT[i].k))
            return CELL_SHORT[i].s;
    return label;
}
static void draw_column(uint32_t c, const char *label0, const char *val0, const char *unit0, uint16_t vc)
{
    char key[48], label[12], val[16], unit[8];
    int hot = c == ui.hot_col && ui.hot_t, strip, snap, named = 0;
    uint8_t sig;
    uint16_t lc = vc == T_DIM ? T_DIM : card_tc[c & 3u] ? card_tc[c & 3u] : T_KNOB(c), cbg = T_BG;   /* (knob c's colour) */
    int32_t x, uw, room = COL_W;                        /* (the 3 px between cells keep values apart) */
    const aafont_t *vf = TY_TX, *lf = TY_LB;
    uint32_t n, kn;
    int32_t kid;
    pv_words(label0, val0, unit0, label, val, unit);    /* (the words as the pages write them: "Saw", "ms") */
    if (str_eq(unit0, label0))
        unit[0] = 0;                                    /* "BPM 124 BPM": the label says it */
    named = val[0] >= 'A' && val[0] <= 'Z' && val[1] >= 'a' && val[1] <= 'z';   /* (a name: no roll) */
    kid = kc_tag(val, &kn);
    str_cpy(key, label, 12);                            /* cache key: texts + colour */
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), val, 14);
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), unit, 8);
    n = str_len(key);
    if (n > sizeof ui.col[0] - 8u)
        n = sizeof ui.col[0] - 8u;
    /* every RGB565 bit: status colours can change with identical text */
    key[n] = (char)('A' + (vc & 15u));
    key[n + 1] = (char)('A' + ((vc >> 4) & 15u));
    key[n + 2] = (char)('A' + ((vc >> 8) & 15u));
    key[n + 3] = (char)('A' + (vc >> 12));
    key[n + 4] = (char)('A' + (lc & 15u) + ((lc >> 8) & 7u));
    key[n + 5] = (char)('0' + hot);
    key[n + 6] = 0;
    uw = unit[0] ? text_w(lf, unit) + 2 : 0;
    if (uw && text_w(vf, val) + uw > room)              /* ("1.0 ms"): the value keeps its size, the unit goes */
        unit[0] = 0, uw = 0;
    if (text_w(vf, val) > room)                         /* ("HARM MIN"): the label face before it is cut */
        vf = lf;
    sig = (uint8_t)str_hash(str_hash(V->sel + ux.gen * 64u, label), unit);
    snap = ui.force || sig != ui.roll[c].sig;           /* what the value is of: label, unit, track, palette */
    strip = !snap && str_eq(key, ui.col[c]);
    if (strip && !ui.roll[c].from[0])
        return;
    roll_bg = cbg;
    if (hot && vc != T_DIM)                             /* the knob turning: its value in the track's colour */
        vc = lc;
    if (strip) {                                        /* rolling: the value strip only */
        cv_begin(COL_W, ROLL_H, cbg);
        cv_oy = -ROLL_Y;
    } else {
        char ov[16];                                    /* the value drawn before (in the cache key) */
        col_old_value(c, ov);
        ui.roll[c].sig = sig;
        if (!str_eq(ov, val))
            roll_note(c, ov, val, snap || named || vf != TY_TX || kid >= 0);
        else if (snap)
            ui.roll[c].from[0] = 0;
        str_cpy(ui.col[c], key, sizeof ui.col[c]);
        cv_begin(COL_W, COL_H, T_BG);
        if (hot)                                        /* (and a bar under it) */
            cv_rrect(COL_W / 2 - 14, COL_H - 4, 28, 3, 1, lc, T_BG);
    }
    if (label[0] || val[0]) {
        /* the label: capitals in the knob's colour, centred, rows 3..21; the value under it (the text face, rolling),
         * the unit (label face, mid) on its baseline, both centred */
        int32_t vw = text_w(vf, val) + uw, vx = COL_W / 2 - (vw < room ? vw : room) / 2;
        if (!strip && label[0]) {
            const char *ls = cell_label(label, COL_W - 2);
            cv_text_fit(COL_W / 2 - (text_w(lf, ls) < COL_W - 2 ? text_w(lf, ls) : COL_W - 2) / 2, 3, lf, ls, lc,
                        cbg, COL_W - 2);
        }
        if (kid >= 0)                                   /* "[OCT+]": the keycap (accent: it would act; DIM: it would not) */
            x = cv_keycap(COL_W / 2 - kc_w((uint32_t)kid) / 2, 21, (uint32_t)kid,
                          vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_INK, cbg);
        else if (binary_value(val) >= 0) {
            binary_badge(COL_W / 2, 21, 22, val, cbg);
            x = COL_W - 6;
        } else if (vf == TY_TX)                         /* the value rolls */
            x = roll_text(c, vx, ROLL_Y, val, vc);
        else if (named)                                 /* a name ("Chorus"): free text */
            x = cv_free_text(vx, ROLL_Y + TY_TX->asc - vf->asc, vf, val, vc, cbg, room - uw);
        else
            x = cv_text_fit(vx, ROLL_Y + TY_TX->asc - vf->asc, vf, val, vc, cbg, room - uw);
        if (unit[0])                                    /* the unit on the value's baseline */
            cv_text_on(x + 2, ROLL_Y + TY_TX->asc - lf->asc, lf, unit, T_MID, cbg);
    }
    cv_oy = 0;
    if (stage_drop_rows && !strip)                      /* (rows r0 .. under the header: Y_LABEL - r0 wraps, + r0 back) */
        cv_blit_from((uint32_t)CARD_X(c), (uint32_t)card_y - (uint32_t)(COL_H - stage_drop_rows), COL_H - stage_drop_rows);
    else
        cv_blit((uint32_t)CARD_X(c), (uint32_t)(card_y + (strip ? ROLL_Y : 0)));
}
/* the four cards as Melodee gives them (MEL_CARD); GLO's: each label in its place's track colour */
static void draw_columns(void)
{
    uint32_t c;
    for (c = 0; c < 4u; c++) {
        const mel_card_t *k = &V->card[c];
        uint32_t cv = k->flags & 3u;
        uint16_t vc = cv == MEL_CV_DIM ? T_DIM : cv == MEL_CV_ACCENT ? T_ACCENT : cv == MEL_CV_KNOB ? T_KNOB(c) : VAL(c);
        card_tc[c] = (k->flags & MEL_CF_TRACK) ? T_TRK(c) : 0;
        if ((k->flags & MEL_CF_HOT) && !ui.hot_t) {
            ui.hot_col = (uint8_t)c;
            ui.hot_t = 1;
        }
        draw_column(c, k->label, k->value, k->unit, vc);
    }
    memset(card_tc, 0, sizeof card_tc);
}

/* --------------------------------------------------------- the sound bar --- */
/* the sound bar (ui_pages.c pv_sound_bar): the track's numeral (filled in its colour) and the sound's name (the text
 * face, secondary), capitals centred on the bar's middle row; at the right the page's place ("1/2") when it has one */
static void pv_sound_bar(int32_t y, uint32_t k, const char *nm, const char *right, uint16_t tc, uint16_t bg)
{
    const aafont_t *lf = TY_LB, *tf = TY_TX;
    char d[3];
    int32_t cy = y + H_FOOT / 2, bw = right[0] ? text_w(tf, right) + 12 : 0;
    numeral_text(d, k);
    cv_rrect(4, cy - 11, 22, 22, 6, tc, bg);
    cv_text_c(15, ty_y(cy, lf), lf, d, T_INK, tc);
    cv_free_text(34, ty_y(cy, tf), tf, nm, T_SEC, bg, 236 - bw - 34);
    if (bw)
        cv_text_r(236, ty_y(cy, tf), tf, right, T_THEME, bg);
}
/* the sound being edited (pv_sound): its track and name; a shared FX slot: every track's ("All tracks") */
static void pv_sound(void)
{
    uint32_t sig, all = V->foot == MEL_FT_ALL;
    if (V->foot == MEL_FT_NONE)
        return;
    sig = str_hash(str_hash(V->sel * 7u + ux.gen * 977u + ux.pal * 31u + all * 4099u + trk_numeral(V->sel) * 613u,
                            V->foot_text), V->foot_right);
    if (!ui.force && sig == ui.foot_sig)
        return;
    ui.foot_sig = sig;
    cv_begin(240, 240 - Y_FOOT, T_BG);
    if (all) {                                          /* the four tracks' colours for the numeral */
        int32_t cy = H_FOOT / 2;
        uint32_t k;
        for (k = 0; k < 4u; k++)
            cv_rrect(4 + (int32_t)(k & 1u) * 12, cy - 11 + (int32_t)(k >> 1) * 12, 10, 10, 3, T_TRK(k), T_BG);
        cv_free_text(34, ty_y(cy, TY_TX), TY_TX, V->foot_text[0] ? V->foot_text : "All tracks", T_SEC, T_BG, 236 - 34);
    } else {
        pv_sound_bar(0, V->sel, V->foot_text, V->foot_right, T_THEME, T_BG);
    }
    cv_blit(0, Y_FOOT);
}

/* the toast: a message too long for the header on a card under it, wrapped (nothing cut); when it goes, what it covered
 * is drawn again */
static void draw_toast(void)
{
    char l[3][32];
    const char *m = head_msg();
    uint32_t n, i;
    int32_t h;
    if (!head_toast(BPM_X + BPM_W + 6))
        return;
    n = text_lines(m, l, 3u, BOX_W - 20, TY_TX);
    h = 10 + (int32_t)n * 24;
    for (uint32_t band = 0; band < (uint32_t)((uint32_t)h); band += CV_MAX / (BOX_W)) {
        cv_begin_band(BOX_W,(uint32_t)h,T_BG,band);
        cv_rrect(0, 0, BOX_W, h, 8, T_LINE, T_BG);
        cv_rrect(1, 1, BOX_W - 2, h - 2, 7, T_SURF, T_LINE);
        for (i = 0; i < n; i++)
            cv_free_hint(10, 5 + (int32_t)i * 24, l[i], T_TEXT, T_SURF, BOX_W - 20);
        GFX_HOOK_COVER(BOX_X, H_HEAD + 2, BOX_X + BOX_W, H_HEAD + 2 + h);
        cv_blit_band(BOX_X, H_HEAD + 2);
    }
    ui.toast = 1;
}
