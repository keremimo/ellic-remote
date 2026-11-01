/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* What Melodee FM-1's pages and controls read and ask of its model (its sequencer, patterns, takes, song, motion),
 * on a pure remote: Melodee's. The remote keeps no music of its own and plays none.
 *   the mirror: what the pages read (core.h's song and tracks: the parameters, the playing pattern's steps, the
 *     playhead; the patterns that hold something; the takes' notes; the song's rows), as Melodee says it (link.c);
 *   the requests: what a control asks (play, record, a pattern chosen, copied, cleared, a track added or removed,
 *     undo), said to Melodee (link_request); what it changes in place (a value, a step) Melodee hears from link.c,
 *     which keeps the mirror as Melodee last said it and tells what differs;
 *   none here: motion, a pattern's own sound, the note history, a capture of what was played. */

static void link_request(uint32_t what, uint32_t a, uint32_t b, uint32_t c);   /* (link.c) */
enum { RQ_PLAY = 1, RQ_STOP, RQ_RESTART, RQ_ARM, RQ_PATTERN, RQ_PAT_COPY, RQ_PAT_CLEAR, RQ_PAT_DOUBLE, RQ_PAT_HALF,
       RQ_TRACK_ADD, RQ_TRACK_REMOVE, RQ_UNDO, RQ_ERASE, RQ_TAKE_REMOVE, RQ_TAKES_CLEAR, RQ_PANIC, RQ_SAVE,
       RQ_ACTION = 20 };

/* ------------------------------------------------------------- MIDI --- */
/* what the pages read of USB (usb.c): up while fm1-base has a host */
static struct { uint8_t up, config; volatile uint8_t suspended; uint32_t setups, rx_pkts, sof_seen; } usb;
static void midi_out_event(uint32_t pkt) { plat_midi_send(pkt, U_TO_USB | U_TO_BLE); }

/* --------------------------------------------- the transport, the arm --- */
static volatile uint8_t transport_req;         /* the controls': 1 play, 2 stop, 3 from the top (model_poll) */
static volatile uint8_t panic_req;             /* bit per track: its notes off */
static volatile uint32_t cin_left;             /* counting in (Melodee's) */
static int seq_counting(void) { return cin_left != 0; }
static volatile uint32_t beat_pos, beat_n;     /* where in the beat (Melodee's) */
static volatile uint16_t seq_loops[NTRK], seq_pend[NTRK];
static volatile uint8_t seq_hush;
static volatile uint8_t midi_hint;
#define MIDI_TRACK_FOLLOW 0x80u
static uint8_t midi_sync_active;
static uint32_t midi_beat_samples;
#define MIDI_BEAT_UNITS 24576u
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};
static uint32_t beat_samples(void) { return (uint32_t)FS * 60u / (uint32_t)clamp(song.g[G_BPM], 20, 999); }
static uint32_t division_samples(uint32_t quarter, uint32_t div)
{
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : quarter / DIV_DEN[div % 6u];
}
static uint32_t seq_beat_samples(void) { return beat_samples(); }
static uint32_t seq_div_samples(uint32_t div) { return division_samples(seq_beat_samples(), div); }
static volatile uint32_t kb_mask, kb_layer;    /* the keys a layer or the patterns hold (ui_layer.c, remote_keys.c) */
static volatile uint8_t kb_lock, kb_asleep;
static uint8_t last_note = 60;
static uint32_t live_held[NTRK][4], live_last[NTRK][4];

static void rec_set(track_t *t, uint32_t on) { link_request(RQ_ARM, (uint32_t)(t - trk), on, 0); }
static void recording_stop(track_t *t) { (void)t; }

/* the erase gesture (EDIT held while recording: seq_erase.c): asked of Melodee */
static volatile uint32_t seq_erase_request, seq_erase_generation;
static uint32_t seq_erase_transport;
static uint8_t seq_erase_seen[NTRK];
static volatile uint16_t seq_erase_button;
static int seq_erase_active(const track_t *t) { (void)t; return 0; }

