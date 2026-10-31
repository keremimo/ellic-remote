/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments: his Felucca code, used under MPL-2.0
 * with his permission (hugelton/felucca-bsp#4) */
/* What Melodee FM-1's drawing reads, on the remote: its layout (ui.c), the UI's state that drawing keeps (ui.c's ui),
 * the little helpers its pages share (note names, the chord recogniser, words in lines and in sentence case: ui.c,
 * ui_graph.c, ui_menu.c), and the mirror in the model's place: the page shown reads Melodee's (mel_proto.h
 * mel_v2_t) through S, V and TSEL. Part of app.c, before the pages (ui_draw.c). */

/* ------------------------------------------------------------ layout (ui.c) --- */
#define Y_HEAD 0
#define H_HEAD 30
#define Y_LABEL 32                    /* the strip (the knobs' cells) */
#define Y_SEP_END 80
#define CARD_W 57
#define CARD_H 48
#define CARD_X(c) (2 + 60 * (int32_t)(c))
#define Y_GRAPH 84                    /* the panel */
#define H_GRAPH 124
#define Y_FOOT 212                    /* the sound bar */
#define H_FOOT 28
#define BOX_X 2                       /* panels and rows: x 2 .. 237 */
#define BOX_W 236
#define TL_MAX 4u
#define NPART 4u                      /* the places a page shows (Melodee FM-1's group of four tracks) */
#define NTRK MEL_PLACES
#define NSTEP MEL_NSTEP
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define ACC T_THEME                   /* values, curves */
#define VAL(c) ((void)(c), T_TEXT)    /* a value (the knob just turned: its card tinted and outlined, ui_draw.c) */
#define HOT_FRAMES 40u                /* (ui_input.c: a knob turned stays hot this many frames) */

/* ------------------------------------------------- the UI's state (ui.c's ui) --- */
static struct {
    uint8_t hot_col, hot_t;      /* column whose knob was just turned (drawn white) */
    uint8_t force;               /* full redraw pending */
    uint8_t bpm_t;               /* frames the BPM stays highlighted */
    uint8_t page, layer;         /* the page drawn last (MEL_PG_*), whose screen (1: the remote's own) */
    uint8_t popup;               /* the overlay drawn last: 0 none, 1 a sheet, 2 a question, 3 a picker */
    uint8_t toast;               /* a toast is up (draw_toast) */
    char msg[48];                /* the header's message (a toast when too long) */
    /* drawn-state cache */
    char col[4][32];
    uint32_t graph_sig, head_sig, foot_sig, layer_sig, pop_sig, frame;
    /* rolling digits (ui_draw.c roll_*): the four card values and the header BPM */
    struct {
        char from[7];            /* the value rolling out; "" = idle */
        uint8_t t0;              /* (ui.frame) of the value's last change */
        int8_t dir;              /* +1: it went up (the old digit leaves upward), -1: down */
        uint8_t sig;             /* a card: what its value is of (label, unit, track, palette) */
    } roll[5];
    char roll_bpm[MEL_BPM_N + 1];   /* the BPM last drawn */
} ui;

/* the screen drawn: Melodee's, or the remote's own (app.c); its version 2 part; the selected place's track */
static const mel_screen_t *S;
#define V (&S->v2)
#define TSEL (&V->trk[V->sel % NTRK])
#define TRK(k) (&V->trk[(k) % NTRK])

/* ---------------------------------------------------- the platform's libc --- */
#ifdef PLAT_HOST                      /* (felucca-bsp's libc.c has them on the device) */
static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
static unsigned str_len(const char *s)
{
    unsigned n = 0;
    while (s[n])
        n++;
    return n;
}
static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1 < max) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}
static void fmt_fix(char *b, int32_t v, int decimals)
{
    char t[12];
    int n = 0, neg = v < 0, i;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
        if (n == decimals)
            t[n++] = '.';
    } while (u || n <= decimals);
    if (neg)
        t[n++] = '-';
    for (i = 0; i < n; i++)
        b[i] = t[n - 1 - i];
    b[n] = 0;
}
static void fmt_int(char *b, int32_t v) { fmt_fix(b, v, 0); }
#endif

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}
static uint32_t mem_hash(uint32_t h, const void *p, uint32_t n)
{
    const uint8_t *b = p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}

