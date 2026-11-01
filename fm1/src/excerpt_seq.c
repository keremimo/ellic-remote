/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Excerpts of Melodee FM-1's seq.c as they are (tools/sync_fm1.py): the keys, the scale, the tracks' channels (seq.c). */
#define KB_SILENT 255u
static uint32_t trk_index(const track_t *t) { return (uint32_t)(t - trk); }
static uint32_t trk_midi_ch(uint32_t i)
{ return midi_track_only(&trk[i % NTRK]) ? midi_track_channel(&trk[i % NTRK]) : i % NTRK; }
static int drum_track(const track_t *t) { return ENGINES[eng_idx(t->eng_req)] == &ENG_DRUM; }
/* the 27 keys from F: black or white, and the key's place among the keys of its colour (white 0..15, black
 * 0..10). The DRUM grid: white keys are steps, black keys 1..8 lanes, 9 ACC, 10 / 11 the page */
static int key_black(uint32_t k) { return (int)((0x54Au >> ((k + 5u) % 12u)) & 1u); }
static uint32_t key_place(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < k; i++)
        n += (uint32_t)(key_black(i) == key_black(k));
    return n;
}
enum { GK_ACC = NLANE, GK_PGDN, GK_PGUP };
static uint32_t scale_mask(const track_t *t)
{
    const micro_scale_t *s = micro_scale(t);
    if (s) { /* approximate pitch classes for the keyboard LEDs only */
        uint32_t mask = 0;
        for (uint32_t i = 0; i < s->count; i++)
            mask |= 1u << ((s->pitch[i] + 5000) / 10000 % 12);
        return mask;
    }
    return SCALE_MASK[clamp(t->p[P_SCALE], 0, SCALE_LEGACY - 1)];
}
static uint32_t scale_count(const track_t *t)
{
    if (micro_scale(t)) return micro_scale(t)->count;
    uint32_t mask = scale_mask(t), count = 0;
    while (mask) {
        count += mask & 1u;
        mask >>= 1;
    }
    return count;
}
static int32_t mpc_degree(const track_t *t) { return clamp(t->p[P_MPCDEG], 1, (int32_t)scale_count(t)); }
/* scale degree `degree` from C4 (0 = the ROOT above C4) in the track's scale, + offset semitones and TRN. strict: a
 * note off the MIDI range is silent (else clamped). Scales of 5, 6, 8 or 12 notes repeat no degree */
static uint32_t scale_degree_map(const track_t *t, int32_t degree, int32_t offset, int strict)
{
    if (micro_active(t)) return micro_degree_map(t, degree, offset, strict);
    uint32_t mask = scale_mask(t), i;
    int32_t count = (int32_t)scale_count(t), oct, n;
    oct = degree / count;
    degree %= count;
    if (degree < 0) {
        degree += count;
        oct--;
    }
    for (i = 0; i < 12u; i++)
        if ((mask >> i) & 1u) {
            if (!degree)
                break;
            degree--;
        }
    n = 60 + t->p[P_ROOT] + 12 * oct + (int32_t)i + offset + t->p[P_TRANS];
    if (strict && (n < 0 || n > 127))
        return KB_SILENT;
    return (uint32_t)clamp(n, 0, 127);
}
/* note n (a key's or a MIDI note) through the WHITE / ALL layouts. WHITE (and MPC on the keys): the white keys walk
 * the scale from C4 = ROOT, the black keys are silent. ALL: every key the next degree of the scale */
static uint32_t scale_map(const track_t *t, int32_t n, int32_t offset)
{
    static const int8_t DEGREE[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    int32_t degree;
    if (t->p[P_QUANT] == Q_ALL)
        return scale_degree_map(t, n - 60, offset, 1);
    if (t->p[P_QUANT] == Q_WHITE || t->p[P_QUANT] == Q_MPC) {
        if ((degree = DEGREE[n % 12]) < 0)
            return KB_SILENT;
        return scale_degree_map(t, degree + (n / 12 - 5) * 7, offset, 0);
    }
    return (uint32_t)clamp(n + offset + t->p[P_TRANS], 0, 127);
}
static uint32_t kb_map(const track_t *t, uint32_t k)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];   /* (the engine it switches to) */
    int32_t n;
    if (e->keys && (n = e->keys(t, k)) >= 0)           /* the engine's own key map (GM kit, slices) */
        return (uint32_t)n;
    n = 53 + (int32_t)k;
    if (t->p[P_QUANT] == Q_SNAP) {                     /* SNAP: every key, rounded down to the scale */
        if (micro_active(t)) {
            uint32_t note = micro_snap(t, n + t->p[P_TRANS]);
            return note == KB_SILENT ? note : micro_degree_map(t, (int32_t)note - 60, 12 * input_octave(t), 1);
        }
        uint32_t mask = scale_mask(t), guard = 12;
        n += 12 * input_octave(t) + t->p[P_TRANS];
        while (guard-- && !((mask >> (uint32_t)((n - t->p[P_ROOT] + 120) % 12)) & 1u))
            n--;
        return (uint32_t)clamp(n, 0, 127);
    }
    return scale_map(t, n, 12 * input_octave(t));
}
static int rec_on(const track_t *t) { return (((song.rec & ~song.rec_wait) >> trk_index(t)) & 1u) && song.playing && !chain.running; }
static int rec_any(void) { return (song.rec & ~song.rec_wait) && song.playing && !chain.running; }
/* the length of step idx in samples: SWING (the track's + the global, at most 100) makes the even steps longer
 * and the odd ones shorter, so every odd step starts late */
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx)
{
    return swing_step_len(t, period, idx);   /* core.h: own + global, at most 100 */
}
/* Map a quantized onset back to the compact tempo-relative event format. The
 * inverse swung pair calculation is constant-time, even at a 64-step boundary. */
static void recording_quant_position(const recorded_note_t *r, uint32_t at, uint32_t a, uint32_t b, uint32_t len, recorded_note_t *out)
{
    uint32_t actual = at / (a + b) * 2u, pos = at % (a + b), span = a;
    if (pos >= a) { actual++; pos -= a; span = b; }
    uint32_t fraction = span <= 65536u ? (pos * RECORD_UNIT + span / 2u) / span :
        (uint32_t)(((uint64_t)pos * RECORD_UNIT + span / 2u) / span);
    *out = *r;
    out->on = (uint16_t)(fraction > 65535u ? 65535u : fraction);
    uint32_t rounded = out->on >= RECORD_UNIT / 2u;
    recorded_set_step(out, actual, rounded, rounded && actual + 1u == len);
}
