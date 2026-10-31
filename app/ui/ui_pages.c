/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments: his Felucca code, used under MPL-2.0
 * with his permission (hugelton/felucca-bsp#4) */
/* Melodee FM-1's knob pages, FX racks and MIXER (its firmware/src/ui_pages.c, with CHORD's picture from ui_graph.c),
 * ported to Melodee Remote in the large type: no footer; the header names the page and shows where it is in its
 * family (dots); the four knobs as the strip under the header (a cell a knob: its label over its value, the knob just
 * turned lifted in the track's colour); the page's own picture under them (Melodee's MEL_PICTURE); the sound being
 * edited at the foot (MEL_FOOT). The knob turning shows in its cell alone: nothing over the picture.
 * The pictures: the scope (TEMPO, a page of plain values), a filter's response, ARP's notes, an effect's shape, CHORD's
 * chord and keys, a pattern's 64 steps.
 * FX's racks (MEL_RACK) under the strip; MIXER (MEL_MIX): Stage's rows as channel strips. Each part remembers what it
 * drew. Part of app.c */
#define PV_PANEL_X BOX_X                                /* a panel: x 2 .. 237 */
#define PV_PANEL_W BOX_W
#define PV_SOUND_Y Y_FOOT                               /* the sound: rows 212 .. 239 */
#define PV_PIC_Y Y_GRAPH                                /* RINGS: the picture, rows 84 .. 207 */
#define PV_PIC_H H_GRAPH

static struct { uint32_t panel, rack, mix[NTRK]; uint8_t meter[NTRK]; int32_t pplay; } pv;

/* (dsp.c, gen_tables.py) the sine of a phase (a turn: 2^32), Q15; tanh's soft clip; a filter's cutoff index in Hz */
static int32_t sine_i(uint32_t ph) { return (int32_t)(32767.0f * fm_sinf((float)ph * 1.46291808e-9f)); }
static int32_t softclip(int32_t x) { return (int32_t)(32767.0f * fm_tanhf((float)x * (1.0f / 32768.0f))); }
static int32_t cutoff_hz(int32_t v) { return (int32_t)(30.0f * fm_powf(16000.0f / 30.0f, (float)clamp(v, 0, 127) / 127.0f) + 0.5f); }

/* ---------------------------------------------------------- RINGS --- */
/* a page with a cutoff: the filter's response (flat, the resonance's peak at the cutoff, then the slope); h: the
 * panel's rows */
static void pv_filter(int32_t cut, int32_t res, int32_t h)
{
    int32_t x, fc = 16 + clamp(cut, 0, 127) * 196 / 127, peak = clamp(res, 0, 127) * 30 / 127, y0 = h * 2 / 5 * 16, py = 0;
    for (x = 10; x < h - 10; x += 5)
        cv_rect(fc, x, 1, 2, ux_mix(T_PANEL, T_THEME, 60));
    for (x = 0; x < 216; x++) {
        int32_t d = 12 + x - fc, y = y0 - peak * 16 * 64 / (64 + d * d) + (d > 0 ? d * 14 : 0);
        y = clamp(y, 8 * 16, (h - 10) * 16);
        cv_vspan_aa(12 + x, x ? py : y, y, T_THEME);
        py = y;
    }
}
/* ARP (mock/r6_pages ARP): the notes it plays, a block a step (16 at most): the keys held (else C3 E3 G3) over OCT
 * octaves in MODE's order, low to high bottom to top; the notes and the octaves at the top. OFF: dim. Melodee's MODE
 * (0 off, 1 up, 2 down, 3 up-down, 4 random, 5 as played, 6 repeat), octaves and held keys */
static void pv_arp(int32_t h)
{
    static const uint8_t DEF[3] = {48, 52, 55};
    uint8_t base[16], seq[16];
    uint32_t nb = V->pic_n ? V->pic_n : 3u, n = 0, o, i, j, oct = (uint32_t)clamp(V->pic_v[1], 1, 4), mode = V->pic_v[0];
    int32_t lo = 127, hi = 0, w;
    char b[32];
    if (nb > 16u) nb = 16u;
    for (i = 0; i < nb; i++) base[i] = V->pic_n ? V->pic_note[i] : DEF[i];
    if (mode != 5u)                                     /* (ORD: as played; the others from the lowest) */
        for (i = 1; i < nb; i++)
            for (j = i; j > 0 && base[j - 1] > base[j]; j--) { uint8_t x = base[j]; base[j] = base[j - 1]; base[j - 1] = x; }
    for (o = 0; o < oct; o++)
        for (i = 0; i < nb && n < 16u; i++) seq[n++] = (uint8_t)clamp(base[i] + 12 * (int32_t)o, 0, 127);
    if (mode == 2u)                                     /* DN */
        for (i = 0; i < n / 2u; i++) { uint8_t x = seq[i]; seq[i] = seq[n - 1u - i]; seq[n - 1u - i] = x; }
    else if (mode == 3u)                                /* UPDN: up, then down without the ends again */
        for (i = n >= 2u ? n - 2u : 0u; i > 0u && n < 16u; i--) seq[n++] = seq[i];
    else if (mode == 4u)                                /* RND: a shuffle (as a picture: one of its orders) */
        for (i = n; i > 1u; i--) { j = (i * 7u + 3u) % i; uint8_t x = seq[i - 1u]; seq[i - 1u] = seq[j]; seq[j] = x; }
    for (i = 0; i < n; i++) { lo = seq[i] < lo ? seq[i] : lo; hi = seq[i] > hi ? seq[i] : hi; }
    w = n ? 208 / (int32_t)n : 208;
    for (i = 0; i < n; i++) {                           /* (under the notes' line: rows 34 .. h - 12) */
        int32_t y = h - 19 - (hi > lo ? (seq[i] - lo) * (h - 53) / (hi - lo) : (h - 53) / 2);
        cv_rrect(16 + (int32_t)i * w, y, w - 4 < 18 ? w - 4 : 18, 7, 2, mode ? T_KNOB((uint32_t)(seq[i] - lo) / 12u) : T_DIM,
                 T_PANEL);                              /* (each octave a colour) */
    }
    b[0] = 0;
    for (i = 0; i < nb && i < 4u; i++) {
        note_name(b + str_len(b), base[i]);
        str_cpy(b + str_len(b), " ", 2);
    }
    str_cpy(b + str_len(b), "\xB7 ", 4);
    fmt_int(b + str_len(b), (int32_t)oct);
    str_cpy(b + str_len(b), " oct", 5);
    cv_free_text(12, 6, TY_LB, b, T_MID, T_PANEL, PV_PANEL_W - 24);
}
/* PATTERN (mock/r6_pages PATTERN): the 64 steps as dots, four rows of 16: a note or a hit the track's colour, a rest
 * a line, past LEN dimmer still; the step playing white; LEN of 64 as the bar under them */
#define PV_STEP_X(s) (22 + (int32_t)((s) % 16u) * 12 + (int32_t)((s) % 16u / 4u) * 4)   /* a step's dot (7 px) */
static void pv_steps(int32_t h)
{
    const mel_track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->len, 1, NSTEP), s, ph = pv.pplay >= 0 ? (uint32_t)pv.pplay : 0xFFu;
    for (s = 0; s < NSTEP; s++) {
        int32_t x = PV_STEP_X(s), y = 16 + (int32_t)(s / 16u) * (h - 40) / 3;
        uint16_t c = s == ph ? T_TEXT : s >= len ? ux_mix(T_PANEL, T_LINE, 50) :
                     step_on_at(t, s) ? T_KNOB(s % 16u / 4u) : T_LINE;   /* (each beat a colour) */
        cv_disc(x, y, s == ph || (s < len && step_on_at(t, s)) ? 7 : 5, c, 0);
    }
    cv_rrect(16, h - 12, 208, 4, 2, T_LINE, T_PANEL);
    cv_rrect(16, h - 12, (int32_t)len * 208 / NSTEP, 4, 2, T_THEME, T_LINE);
}
/* FX (README "FX rack"): an effect slot's page: no words, a picture of what the effect does, in the track's colour, the
 * dry sound dimmer behind it where it reshapes it: DRIVE its wave through the SHAPE, CRUSH its steps (BITS) and holds
 * (ALIAS), FLANGER its comb and PEDAL PHASER its two notches (dim: where DEPTH sweeps them to), PEDAL CHORUS its voices,
 * ROOM and REVERB their tail (the lighter top: the highs, which DAMP shortens), NOTE ECHO its echoes at TIME, their
 * height the velocity; the older effects their curves. ROOM, REVERB: TONE as a strip under it (the band that reaches
 * the effect, from its own low cut, the cut TONE sets under its edge; FULL dimmed); DRIVE: its TONE, the low-pass it
 * ends in. WIDTH says what mono does; a time effect without its buffer says MEMORY FULL. Melodee's MEL_PIC_FX: the
 * effect (Melodee FM-1's number), its MIX, RATE, CHAR, 64 + TONE, flags */