/* the step entry's MIDI and the preview of a sound (Melodee hears the keys itself) */
#define STEP_MIDI_Q 64u
static uint32_t step_midi_q[STEP_MIDI_Q];
static volatile uint32_t step_midi_w, step_midi_r;
static volatile uint8_t step_midi_overflow;
static volatile uint8_t audition_request;
static uint8_t audition_track, audition_count, audition_notes[12], audition_vel[12];
static uint32_t audition_gate;
static int32_t audition_pitch;

/* ---------------------------------------------------- the tracks' names --- */
/* each track's name as Melodee tells it (TRACKS): Stage's cards and the mixer's strips show it ("": the sound's) */
#define TRK_NAME_LEN 12u
static char trk_name[NTRK][TRK_NAME_LEN + 1u];

/* ------------------------------------------------- patterns (clips) --- */
/* each track's patterns: which hold something, their lengths and speeds (pattern_timing), as Melodee says; the
 * playing one's steps are the track's own (trk[k].step) */
typedef struct { step_t step[NSTEP]; int16_t timing[4]; } pattern_t;
static uint16_t pattern_holds[NTRK];           /* bit per bank: it holds something */
static int16_t pattern_timings[NTRK][NPAT][4];
static uint16_t pattern_epoch;
static volatile uint8_t pattern_full;
static uint8_t chain_patterns[CHAIN_ROWS][NTRK];
static void pattern_init(void) {}
static void pattern_poll(void) {}
static void pattern_read(uint32_t k, uint32_t bank, pattern_t *p)   /* another pattern's steps: the playing one's */
{
    uint32_t i;
    k %= NTRK;
    if (bank == trk[k].pattern) {
        memcpy(p->step, trk[k].step, sizeof p->step);
        for (i = 0; i < 4u; i++)
            p->timing[i] = trk[k].p[P_SLEN + i];
        return;
    }
    memset(p, 0, sizeof *p);
    for (i = 0; i < NSTEP; i++)
        p->step[i].time = ST_REST;
    for (i = 0; i < 4u; i++)
        p->timing[i] = pattern_timings[k][bank % NPAT][i];
    if ((pattern_holds[k] >> (bank % NPAT)) & 1u)   /* (holds something Melodee has not shown: one note says so) */
        p->step[0].time = ST_NOTE, p->step[0].n = 1, p->step[0].note[0] = 60;
}
static step_t pattern_step(uint32_t k, uint32_t bank, uint32_t i)
{
    pattern_t p;
    pattern_read(k, bank, &p);
    return p.step[i % NSTEP];
}
static int16_t pattern_timing(uint32_t k, uint32_t bank, uint32_t i)
{
    return bank == trk[k].pattern ? trk[k].p[P_SLEN + i] : pattern_timings[k][bank % NPAT][i % 4u];
}
static uint32_t pattern_room(void) { return 1u << 16; }   /* (room enough: the patterns are Melodee's) */
static uint32_t pattern_file_cost(void) { return 0; }
static int pattern_request(track_t *t, uint32_t bank) { link_request(RQ_PATTERN, (uint32_t)(t - trk), bank, 0); return 0; }
static int pattern_copy(track_t *t, uint32_t source, uint32_t dest)
{
    link_request(RQ_PAT_COPY, (uint32_t)(t - trk), source, (uint32_t)(t - trk) << 8 | dest);
    return 0;
}
static int pattern_copy_to(track_t *ts, uint32_t source, track_t *td, uint32_t dest, int live)
{
    (void)live;
    link_request(RQ_PAT_COPY, (uint32_t)(ts - trk), source, (uint32_t)(td - trk) << 8 | dest);
    return 0;
}
static int pattern_clear(track_t *t, uint32_t bank) { link_request(RQ_PAT_CLEAR, (uint32_t)(t - trk), bank, 0); return 0; }
static int pattern_double(track_t *t, uint32_t bank, int apply)
{
    if (apply)
        link_request(RQ_PAT_DOUBLE, (uint32_t)(t - trk), bank, 0);
    return 0;
}
static int pattern_half(track_t *t, uint32_t bank, int apply)
{
    if (apply)
        link_request(RQ_PAT_HALF, (uint32_t)(t - trk), bank, 0);
    return 0;
}

