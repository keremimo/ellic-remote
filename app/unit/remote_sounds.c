/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's engine table (its engines.c) on a pure remote: Melodee's instruments, none sounding here. Melodee
 * tells each (link.c INSTRUMENT, PARAM_DESC, SOUNDS): its name, its two EDIT pages' titles and eight parameters, its
 * HOME knobs and its sounds' names. Each takes an index of Melodee FM-1's table that none of Melodee FM-1's own
 * engines' pages answers to (DRUM, FM6, CZ, PROPHET, MIDI, SLICE keep theirs, unused), in Melodee's order. Until
 * Melodee has told any: one, SOUND, its eight parameters numbered. */
#include "absent_engines.h"      /* (Melodee FM-1's own engines: none of them here) */

/* the working arena (resources.c): none, Melodee FM-1 keeps its undo histories and the like there; the remote's
 * undo is Melodee's */
enum { RES_ENGINE0, RES_NOTE_HISTORY = RES_ENGINE0 + NPART, RES_FX0, RES_RECORD_UNDO = RES_FX0 + FX_STATE_COUNT,
       RES_STEP_UNDO, RES_LOAD_UNDO, RES_SONG_UNDO, RES_SONG_SOUNDS, RES_SONG_BASE, RES_SONG_PREVIEW, RES_PUNCH,
       RES_COUNT };
static struct { void *ptr; uint32_t size; } resource[RES_COUNT];
static void *resource_get(uint32_t id, uint32_t bytes) { (void)id; (void)bytes; return 0; }
static void *resource_resize(uint32_t id, uint32_t bytes) { (void)id; (void)bytes; return 0; }
static void resource_release(uint32_t id) { (void)id; }

/* the indices Melodee's instruments take, in their order */
static const uint8_t REMOTE_IDX[] = {3, 2, 4, 5, 6, 7, 8, 9, 11, 13, 14, 16};
#define REMOTE_ENGINES ((uint32_t)sizeof REMOTE_IDX)
#define REMOTE_PRESETS 128u      /* sounds an instrument lists (Melodee's longest: BRO-FAT's 68) */
#define REMOTE_TEXT 2048u        /* an instrument's names: its parameters', their values', its units, its sounds' */
#define REMOTE_NAMES 24u         /* value names a parameter has at most (F_ENUM) */

static void remote_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void remote_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}

typedef struct {
    engine_t e;
    uint16_t sound[REMOTE_PRESETS];          /* each sound's name: its place in text */
    const char *names[8][REMOTE_NAMES + 1u]; /* each parameter's value names, 0-terminated */
    char text[REMOTE_TEXT];
    uint16_t used;                           /* of text */
} remote_engine_t;
static remote_engine_t remote_eng[REMOTE_ENGINES] __attribute__((section(".pool")));
static const engine_t *ENGINES[NENGINES];
static uint8_t ENGINE_ORDER[REMOTE_ENGINES];
static uint32_t remote_eng_n;                /* the instruments Melodee has told (0: SOUND alone) */
static uint8_t TRK_DEF[NPART][2];            /* the parts at power-on (engine, sound) */
#undef NENG_SHOWN
#define NENG_SHOWN (remote_eng_n ? remote_eng_n : 1u)