static const uint8_t CRUSH_HOLD[16] = {64, 48, 40, 32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1};
#define FX_TONE_MAX 63
static int fx_drive(int32_t type) { return type >= MEL_FX_SOFT && type <= MEL_FX_FUZZ; }
static int fx_sloop_type(int32_t t) { return t >= MEL_FX_SOFT && t <= MEL_FX_PEDAL_CHORUS; }
static int fx_has_tone(int32_t type) { return type == MEL_FX_ROOM || type == MEL_FX_REVERB; }
static int32_t fx_tone_hz(int32_t v)
{
    int32_t a = v < 0 ? -v : v;
    if (a > FX_TONE_MAX) a = FX_TONE_MAX;
    return v > 0 ? 150 + a * a * 1130 / 3969 : v < 0 ? 120 + (64 - a) * (64 - a) * 2520 / 3969 : 0;
}
static int32_t fx_low_cut_hz(int32_t type, int32_t v)   /* a ROOM / REVERB slot's low cut at TONE v */
{
    int32_t f = type == MEL_FX_REVERB ? 0 : 150, t = v > 0 ? fx_tone_hz(v) : 0;
    return t > f ? t : f;
}
/* (fx_sloop_insert.h) the drives' shape of v (a voice's full scale 32768; v already through DRIVE) */
static int32_t ins_shape(uint32_t ty, int32_t v)
{
    switch (ty) {
    case MEL_FX_SOFT:
        return softclip(v);
    case MEL_FX_HARD:
        return clamp(v, -24000, 24000);
    case MEL_FX_FOLD: {
        int32_t u = (int32_t)((uint32_t)(v + 16384) & 0xFFFFu) - 32768;   /* a triangle of period 65536: +-16384 */
        u = 16384 - (u < 0 ? -u : u);
        return u + (u >> 1);                            /* x 1.5 (no divide) */
    }
    case MEL_FX_FUZZ:
        return v > 0 ? (v > 20000 ? 20000 : v) : softclip(v) >> 1;
    default:
        return v;
    }
}
static int32_t pv_log_x(int32_t hz, int32_t top, int32_t span)   /* 20 Hz .. top on a log scale: x 16 .. 223;
                                                                  * span: log2(top / 20) Q8 */
{
    uint32_t v = (uint32_t)clamp(hz, 20, top) * 16u / 20u, o = 4;   /* (v 16 ..: its log2 from 4) */
    while (v >> (o + 1u))
        o++;
    return 16 + (int32_t)((o - 4u) * 256u + ((v << 8) >> o) - 256u) * 208 / span;
}
static int32_t pv_hz_x(int32_t hz) { return pv_log_x(hz, 2640, 1803); }   /* (log2 132 = 1803 / 256) */
/* a strip under the picture: the band x a .. b lit in c (dim: FULL, calm), "320 Hz" under the edge x when hz */
static void pv_fx_strip(int32_t sy, int32_t ly, int32_t a, int32_t b, int32_t x, int32_t hz, uint16_t c, int dim)
{
    const aafont_t *lf = TY_LB;
    char t[12];
    cv_rrect(16, sy, 208, 5, 2, T_LINE, T_PANEL);
    cv_rrect(a, sy, b - a > 5 ? b - a : 5, 5, 2, dim ? ux_mix(T_LINE, c, 45) : c, T_LINE);
    if (!hz)
        return;
    fmt_int(t, hz < 1000 ? hz : hz / 1000);
    if (hz >= 1000) {
        uint32_t n = str_len(t);
        t[n] = '.';
        t[n + 1u] = (char)('0' + hz % 1000 / 100);
        t[n + 2u] = 0;
    }
    str_cpy(t + str_len(t), hz < 1000 ? " Hz" : " kHz", 6);
    if (x < 16 + text_w(lf, t) / 2) cv_text_on(16, ty_y(ly + lf->h / 2, lf), lf, t, c, T_PANEL);
    else if (x > 224 - text_w(lf, t) / 2) cv_text_r(224, ty_y(ly + lf->h / 2, lf), lf, t, c, T_PANEL);
    else cv_text_c(x, ty_y(ly + lf->h / 2, lf), lf, t, c, T_PANEL);
}
/* the picture's value at column i (0 .. 207) of trace k (0: the dry or dim one), 1/16 px from cy, amp the half height */
static int32_t pv_fx_y(uint32_t type, uint32_t k, int32_t i, const int16_t *p, int32_t tone, int32_t amp)
{
    int32_t mix = p[1], rate = p[2], cha = p[3], s, v, n;
    uint32_t ph2 = (uint32_t)i * 41297762u;            /* (two turns over the 208 columns) */
    if (fx_drive((int32_t)type)) {                      /* DRIVE: the sine, through the SHAPE at DRIVE */
        s = sine_i(ph2);
        v = k ? ins_shape(type, (s * (4096 + rate * rate * 3)) >> 12) : s;
        return -v * amp / 36000;
    }
    if (type == MEL_FX_CRUSH) {                         /* CRUSH: held (ALIAS) and stepped (BITS) */
        int32_t bits = 1 + ((127 - clamp(rate, 0, 127)) >> 3), hw = 2 + CRUSH_HOLD[(127 - clamp(cha, 0, 127)) >> 3] / 2;
        if (!k)
            return -sine_i(ph2) * amp / 36000;
        s = sine_i((uint32_t)(i / hw * hw) * 41297762u);
        v = (s * bits + (s >= 0 ? 16384 : -16384)) / 32768;
        return -v * amp * 32 / (bits * 36);
    }
    if (type == MEL_FX_FLANGER) {                       /* its comb: notches across (DEPTH), sharper with RESO */
        n = k ? 96 + cha * 300 / 127 : 30 + cha * 80 / 127;   /* (notches x 64 over the width) */
        s = sine_i((uint32_t)(i * n) * 322638u + 0x40000000u);   /* (a turn: 2^32 / 208 / 64) */
        v = (32768 + s) / 2;                            /* 0 .. 32768: cos^2 */
        if (tone > 0) v += tone * ((v * v >> 15) - v) / 63;
        else if (tone < 0) v -= tone * ((32768 - ((32768 - v) * (32768 - v) >> 15)) - v) / 63;
        return amp - v * amp * 2 / 33000;
    }
    if (type == MEL_FX_PEDAL_PHASER) {                  /* two notches, DEPTH apart, narrower with RESO */
        int32_t c = k ? 116 + cha / 4 : 60 + cha / 8, w = 14 - (tone < 0 ? -tone : tone) * 7 / 63, j, m = 256;
        for (j = -1; j <= 1; j += 2) {
            int32_t t = (i - c - j * 30) * 16 / w;
            m -= 65536 / (256 + t * t);
        }
        m = clamp(m, 0, 256);
        return amp - m * amp * 7 / 1000;
    }
    if (type == MEL_FX_PEDAL_CHORUS) {                  /* the dry wave and its voices, apart by DEPTH, drifting */
        uint32_t ph3 = (uint32_t)i * 61946643u, drift = (uint32_t)(sine_i((uint32_t)i * 20648881u) + 32768) * 4000u;
        return -sine_i(ph3 - (uint32_t)k * ((uint32_t)cha * 2900000u + drift)) * amp / 46000;
    }
    s = sine_i((uint32_t)i * 0x06000000u) >> 10;       /* the older effects (MIX: their reach) */
    if (type == MEL_FX_PHASER || type == MEL_FX_WIDTH || type == MEL_FX_CHORUS) return s * mix / 9;
    if (type == MEL_FX_FORMANT) return -((s + 32) * mix) / 15;
    if (type == MEL_FX_TAPE || type == MEL_FX_SPIT) return (s * ((i * (cha + 1) / 32) % 32)) / 4;
    return 0;
}
static void pv_fx(int32_t h)
{
    const aafont_t *lf = TY_LB;
    uint32_t type = (uint32_t)clamp(V->pic_v[0], 0, MEL_NFX - 1), failed = V->pic_v[5] & MEL_FXF_MEMORY, k;
    int16_t p[4] = {(int16_t)type, V->pic_v[1], V->pic_v[2], V->pic_v[3]};   /* TYPE MIX RATE CHAR */
    int32_t mix = p[1], rate = p[2], cha = p[3], tone = clamp((int32_t)V->pic_v[4] - 64, -FX_TONE_MAX, FX_TONE_MAX);
    int strip = (fx_has_tone((int32_t)type) || fx_drive((int32_t)type)) && !failed, i;
    int32_t ly = h - lf->h - 4, sy = ly - 9, top = 8 * 16;
    int32_t low = (strip ? sy - 6 : failed || type == MEL_FX_WIDTH ? h - lf->h - 10 : h - 8) * 16;
    int32_t cy = (top + low) / 2, amp = (low - top) / 2;
    uint16_t th = type && mix ? T_THEME : T_DIM, gh = ux_mix(T_PANEL, th, 38);
    if (type == MEL_FX_ROOM || type == MEL_FX_REVERB) {   /* the tail: SIZE long (ROOM: DECAY too), its highs lighter */
        int32_t len = type == MEL_FX_ROOM ? 30 + rate * 70 / 127 + cha * 60 / 127 : 40 + rate * 150 / 127;
        int32_t kb = 32768 - 9 * 32768 / len,
                kh = 32768 - 9 * 32768 * 10 / (len * (10 - (type == MEL_FX_REVERB ? cha * 7 / 127 : 5)));
        int32_t eb = 32767, eh = 32767, base = low >> 4, hgt = (low - top) >> 4;
        uint32_t r = 0x2545F491u;
        uint16_t hc = ux_mix(th, T_TEXT, 55);
        for (i = 20; i < 224 && eb > 400; i += 3) {
            int32_t a = i < 24 ? (i - 19) * 6553 : 32767, b, c;
            r = r * 1103515245u + 12345u;
            b = hgt * (eb >> 7) / 256 * (18000 + (int32_t)(r >> 17) % 14768) / 32768 * a / 32768;
            r = r * 1103515245u + 12345u;
            c = hgt * 45 / 100 * (eh >> 7) / 256 * (16384 + (int32_t)(r >> 17) % 16384) / 32768 * a / 32768;
            if (b > 0) cv_rect(i, base - b, 2, b, th);
            if (c > 1) cv_rect(i, base - c, 2, c, hc);
            eb = eb * kb >> 15;
            eh = eh * kh >> 15;
        }
    } else if (type == MEL_FX_NOTE_ECHO) {              /* the note and its echoes at TIME, LEVEL and DECAY */
        static const uint8_t DIVQ[10] = {16, 8, 4, 2, 5, 3, 32, 64, 128, 255};   /* N_DIV in 1/16 beats */
        int32_t step = 52 * DIVQ[clamp(rate, 0, 9)] / 16, x = 18, v = 127, n = 0, base = (low >> 4) - 6, hgt = ((low - top) >> 4) - 6;
        for (k = 0; k < 5u; k++)                        /* the beats */
            cv_rect(18 + (int32_t)k * 51, base + 5, 2, 4, k % 4u ? T_LINE : T_DIM);
        if (step < 3) step = 3;
        while (x < 222 && v > 0 && n < 32) {
            int32_t b = hgt * v / 127, w = step * 55 / 100 < 3 ? 3 : step * 55 / 100;
            if (b > 0)
                cv_rrect(x, base - b, w, b, w > 5 ? 3 : 1, n ? ux_mix(T_PANEL, th, 95 - (n < 7 ? n : 7) * 9) : th, T_PANEL);
            v = n ? v * (127 - clamp(cha, 0, 127)) / 127 : v * (127 - clamp(cha, 0, 127)) / 127 * clamp(mix, 0, 127) / 127;
            x += step;
            n++;
        }
    } else {
        int ghost = fx_sloop_type((int32_t)type);       /* (the older effects: their curve alone) */
        for (k = ghost ? 0u : 1u; k < (type == MEL_FX_PEDAL_CHORUS ? 3u : 2u); k++) {   /* the dry trace (dim), the
                                                                                    * sound; PEDAL CHORUS: two voices */
            int32_t py = 0;
            uint16_t c = !k ? gh : k == 1u && type == MEL_FX_PEDAL_CHORUS ? ux_mix(T_PANEL, th, 70) : th;
            for (i = 0; i < 208; i++) {
                int32_t y = clamp(cy + pv_fx_y(type, k, i, p, tone, amp), top, low);
                cv_vspan_aa(16 + i, i ? py : y, y, c);
                py = y;
            }
        }
    }
    if (failed || type == MEL_FX_WIDTH) {
        cv_free_text(12, h - lf->h - 6, lf, failed ? "MEMORY FULL" : "Mono changes the tone", T_DIM, T_PANEL, PV_PANEL_W - 24);
    } else if (fx_drive((int32_t)type)) {               /* DRIVE's TONE: the low-pass, lit up to its cut (20 Hz .. 20 kHz) */
        int32_t hz = cutoff_hz(cha), x = pv_log_x(hz, 20000, 2551);
        pv_fx_strip(sy, ly, 16, x, x, hz, T_KNOB(2), 0);
    } else if (fx_has_tone((int32_t)type)) {             /* TONE: what passes, lit (from the effect's floor) */
        int32_t lo = fx_low_cut_hz((int32_t)type, tone), hz = tone < 0 ? fx_tone_hz(tone) : lo;
        int32_t a = pv_hz_x(lo), b = tone < 0 ? pv_hz_x(hz) : 224;
        pv_fx_strip(sy, ly, a, b, tone < 0 ? b : a, tone ? hz : 0, T_KNOB(3), !tone);
    }
}