/* --------------------------------------------------------- the song --- */
static chain_config_t chain_config;
static struct {
    uint8_t original[NTRK];
    chain_config_t config;
    int16_t timing[NTRK][4];
    volatile uint8_t armed, running, row, remaining;
    uint8_t slot, rec, start, seek_chase, fx_event;
    uint32_t carry, rest_pos, seek_phase[NTRK];
    uint64_t seek_samples[NTRK];
} chain;
static struct { uint8_t n, pat[CHAIN_ROWS][NTRK], rep[CHAIN_ROWS]; } jam;   /* (a jam taken into the song: Melodee's) */
static uint32_t chain_div_samples(uint64_t value, uint32_t divisor, uint32_t *remainder)
{
    if (remainder)
        *remainder = (uint32_t)(value % divisor);
    return (uint32_t)(value / divisor);
}
static uint64_t chain_row_samples(uint32_t row)        /* a row's length (its four tracks' longest pattern) */
{
    uint64_t most = 0;
    for (uint32_t k = 0; k < NPART; k++) {
        uint32_t b = chain_config.row[row % CHAIN_ROWS].slot, len = (uint32_t)clamp(pattern_timing(k, b, 0), 1, NSTEP);
        uint64_t n = (uint64_t)len * seq_div_samples((uint32_t)pattern_timing(k, b, 1));
        if (n > most)
            most = n;
    }
    return most * (uint32_t)clamp(chain_config.row[row % CHAIN_ROWS].repeat, 1, 16);
}
static uint32_t chain_sound_id(uint32_t row, uint32_t track) { (void)row; (void)track; return 0; }

/* ------------------------------------------------------------ takes --- */
/* the takes' notes (the timed notes recorded over a pattern's steps), as Melodee says them: lists per pattern */
static recorded_note_t recording[RECORD_MAX] __attribute__((section(".pool")));
static uint16_t recording_next[RECORD_MAX] __attribute__((section(".pool")));
static uint16_t recording_head[NTRK * NPAT] = {[0 ... NTRK * NPAT - 1u] = RECORD_MAX};   /* (RECORD_MAX: none) */
static volatile uint32_t recording_generation;
static volatile uint8_t recording_full, recording_quant_blocked;
static uint8_t recording_undo_lost;
static void recording_reindex(void) {}
static void takes_none(void)                        /* no takes: every list empty */
{
    uint32_t i;
    for (i = 0; i < NTRK * NPAT; i++)
        recording_head[i] = RECORD_MAX;
    for (i = 0; i < RECORD_MAX; i++)
        recording_next[i] = RECORD_MAX;
}
static void recording_prune(track_t *t) { (void)t; }
static void recording_unlink(uint32_t i) { (void)i; }
static int recording_sounding(uint32_t i) { (void)i; return 0; }
static recorded_note_t recording_snapshot(uint32_t i) { return recording[i % RECORD_MAX]; }
static void recording_restore_note(track_t *t, uint32_t i, recorded_note_t r) { (void)t; (void)i; (void)r; }
static void recording_remove(track_t *t, uint32_t i) { link_request(RQ_TAKE_REMOVE, (uint32_t)(t - trk), i, 0); }
static void recording_clear(track_t *t) { link_request(RQ_TAKES_CLEAR, (uint32_t)(t - trk), 0, 0); }
static int recording_undo_apply(int redo) { link_request(RQ_UNDO, (uint32_t)redo, 0, 0); return 1; }