static const engine_t ENG_NONE = {           /* every index no instrument has: never offered (eng_ok) */
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {{"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}},
    .note_on = remote_note_on,
    .render = remote_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
};
static const preset_t SOUND_PRESET[] = {{.name = "Init"}};
static const engine_t ENG_SOUND = {          /* before Melodee has told its instruments */
    .name = "SOUND",
    .page_title = {"EDIT 1", "EDIT 2"},
    .edit = {{"PARAM 1", F_INT, 0, 127, 64, 0, 0}, {"PARAM 2", F_INT, 0, 127, 64, 0, 0},
             {"PARAM 3", F_INT, 0, 127, 64, 0, 0}, {"PARAM 4", F_INT, 0, 127, 64, 0, 0},
             {"PARAM 5", F_INT, 0, 127, 64, 0, 0}, {"PARAM 6", F_INT, 0, 127, 64, 0, 0},
             {"PARAM 7", F_INT, 0, 127, 64, 0, 0}, {"PARAM 8", F_INT, 0, 127, 64, 0, 0}},
    .presets = SOUND_PRESET, .npresets = 1,
    .note_on = remote_note_on,
    .render = remote_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
};

/* a sound's name (engine_t.preset_make: each instrument its own, as the table holds no instrument number) */
static void remote_sound(uint32_t n, uint32_t k, preset_t *p, char *name)
{
    const remote_engine_t *r = &remote_eng[n % REMOTE_ENGINES];
    (void)p;
    str_cpy(name, k < REMOTE_PRESETS ? r->text + r->sound[k] : "", 21);
}
#define SOUND_OF(n) static void remote_sound_##n(uint32_t k, preset_t *p, char *name) { remote_sound(n, k, p, name); }
SOUND_OF(0) SOUND_OF(1) SOUND_OF(2) SOUND_OF(3) SOUND_OF(4) SOUND_OF(5) SOUND_OF(6) SOUND_OF(7) SOUND_OF(8)
SOUND_OF(9) SOUND_OF(10) SOUND_OF(11)
static void (*const REMOTE_SOUND[REMOTE_ENGINES])(uint32_t, preset_t *, char *) = {
    remote_sound_0, remote_sound_1, remote_sound_2, remote_sound_3, remote_sound_4, remote_sound_5,
    remote_sound_6, remote_sound_7, remote_sound_8, remote_sound_9, remote_sound_10, remote_sound_11};

static void remote_engines_init(void)
{
    uint32_t i;
    for (i = 0; i < NENGINES; i++)
        ENGINES[i] = &ENG_NONE;
    ENGINES[REMOTE_IDX[0]] = &ENG_SOUND;
    ENGINE_ORDER[0] = REMOTE_IDX[0];
    remote_eng_n = 0;
    for (i = 0; i < NPART; i++) {
        TRK_DEF[i][0] = REMOTE_IDX[0];
        TRK_DEF[i][1] = 0;
    }
}

/* a track's engine number as an index (engines.c) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* preset k (< npresets) of e (engines.c) */
static const preset_t *eng_preset(const engine_t *e, uint32_t k)
{
    static preset_t made[2];
    static char made_name[2][21];
    static uint8_t turn;
    if (e->presets)
        return &e->presets[k];
    turn ^= 1u;
    memset(&made[turn], 0, sizeof made[turn]);
    made_name[turn][0] = 0;
    made[turn].name = made_name[turn];
    e->preset_make(k, &made[turn], made_name[turn]);
    return &made[turn];
}

static int eng_extra_retired(uint32_t e) { (void)e; return 0; }
static int eng_ok(uint32_t e) { return e < NENGINES && ENGINES[e] != &ENG_NONE; }
static uint32_t eng_sound_idx(uint32_t e) { return eng_ok(e) ? e : ENGINE_ORDER[0]; }
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = ENGINE_ORDER[0];
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

static uint32_t trk_def_engine(uint32_t k) { return TRK_DEF[k % NPART][0]; }
static uint32_t trk_def_preset(uint32_t k) { return TRK_DEF[k % NPART][1]; }

/* no instrument's working state here (engines.c's eng_state_*) */
static void *fm6_part(uint32_t part) { (void)part; return 0; }
static uint32_t eng_state_size(uint32_t e) { (void)e; return 0; }
static void eng_state_clear(uint32_t part) { (void)part; }
static void eng_state_reset(void) {}
static int eng_state_prepare(const track_t *t) { (void)t; return 1; }

/* ------------------------------------------- Melodee's instruments in --- */
/* text kept for instrument n: 0 if there is no room left (its names shorter, then) */
static const char *remote_text(remote_engine_t *r, const char *s, uint32_t n)
{
    char *d;
    if (r->used + n + 1u > REMOTE_TEXT)
        return "";
    d = r->text + r->used;
    memcpy(d, s, n);
    d[n] = 0;
    r->used = (uint16_t)(r->used + n + 1u);
    return d;
}
/* instrument n (0..): its name, its EDIT pages' titles, HOME's four knobs (P_* ids), its sounds' count; its eight
 * parameters and sounds' names follow (remote_param, remote_sounds_named), then remote_instruments */
static void remote_instrument(uint32_t n, const char *name, uint32_t nl, const char *t1, uint32_t l1, const char *t2,
                              uint32_t l2, const uint8_t knob[4], uint32_t sounds)
{
    remote_engine_t *r;
    uint32_t i;
    if (n >= REMOTE_ENGINES)
        return;
    r = &remote_eng[n];
    memset(r, 0, sizeof *r);
    r->e = ENG_SOUND;
    r->e.presets = 0;
    r->e.preset_make = REMOTE_SOUND[n];
    r->e.npresets = (uint8_t)(sounds < REMOTE_PRESETS ? sounds : REMOTE_PRESETS);
    r->e.name = remote_text(r, name, nl);
    r->e.page_title[0] = remote_text(r, t1, l1);
    r->e.page_title[1] = remote_text(r, t2, l2);
    for (i = 0; i < 4u; i++)
        r->e.knob[i] = knob[i] < P_COUNT ? knob[i] : 255u;
    for (i = 0; i < REMOTE_PRESETS; i++)
        r->sound[i] = 0;                         /* ("" until named: the text's first byte is a name's end) */
    for (i = 0; i < 8u; i++)
        r->e.edit[i] = ENG_NONE.edit[i];
}
/* its parameter k (P_E0 + k): label, format (F_*), range, default, unit, value names (each 0-terminated, nn of them) */
static void remote_param(uint32_t n, uint32_t k, const char *label, uint32_t ll, uint32_t fmt, int32_t min, int32_t max,
                         int32_t def, const char *unit, uint32_t ul, const char *names, uint32_t nn)
{
    remote_engine_t *r;
    param_desc_t *d;
    uint32_t i;
    if (n >= REMOTE_ENGINES || k >= 8u)
        return;
    r = &remote_eng[n];
    d = &r->e.edit[k];
    d->label = remote_text(r, label, ll);
    d->fmt = (uint8_t)fmt;
    d->min = (int16_t)min;
    d->max = (int16_t)max;
    d->def = (int16_t)def;
    d->unit = ul ? remote_text(r, unit, ul) : 0;
    d->names = 0;
    if (nn) {
        for (i = 0; i < nn && i < REMOTE_NAMES; i++) {
            uint32_t len = (uint32_t)str_len(names);
            r->names[k][i] = remote_text(r, names, len);
            names += len + 1u;
        }
        r->names[k][i] = 0;
        d->names = r->names[k];
    }
}
/* its sounds from first, count names (each 0-terminated) */
static void remote_sounds_named(uint32_t n, uint32_t first, uint32_t count, const char *names)
{
    remote_engine_t *r;
    uint32_t i;
    if (n >= REMOTE_ENGINES)
        return;
    r = &remote_eng[n];
    for (i = 0; i < count && first + i < REMOTE_PRESETS; i++) {
        uint32_t len = (uint32_t)str_len(names);
        const char *t = remote_text(r, names, len);
        r->sound[first + i] = (uint16_t)(t - r->text);
        names += len + 1u;
    }
}
/* the instruments told are count: the table from them (the tracks' engine numbers are REMOTE_IDX[n]) */
static void remote_instruments(uint32_t count)
{
    uint32_t i;
    for (i = 0; i < NENGINES; i++)
        ENGINES[i] = &ENG_NONE;
    remote_eng_n = count < REMOTE_ENGINES ? count : REMOTE_ENGINES;
    for (i = 0; i < remote_eng_n; i++) {
        ENGINES[REMOTE_IDX[i]] = &remote_eng[i].e;
        ENGINE_ORDER[i] = REMOTE_IDX[i];
    }
    if (!remote_eng_n) {
        ENGINES[REMOTE_IDX[0]] = &ENG_SOUND;
        ENGINE_ORDER[0] = REMOTE_IDX[0];
    }
}