/* (ui_graph.c) words in the panel: a line, centred, its first a key hint when it is one ("[K1] DIA3: IN-KEY CHORDS") */
static void note_line(int32_t y, const char *s, uint16_t fg)
{
    uint32_t n;
    int32_t id = kc_tag(s, &n);
    if (id < 0) {
        cv_text_c(120, ty_y(y + 8, TY_LB), TY_LB, s, fg, cv_bg);
        return;
    }
    s += n;
    while (*s == ' ')
        s++;
    cv_text_on(cv_keycap(120 - kh_w((uint32_t)id, s) / 2, y + 8 - KC_PH / 2, (uint32_t)id, T_KEY, T_INK, cv_bg) + KH_GAP,
               ty_y(y + 8, TY_LB), TY_LB, s, fg, cv_bg);
}
static void panel_note(const char *a, const char *b, const char *c)
{
    cv_text_c(120, ty_y(36, ty_fit(TY_TX, a, 220)), ty_fit(TY_TX, a, 220), a, T_TEXT, cv_bg);
    if (b && b[0]) note_line(60, b, T_MID);
    if (c && c[0]) note_line(86, c, T_DIM);
}
/* CHORD (ui_graph.c graph_chord): the chord's name ("Cm7": the last one the track played; before any, the one on its
 * ROOT) over the keys from the C below its lowest note (two octaves, three for a wide one): its notes THEME, the
 * accented one (its root) ACCENT, the others RAISE; its caption ("LAST", "ON ROOT") at the right. Melodee's: the notes,
 * the accented note, the name and the caption (MEL_PIC_CHORD); else three lines of what to do instead (MEL_PIC_LINES) */
