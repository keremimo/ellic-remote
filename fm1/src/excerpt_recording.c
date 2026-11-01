/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Excerpts of Melodee FM-1's recording.c as they are (tools/sync_fm1.py): a take's notes as the pages read them (recording.c). */
static uint32_t recording_owner(const track_t *t) { return trk_index(t) * NPAT + t->pattern; }
static uint32_t recording_view(const track_t *t, const recorded_note_t *r)
{
    (void)t;
    return r->step & 128u ? 0u : recorded_step(r) + ((r->step & 64u) != 0u);
}
static int recording_present(const recorded_note_t *r)
{
    uint32_t owner = recorded_owner(r), k = owner / NPAT, bank = owner % NPAT;
    uint32_t view = recording_view(&trk[k], r);
    if (view >= NSTEP) return 0;
    step_t step = pattern_step(k, bank, view);
    return step.time == ST_NOTE && (step.flags & SF_RECORDED);
}
static int recording_active(const track_t *t, uint32_t i)
{
    const recorded_note_t *r = &recording[i];
    if (!r->vel || recorded_owner(r) != recording_owner(t) || recorded_step(r) >= (uint32_t)t->p[P_SLEN]) return 0;
    uint32_t view = recording_view(t, r);
    return t->step[view].time == ST_NOTE && (t->step[view].flags & SF_RECORDED);
}
static uint32_t recording_prefix(const track_t *t, uint32_t period, uint32_t idx)
{
    uint32_t a = step_samples(t, period, 0), b = step_samples(t, period, 1);
    return (idx / 2u) * (a + b) + (idx & 1u) * a;
}
static uint32_t recording_loop(const track_t *t, uint32_t period)
{
    return recording_prefix(t, period, (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP));
}
/* Onsets are fractions within their original swung step. Durations are
 * nominal-step fractions with a compact exponent. Optional quantization finds the nearest swung grid boundary,
 * considering the end of the loop as another copy of zero. */
static uint32_t recording_raw_on(const track_t *t, const recorded_note_t *r, uint32_t period)
{
    uint32_t loop = recording_loop(t, period);
    uint32_t span = step_samples(t, period, recorded_step(r));
    return (recording_prefix(t, period, recorded_step(r)) + (uint32_t)(((uint64_t)r->on * span + RECORD_UNIT / 2u) / RECORD_UNIT)) % loop;
}
/* The step an event belongs to on the grid (NOTES selection, live erase): the unswung step its onset plays in.
 * SWING stretches each even step over the next one's straight onset, so a note played on the grid for an odd step
 * is stored in the even interval. Straight, this is its actual interval. */
static uint32_t recording_grid_step(const track_t *t, const recorded_note_t *r)
{
    if (!track_swing(t)) return recorded_step(r);
    uint32_t period = seq_div_samples((uint32_t)t->p[P_SDIV]);
    return recording_raw_on(t, r, period) / period % (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
}