/* none here: what was played kept for a capture, the note history */
#define CAP_N 128u
typedef struct { uint16_t step, dur; uint8_t on, note, vel, tr; } cap_ev_t;
static cap_ev_t cap_ev[CAP_N];
static uint32_t cap_w, cap_from;
static void cap_select(uint32_t k) { (void)k; }
static uint32_t cap_len(const cap_ev_t *e, uint32_t now) { (void)e; (void)now; return 0; }
static uint32_t cap_now(const track_t *t) { (void)t; return 0; }
static uint32_t note_history_bytes(uint32_t count) { (void)count; return 0; }
static void note_history_capture(recorded_note_t *notes, uint32_t count, int owner) { (void)notes; (void)count; (void)owner; }
static uint32_t note_history_count(int owner) { (void)owner; return 0; }
static void note_history_expand(recorded_note_t *notes, uint32_t count) { (void)notes; (void)count; }
static uint32_t note_history_pack(recorded_note_t *notes) { (void)notes; return 0; }

/* ----------------------------------------------- motion, a pattern's sound: none here --- */
#define MO_NATIVE 112u
#define MO_NATIVE_N 144u
enum { MO_TAG_NONE, MO_TAG_P5, MO_TAG_CZ, MO_TAG_FM6 };
static motion_store_t motion;
static uint8_t motion_pattern[MOTION_MAX];
static uint8_t motion_full;
static uint8_t motion_nbase[NPART][MO_NATIVE_N];
static uint32_t motion_guard(void) { return 0; }
static void motion_unguard(uint32_t f) { (void)f; }
static uint32_t motion_count(const track_t *t) { (void)t; return 0; }
static int motion_param(uint32_t id) { (void)id; return 0; }
static int motion_capture(track_t *t, uint32_t id, int16_t value) { (void)t; (void)id; (void)value; return 0; }
static void motion_clear_bank(track_t *t, uint32_t b) { (void)t; (void)b; }
static void motion_reset(track_t *t) { (void)t; }
static void motion_restore(track_t *t) { (void)t; }
static void motion_rebase(track_t *t) { (void)t; }
static void motion_snapshot_track(track_t *t, motion_store_t *out) { (void)t; memset(out, 0, sizeof *out); }
static int motion_replace_track(track_t *t, const motion_store_t *in) { (void)t; (void)in; return 0; }

typedef struct {
    uint8_t p[P_COUNT], engine, preset, user[2], user_native, fm_on;
    char name[16];
    union { uint8_t fm[FP_SIZE + 1u]; cz_patch_t cz; p5_patch_t p5; drum_patch_t drum; } patch;
} song_sound_t;
static uint8_t song_sound_count, song_base_reserved;
static struct { uint8_t mask, active[NTRK]; } song_sounds;
static uint8_t pattern_sound[NTRK][NPAT];
static uint32_t pattern_sound_id(uint32_t k, uint32_t bank) { (void)k; (void)bank; return 0; }
static int pattern_sound_reserve(uint32_t k) { (void)k; return 0; }
static void pattern_sound_switch(uint32_t k, uint32_t bank) { (void)k; (void)bank; }
static int pattern_sounds_any(void) { return 0; }
static uint32_t resolved_sound_id(uint32_t row, uint32_t k) { (void)row; (void)k; return 0; }
static const song_sound_t *song_sound_get(uint32_t id) { (void)id; return 0; }
static song_sound_t *song_sound_pool(void) { return 0; }
static int song_sound_valid(const song_sound_t *s) { (void)s; return 0; }
static void song_sound_apply(uint32_t k, const song_sound_t *s) { (void)k; (void)s; }
static void song_sound_capture(song_sound_t *s, const track_t *t, const char *name) { (void)s; (void)t; (void)name; }

/* ------------------------------------------- MIDI knobs, the master --- */
static uint8_t midi_master_volume = 127;
static void midi_master_physical(uint32_t q12) { song.master_q12 = q12; }
static void midi_insert_init(void) {}

/* ------------------------------------------------------- the polling --- */
/* the controls' transport asks become requests (the pages set transport_req as Melodee FM-1's audio would take it) */
static void model_poll(void)
{
    uint32_t r = transport_req;
    if (r) {
        transport_req = 0;
        link_request(r == 1u ? RQ_PLAY : r == 2u ? RQ_STOP : RQ_RESTART, 0, 0, 0);
    }
    panic_req = 0;                             /* (a sound changed: Melodee ends its notes itself) */
}