static void graph_chord(uint16_t c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    const uint8_t *nn = V->pic_note;
    uint32_t n = V->pic_n, i, j, lo, cnt, w, acc = V->pic_v[0];
    if (!n) {
        panel_note(V->pic_text[0], "", "");
        return;
    }
    cv_text_on(14, ty_y(18, TY_VL), TY_VL, V->pic_text[0], T_TEXT, cv_bg);
    cv_text_r(226, ty_y(18, TY_LB), TY_LB, V->pic_text[1], T_MID, cv_bg);
    lo = nn[0] - nn[0] % 12u;
    cnt = nn[n - 1u] - lo < 24u ? 24u : 36u;
    w = 216u / cnt;
    for (i = 0; i < cnt; i++) {
        uint32_t note = lo + i, on = 0;
        int32_t x = 12 + (int32_t)(i * w);
        for (j = 0; j < n; j++)
            on |= nn[j] == note;
        cv_rrect(x, BLACK[note % 12u] ? 36 : 62, (int32_t)w - 2, 48, w > 6u ? 3 : 2,
                 !on ? T_RAISE : note == acc ? T_ACCENT : c, cv_bg);
    }
}
/* the picture under the strip (the page's: Melodee's MEL_PICTURE), straight on the background (no panel: the screen
 * calm); the scope moves (redrawn as each new trace comes), PATTERN's playhead too */