/* ------------------------------------------------------- notes (params.c, ui.c) --- */
static const char *const N_NOTE[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static void note_name(char *b, uint32_t n)
{
    str_cpy(b, N_NOTE[n % 12u], 4);
    fmt_int(b + str_len(b), (int32_t)(n / 12u) - 1);
}

/* HOME note/chord names (ui_graph.c): the same recognizer as next, with the 1.0 fonts and palette. */
static const struct {
    uint16_t iv;                                     /* bit i: i semitones over the root */
    char q[8];
} CHORDS[] = {                                       /* simplest first: a chord off its bass takes the first that fits */
    {0x091, ""}, {0x089, "m"}, {0x049, "dim"}, {0x111, "aug"}, {0x0A1, "sus4"}, {0x085, "sus2"},
    {0x491, "7"}, {0x891, "maj7"}, {0x489, "m7"}, {0x449, "m7b5"}, {0x249, "dim7"}, {0x4A1, "7sus4"},
    {0x291, "6"}, {0x289, "m6"}, {0x095, "add9"}, {0x08D, "madd9"}, {0x0B1, "add11"}, {0x0A9, "madd11"},
    {0x889, "mM7"}, {0x511, "7#5"}, {0x451, "7b5"}, {0x911, "maj7#5"}, {0x849, "dimM7"},
    {0x495, "9"}, {0x895, "maj9"}, {0x48D, "m9"}, {0x4A5, "9sus4"}, {0x295, "6/9"}, {0x28D, "m6/9"},
    {0x493, "7b9"}, {0x499, "7#9"}, {0x4D1, "7#11"}, {0x591, "7b13"}, {0x8D1, "maj7#11"},
    /* 11ths and 13ths, also without the 5th or the 9th */
    {0x4B5, "11"}, {0x4AD, "m11"}, {0x4A9, "m11"}, {0x42D, "m11"},
    {0x695, "13"}, {0x691, "13"}, {0x615, "13"}, {0x611, "13"},
    {0x6AD, "m13"}, {0x68D, "m13"}, {0x689, "m13"}, {0x60D, "m13"},
    {0xA95, "maj13"}, {0xA91, "maj13"}, {0xA15, "maj13"},
    /* 7ths and 9ths without the 5th */
    {0x411, "7"}, {0x811, "maj7"}, {0x409, "m7"}, {0x415, "9"}, {0x815, "maj9"}, {0x40D, "m9"},
    {0x413, "7b9"}, {0x419, "7#9"}, {0x851, "maj7#11"},
};
#define NCHORDS (sizeof CHORDS / sizeof CHORDS[0])

/* the chord of pitch classes pcs, *root its root: on the bass when it makes one,
 * else the simplest on another root (shown "/bass") */
static const char *chord_of(uint32_t pcs, uint32_t bass, uint32_t *root)
{
    uint32_t i, k, best = NCHORDS;
    for (i = 0; i < 12u; i++) {
        uint32_t r = (bass + i) % 12u, iv = ((pcs >> r) | (pcs << (12u - r))) & 0xFFFu;
        if (!((pcs >> r) & 1u))
            continue;
        for (k = 0; k < best; k++)
            if (CHORDS[k].iv == iv) {
                best = k;
                *root = r;
                break;
            }
        if (!i && best < NCHORDS)
            break;                                   /* on the bass: no slash */
    }
    return best < NCHORDS ? CHORDS[best].q : 0;
}

/* the lowest of n notes (note[] holds the first 8) that fit in w px of font f, "C4 E4 G4",
 * " .." when some are left out; returns how many are shown */
static uint32_t notes_fit(char *b, const uint8_t *note, uint32_t n, const aafont_t *f, int32_t w)
{
    uint32_t m, i;
    for (m = n < 8u ? n : 8u; m > 1u; m--) {
        b[0] = 0;
        for (i = 0; i < m; i++) {
            if (i)
                str_cpy(b + str_len(b), " ", 2);
            note_name(b + str_len(b), note[i]);
        }
        if (m < n)
            str_cpy(b + str_len(b), " ..", 4);
        if (text_w(f, b) <= w)
            return m;
    }
    note_name(b, note[0]);
    return 1;
}

/* ------------------------------------------------------------ words (ui.c, ui_menu.c) --- */
/* s in up to m lines of w px in face f, broken at spaces (one long word: as much of it as fits); how many */
static uint32_t text_lines(const char *s, char (*l)[32], uint32_t m, int32_t w, const aafont_t *f)
{
    uint32_t k = 0;
    while (*s && k < m) {
        uint32_t i, cut = 0;
        while (*s == ' ' || *s == '\n')
            s++;
        for (i = 1; s[i - 1] && i < 32u; i++) {
            str_cpy(l[k], s, i + 1u);
            if (text_w(f, l[k]) > w)
                break;
            if (!s[i] || s[i] == ' ' || s[i] == '\n')
                cut = i;
            if (s[i] == '\n')
                break;
        }
        if (!cut) {                                     /* (one long word: as much of it as fits) */
            for (cut = 1; s[cut] && cut < 31u; cut++) {
                str_cpy(l[k], s, cut + 2u);
                if (text_w(f, l[k]) > w)
                    break;
            }
        }
        str_cpy(l[k++], s, cut + 1u);
        s += cut;
    }
    return k;
}

/* "AUDIO CLICK" -> "Audio click", "P5 OSC A" -> "Osc A" (acronyms and a letter alone stay capitals) */
static void menu_words(char *d, const char *s, uint32_t n)
{
    static const char *const AC[] = {"USB", "FX", "MIDI", "LFO", "EG", "FM", "OP", "PW", "DCW", "DCA", "DCO", "MPC", "GLO"};
    uint32_t i, w = 0, a;
    str_cpy(d, s, n);
    for (i = 0; d[i]; i++) {
        int first = !i, ac = 0;
        if (d[i] == ' ' || d[i] == '+' || d[i] == '-' || d[i] == '/') { w = i + 1u; continue; }
        if (i == w) {                                   /* a word: an acronym, a letter alone stay */
            for (a = 0; a < NELEM(AC) && !ac; a++) {
                uint32_t l = str_len(AC[a]);
                ac = !memcmp(d + w, AC[a], l) && (!d[w + l] || d[w + l] == ' ' || d[w + l] == '/' || (d[w + l] >= '0' &&
                     d[w + l] <= '9'));
            }
            ac |= !d[i + 1] || d[i + 1] == ' ';
        }
        if (ac) { while (d[i + 1] && d[i + 1] != ' ' && d[i + 1] != '/') i++; continue; }
        if (!first && d[i] >= 'A' && d[i] <= 'Z') d[i] = (char)(d[i] + 32);
    }
}

/* -------------------------------------------------------- the mirror's helpers --- */
static int trk_free(uint32_t k) { return !TRK(k)->numeral; }   /* a place with no track: "Add track" */
static uint32_t trk_numeral(uint32_t k) { return TRK(k)->numeral ? TRK(k)->numeral : k + 1u; }
static int song_playing(void) { return V->transport != MEL_TR_STOPPED; }
static uint32_t stage_rows(void) { return V->shown < 1u ? 1u : V->shown > NPART ? NPART : V->shown; }
static int step_on_at(const mel_track_t *t, uint32_t i) { return mel_step(t, i) == 1u; }
static uint32_t steps_hash(const mel_track_t *t) { return mem_hash(2166136261u + t->len, t->steps, sizeof t->steps); }
static void numeral_text(char *d, uint32_t k)        /* a place's numeral as its chip shows it */
{
    uint32_t n = trk_numeral(k);
    if (n >= 10u)
        *d++ = (char)('0' + n / 10u % 10u);
    *d++ = (char)('0' + n % 10u);
    *d = 0;
}
