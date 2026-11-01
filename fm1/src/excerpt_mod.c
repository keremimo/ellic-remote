/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Excerpts of Melodee FM-1's mod.c as they are (tools/sync_fm1.py): the modulation matrix's names (mod.c). */
/* Modulation matrix: per track four slots, each SRC -> DST by AMT (-64..63), added to the fixed
 * routings (ENV DEST, LFO DEST). Runs in the audio ISR (mix.c mix_part, voice.c track_render).
 *
 * Sources, Q15:
 *   LFO   the track LFO with its FADE (as LFO DEST), bipolar; with LFO 2 POL UNI 0..1 (1.2)
 *   ENV   the amp envelope of the voice (after the engine's own amp curve, as ENV DEST), 0..1
 *   VEL   the note-on velocity (before UNISON's level scaling), 0..1
 *   KEY   the note relative to C4, bipolar: +-64 semitones = +-1 (an octave = 0.19)
 *   RAND  a new random value per note-on (its own generator: the shared rng is not touched), bipolar
 *   MODW  MIDI CC1, AT channel aftertouch, EXPR CC11 of the track's channel (seq.c), 0..1; until a
 *         controller arrives (and after CC121 RESET ALL CONTROLLERS) MODW and AT are 0, EXPR 1
 *   S&H   (#132) a random value held for one cycle of the track LFO: the value LFO WAVE S&H steps through
 *         (voice.c lfo_rand, drawn as each cycle starts, whatever the WAVE), without the LFO's FADE, POL or
 *         DEPTH; bipolar, 0 until the LFO's first cycle ends. It follows RATE and SYNC (a step per note value)
 *   SLEW  (#132) S&H slewed: it glides linearly from the cycle before's value to this cycle's over the cycle
 *         (a smooth random line, one cycle behind S&H, continuous where S&H steps); bipolar
 *   Both are per track (one value for every voice) and cost no state but the one value kept per cycle.
 * Destinations:
 *   per voice (vmod_t, like the fixed routings):
 *     PITCH  AMT 63 at a full source = +-11.8 semitones (as LFO DEST PIT)
 *     CUT    m->cutoff, SHP m->shape: AMT 63 = +-63 steps (as ENV DEST); the sums are clamped to what
 *            the fixed routings can reach, so the engines see no new range
 *     AMP    a gain <= 1: AMT > 0 the source opens it (AMT 64: silent at 0, full at 1), AMT < 0 closes it
 *            (bipolar sources as 0..1, as LFO DEST AMP); several AMP slots multiply
 *   per track, once per block: PAN, RATE (LFO rate), VIB (LFO DEST PIT, the
 *     vibrato depth), E1..E8 (the engine's parameters P_E0..P_E7, named by the engine): AMT 64 at a full
 *     source = the parameter's whole range; the sum is clamped to its range
 *   DEPTH (#132, per track): the depth of the track LFO, a gain <= 1 on it as AMP is on the voice (AMT > 0 the source
 *     opens it, AMT 64: none at 0, full at 1; AMT < 0 closes it; several DEPTH slots multiply): it scales the LFO
 *     wherever it goes, the four LFO DEST routings and the matrix's LFO source (not S&H / SLEW). With RATE (the LFO's
 *     rate) the matrix modulates the modulator: MODW -> DEPTH is a mod-wheel LFO, ENV -> DEPTH an LFO that
 *     follows the note's envelope. A DEPTH slot whose source is the LFO takes it unscaled
 * A per-voice source on a per-block destination takes the latest note-on's value (VEL, KEY, RAND), ENV the
 * envelope of the latest note-on's voice (one block late, 0 when it has ended).
 *
 * Per-block destinations are modulated in place: mod_begin writes the modulated values into t->p for the
 * part's block (its render and its mix in mix.c mix_part) and mod_end puts the stored values back. Both run
 * in the audio ISR, so nothing outside it ever sees a modulated value (the UI, the editor, projects and
 * presets read and write t->p from the main loop, which the ISR preempts; TIMER5 only fills the MIDI rings),
 * and the engines read t->p as before. An engine reads its parameters in note_on (events_block) unmodulated.
 * Nothing active (every slot with SRC, DST or AMT at 0): no work, and the sound is bit-identical. */
enum { MS_OFF, MS_LFO, MS_ENV, MS_VEL, MS_KEY, MS_RAND, MS_MODW, MS_AT, MS_EXPR, MS_SH, MS_SLEW, MS_N };
#define NMSLOT 4u
enum { MD_OFF, MD_PITCH, MD_CUT, MD_SHP, MD_AMP, MD_PAN, MD_RESERVED6, MD_RESERVED7, MD_RESERVED8, MD_RESERVED9, MD_RATE, MD_VIB, MD_E1,
       MD_DEPTH = MD_E1 + 8, MD_N };
#define MD_ENGINE(d) ((d) >= MD_E1 && (d) < MD_E1 + 8)           /* E1..E8: P_E0..P_E7 */
/* the name of destination value v for track t (UI, <= 5 characters): E1..E8 by the engine's labels (of the
 * engine the UI shows), "E5" for a parameter the engine does not use */
static const char *mod_dst_name(const track_t *t, int32_t v)
{
    v = clamp(v, 0, MD_N - 1);
    if (MD_ENGINE(v)) {
        const char *l = track_desc(t, P_E0 + (uint32_t)(v - MD_E1))->label;
        if (l && l[0] && l[0] != '-')
            return l;
    }
    return N_MDST[v];
}