static void pv_picture(void)
{
    int32_t y = PV_PIC_Y, h = PV_PIC_H;
    uint32_t pic = V->pic, sig;
    int32_t pp = pic == MEL_PIC_STEPS && song_playing() && TSEL->ph != MEL_PH_NONE ?
                 (int32_t)(TSEL->ph % (uint32_t)clamp(TSEL->len, 1, NSTEP)) : -1;
    sig = mem_hash(pic * 131u + ux.gen * 977u + ux.pal * 31u + V->sel * 7u, V->pic_v, sizeof V->pic_v);
    sig = mem_hash(sig + V->pic_n, V->pic_note, sizeof V->pic_note);
    sig = mem_hash(sig, V->pic_text, sizeof V->pic_text);
    if (pic == MEL_PIC_SCOPE || pic == MEL_PIC_NONE)    /* (the scope: every trace) */
        sig = mem_hash(sig + V->nscope, V->scope, V->nscope);
    if (pic == MEL_PIC_STEPS)
        sig ^= steps_hash(TSEL);
    if (!ui.force && sig == pv.panel && pp == pv.pplay)
        return;
    pv.panel = sig;                                     /* (PATTERN, the step playing alone moved: cv_blit sends its */
    pv.pplay = pp;                                      /*  two dots) */
    for (uint32_t band = 0; band < (uint32_t)((uint32_t)h); band += CV_MAX / (240)) {
        cv_begin_band(240,(uint32_t)h,T_BG,band);
        cv_rrect(PV_PANEL_X, 0, PV_PANEL_W, h, 6, T_PANEL, T_BG);
        cv_bg = T_PANEL;
        if (pic == MEL_PIC_ARP) pv_arp(h);
        else if (pic == MEL_PIC_STEPS) pv_steps(h);
        else if (pic == MEL_PIC_FILTER) pv_filter(V->pic_v[0], V->pic_v[1], h);
        else if (pic == MEL_PIC_FX) pv_fx(h);
        else if (pic == MEL_PIC_CHORD || pic == MEL_PIC_LINES) {   /* (as ui_graph.c draws them: on a SURF panel) */
            cv_rrect(3, 0, 234, h, 5, T_SURF, T_BG);
            cv_bg = T_SURF;
            if (pic == MEL_PIC_CHORD) graph_chord(T_THEME);
            else panel_note(V->pic_text[0], V->pic_text[1], V->pic_text[2]);
        }
        else if (pic == MEL_PIC_ENV) graph_adsr_v(V->pic_v[0], V->pic_v[1], V->pic_v[2], V->pic_v[3], 12, h - 14, T_THEME);
        else stage_wave(T_THEME, 8, h - 16);
        cv_blit_band(0, (uint32_t)y);
    }
}

/* ------------------------------------------------------------ the racks --- */
/* FX's racks (README "FX rack"): under the knobs' strip (KNOB 1 FX, 2 SLOT, 4 the amount: what the row picked gets;
 * SHARED's 3 ROUTE), a row a slot. FX: the track's three slots and the shared FX; SHARED: its three slots and RETURN. A
 * row: its number (A-C on SHARED; the shared FX an arrow, RETURN the four tracks' colours), the effect ("--" none; the
 * shared row: "Shared" and the shared effects in order), under them the amount's bar and value (a slot's MIX, the
 * track's SEND). The row picked wears the colours of the knobs that edit it: outlined and tinted in KNOB 2's, its effect
 * in KNOB 1's, its amount in KNOB 4's; empty slots dim. Each row Melodee's (MEL_RACK) */
#define PV_RACK_H 39                                    /* the rows: 39 px each under the strip, y 84 .. 239 */
static void pv_rack_rows(void)
{
    uint32_t k, j, sg;
    uint16_t k1 = T_KNOB(0), k2 = T_KNOB(1), k4 = T_KNOB(3);
    sg = mem_hash(ux.gen * 977u + ux.pal * 31u + V->sel * 13u + T_TRK(0) + T_TRK(1) * 3u + T_TRK(2) * 5u + T_TRK(3) * 7u,
                  V->rack, sizeof V->rack);
    if (!ui.force && sg == pv.rack)
        return;
    pv.rack = sg;
    for (k = 0; k < 4u; k++) {
        const mel_rack_t *rk = &V->rack[k];
        int sel = (rk->flags & MEL_KF_SELECTED) != 0u, on = !(rk->flags & MEL_KF_OFF);
        uint16_t bg = sel ? ux_mix(T_BG, k2, 18) : T_BG, wc = on ? (sel ? T_TEXT : T_SEC) : T_DIM;
        char nm[32];
        int32_t x, vx = 228;
        cv_begin(240, PV_RACK_H, T_BG);
        if (sel) {
            cv_rrect(BOX_X, 1, BOX_W, PV_RACK_H - 3, 8, k2, T_BG);
            cv_rrect(BOX_X + 2, 3, BOX_W - 4, PV_RACK_H - 7, 6, bg, k2);
        }
        if (!(rk->flags & MEL_KF_NO_AMOUNT) && (rk->value[0] || rk->amount)) {   /* the amount: its bar, "66 %" at its end */
            int32_t a = clamp(rk->amount, 0, 127), w;
            w = cv_text_r(228, ty_y(28, TY_LB), TY_LB, rk->value, sel ? k4 : wc, bg) - 8 - 30;
            cv_rect(30, 27, w, 3, T_LINE);
            if (a > 0)
                cv_rect(30, 27, a * w / 127, 3, on ? (sel ? k4 : ux_mix(bg, k4, 45)) : T_DIM);
        }
        if (rk->flags & MEL_KF_RETURN) {                /* RETURN: every track's */
            for (j = 0; j < 4u; j++)
                cv_rrect(9 + (int32_t)(j & 1u) * 8, 5 + (int32_t)(j >> 1) * 8, 7, 7, 2, T_TRK(j), bg);
            cv_text_on(30, ty_y(12, TY_TX), TY_TX, rk->name[0] ? rk->name : "Return", wc, bg);
        } else if (rk->flags & MEL_KF_SHARED) {         /* the shared FX: an arrow, its effects in order */
            uint32_t n = 0;
            uint16_t ac = sel ? k2 : T_MID;
            const char *s = rk->name;
            cv_rect(9, 11, 11, 2, ac);
            cv_line(15, 7, 19, 11, ac);
            cv_line(15, 16, 19, 12, ac);
            x = cv_text_on(30, ty_y(12, TY_TX), TY_TX, "Shared", wc, bg) + 8;
            while (*s && x < vx - 12) {                 /* (its effects: Melodee's names, comma-separated) */
                uint32_t l = 0;
                char w[32];
                while (s[l] && s[l] != ',' && l + 1u < sizeof w) {
                    w[l] = s[l];
                    l++;
                }
                w[l] = 0;
                s += l + (s[l] == ',');
                while (*s == ' ')
                    s++;
                if (!w[0])
                    continue;
                if (n++) {                              /* (a chain: an arrow between; SPLIT: a comma) */
                    if (rk->flags & MEL_KF_SPLIT) {
                        x = cv_text_on(x - 7, ty_y(13, TY_LB), TY_LB, ",", T_MID, bg) + 6;
                    } else {
                        cv_rect(x, 13, 7, 2, T_MID);
                        cv_line(x + 4, 10, x + 7, 13, T_MID);
                        cv_line(x + 4, 17, x + 7, 14, T_MID);
                        x += 13;
                    }
                }
                menu_words(nm, w, sizeof nm);
                x = cv_free_text(x, ty_y(13, TY_LB), TY_LB, nm, T_MID, bg, vx - x) + 8;
            }
            if (!n)
                cv_text_on(x, ty_y(13, TY_LB), TY_LB, "--", T_DIM, bg);
        } else {
            cv_text_on(10, ty_y(12, TY_TX), TY_TX, rk->tag, sel ? k2 : T_MID, bg);
            menu_words(nm, on && rk->name[0] ? rk->name : "--", sizeof nm);
            cv_free_text(30, ty_y(12, TY_TX), TY_TX, nm, sel ? k1 : wc, bg, vx - 30);
        }
        cv_blit(0, (uint32_t)(Y_GRAPH + (int32_t)k * PV_RACK_H));
    }
}

/* ---------------------------------------------------------- MIXER --- */
/* MIXER (large type: C, rows as Stage's, ui_stage.c st_row_*): a track a row tinted in its colour (the selected one
 * more, outlined): its numeral, its sound, its level in dB at the right; under them the level as a bar in the track's
 * colour, the meter under it, its pan at the right ("L20", "C"); muted: dim, "Mute" in red; armed: a REC dot on the
 * numeral. The knob turning (LEVEL, PAN, MUTE: the selected track's) in text. Melodee's MEL_MIX, MEL_METER */
#define PV_MIX_BAR_X 46                                 /* the level's bar: x 46 .. 46 + PV_MIX_BAR_W, in the row */
#define PV_MIX_BAR_W 120
static int32_t pv_mix_y(int32_t h) { return (h - (TY_TX->cap + 8 + 10)) / 2; }   /* the row's block: its first capital */
/* track k's meter: a 3 px bar under its level's, as long as the output now */
static void pv_meter_draw(uint32_t k, int32_t x, int32_t y, uint16_t c, uint16_t bg)
{
    int32_t w = pv.meter[k] * PV_MIX_BAR_W / (TS_MH - 2);
    cv_rrect(x, y, PV_MIX_BAR_W, 3, 1, ux_mix(bg, T_LINE, 60), bg);
    if (w > 0)
        cv_rrect(x, y, w, 3, 1, c, T_LINE);
}
static void pv_meter(uint32_t k, uint16_t c)
{
    int32_t h = st_row_h();
    uint16_t bg = ux_mix(T_BG, c, k == V->sel ? 24 : 11);
    cv_begin(PV_MIX_BAR_W, 3, bg);
    pv_meter_draw(k, 0, 0, c, bg);
    cv_blit((uint32_t)(BOX_X + PV_MIX_BAR_X), (uint32_t)(st_row_y(k % NPART) + pv_mix_y(h) + TY_TX->cap + 8 + 7));
}
static void pv_strip(uint32_t k)
{
    const mel_track_t *t = TRK(k);
    uint32_t sel = k == V->sel, mute = (t->mix_flags & MEL_MF_MUTED) != 0u, arm = (t->mix_flags & MEL_MF_ARMED) != 0u;
    uint32_t lvl = t->mix_level, sig, hot = sel ? (t->mix_flags >> 4) & 7u : 0u;   /* 1 LEVEL, 2 PAN, 4 MUTE */
    int32_t m, h = st_row_h(), y = st_row_y(k % NPART), top;
    uint16_t tc = mute ? T_DIM : T_TRK(k), bg = ux_mix(T_BG, mute ? T_TRK(k) : tc, sel ? 24 : 11);
    const aafont_t *tf = TY_TX, *lf = TY_LB;
    int32_t T = pv_mix_y(h), ly = T + tf->cap + 8, cs = h >= 80 ? 34 : 26;
    char d[3];
    numeral_text(d, k);
    m = mute ? 0 : (int32_t)t->meter * (TS_MH - 2) / 127;
    if (m < pv.meter[k] - 1)
        m = pv.meter[k] - 1;                            /* (falls ~2 dB a frame) */
    sig = str_hash(str_hash(str_hash(str_hash(1u + sel * 2u + mute * 4u + arm * 8u + hot * 16u, t->name), t->mix_db),
                            t->mix_pan), d) + lvl * 7919u + (uint32_t)h * 31u + ux.gen * 977u + ux.pal * 31u + tc;
    if (!ui.force && sig == pv.mix[k]) {
        if (m != pv.meter[k]) {                         /* the meter alone */
            pv.meter[k] = (uint8_t)(m < 0 ? 0 : m);
            pv_meter(k, tc);
        }
        return;
    }
    pv.mix[k] = sig;
    pv.meter[k] = (uint8_t)(m < 0 ? 0 : m);
    for (top = 0; top < h; top += (int32_t)(CV_LOGICAL_MAX / BOX_W)) {   /* (a tall row: in slices) */
        int32_t sh = h - top < (int32_t)(CV_LOGICAL_MAX / BOX_W) ? h - top : (int32_t)(CV_LOGICAL_MAX / BOX_W), x;
        for (uint32_t band = 0; band < (uint32_t)((uint32_t)sh); band += CV_MAX / (BOX_W)) {
            cv_begin_band(BOX_W,(uint32_t)sh,T_BG,band);
            cv_oy = -top;
            if (sel) {
                cv_rrect(0, 0, BOX_W, h, 8, mute ? T_DIM : T_TRK(k), T_BG);
                cv_rrect(2, 2, BOX_W - 4, h - 4, 6, bg, mute ? T_DIM : T_TRK(k));
            } else {
                cv_rrect(0, 0, BOX_W, h, 8, bg, T_BG);
            }
            cv_rrect(6, h / 2 - cs / 2, cs, cs, 7, tc, bg);  /* the numeral */
            cv_text_c(6 + cs / 2, ty_y(h / 2, tf), tf, d, T_INK, tc);
            if (arm)
                cv_disc(6 + cs - 2, h / 2 - cs / 2 + 2, 9, T_REC, 0);
            /* the level in dB at the right, the sound before it */
            x = 228;
            if (lvl && text_w(lf, t->name) <= 228 - text_w(lf, "dB") - 3 - text_w(tf, t->mix_db) - 8 - PV_MIX_BAR_X)
                x = cv_text_r(x, ty_y(T + tf->cap / 2, lf), lf, "dB", T_MID, bg) - 3;   /* (the unit goes before the
                                                                                         * name is cut) */
            x = cv_text_r(x, ty_y(T + tf->cap / 2, tf), tf, lvl ? t->mix_db : "Off", hot == 1u ? tc : mute ? T_DIM : T_TEXT, bg);
            cv_free_text(PV_MIX_BAR_X, ty_y(T + tf->cap / 2, ty_fit(tf, t->name, x - 8 - PV_MIX_BAR_X)),
                         ty_fit(tf, t->name, x - 8 - PV_MIX_BAR_X), t->name, mute ? T_DIM : T_TEXT, bg, x - 8 - PV_MIX_BAR_X);
            /* the level's bar, the meter under it; the pan (or Mute) at the right */
            cv_rrect(PV_MIX_BAR_X, ly, PV_MIX_BAR_W, 5, 2, ux_mix(bg, T_LINE, 60), bg);
            if (lvl)
                cv_rrect(PV_MIX_BAR_X, ly, (int32_t)lvl * PV_MIX_BAR_W / 127 < 4 ? 4 : (int32_t)lvl * PV_MIX_BAR_W / 127, 5, 2,
                         hot == 1u ? T_TEXT : tc, T_LINE);
            pv_meter_draw(k, PV_MIX_BAR_X, ly + 7, tc, bg);
            if (mute)
                cv_text_r(228, ty_y(ly + 5, lf), lf, "Mute", hot == 4u ? T_TEXT : T_REC, bg);
            else
                cv_text_r(228, ty_y(ly + 5, lf), lf, t->mix_pan[0] ? t->mix_pan : "C", hot == 2u ? T_TEXT : T_MID, bg);
            cv_oy = 0;
            cv_blit_band(BOX_X, (uint32_t)(y + top));
        }
    }
}

/* ----------------------------------------------------------- draw --- */
/* RINGS: the header, the strip, the sound, then the picture (the busiest last) */
static void pv_draw(void)
{
    if (ui.force)                                       /* (the parts then cover what they draw) */
        lcd_fill(0, H_HEAD, 240, PV_SOUND_Y - H_HEAD, T_BG);
    draw_head();
    card_y = Y_LABEL;
    draw_columns();
    pv_sound();
    {   /* the picture straight on the background (no panel: the screen calm) */
        uint16_t panel = ux.panel;
        ux.panel = ux.bg;
        pv_picture();
        ux.panel = panel;
    }
}
/* a rack: the knobs' strip, the rows under it */
static void rack_draw(void)
{
    if (ui.force)
        lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    draw_head();
    card_y = Y_LABEL;
    draw_columns();
    pv_rack_rows();
}
static void mixer_draw(void)
{
    uint32_t k;
    if (ui.force)
        lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    draw_head();
    for (k = 0; k < stage_rows(); k++)                  /* (the selected track's four) */
        if (!trk_free(k))
            pv_strip(k);
        else
            stage_free(k, &pv.mix[k]);
}
