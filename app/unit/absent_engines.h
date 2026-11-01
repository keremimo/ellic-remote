/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's own engines are not on the remote (Melodee's instruments are Melodee's: remote_sounds.c). Its pages
 * and its sequencer still name them where an engine has pages or ways of its own (FM6's, CZ-1's, the Prophet's, the
 * drum kits'); these are the remote's answers for each: no track is ever one of them, none holds a patch, every list
 * of theirs is empty. Nothing here plays or edits a sound. */
#pragma once

/* the engines' numbers, as Melodee FM-1 stores them (none is ever offered: remote_sounds.c eng_ok) */
#define ENGI_CZ 15u

/* the patches a track of theirs would hold: bytes, never read here */
#define FM6_PACKED 128u
#define CZ_BYTES 144u
#define CZ_NATIVE 2u                      /* (P_E7 of a native CZ-1 tone's track) */
#define P5_NAME_LEN 20u
#define CZ_NAME_LEN 16u
typedef struct { uint8_t raw[CZ_BYTES]; } cz_patch_t;
typedef struct { uint8_t raw[133]; uint8_t size, model, command, group, program; } p5_patch_t;
typedef struct { uint8_t none; } fm6_part_t, cz_part_t, p5_part_t, sid_part_t, phys_slot_t, drw_part_t,
                                 drum909_part_t, dsk_part_t, dlk_part_t, drum_lane_t;
#define PHYS_POLY 1
#define DV_NLANE 8u

/* FM6: a DX7 voice's parameters and functions (its pages' cells), the patches, the SysEx it takes */
/* (a DX7 voice: six operators of 21 values, then the voice's own; its size is in the projects' song sounds) */
enum { FP_R1, FP_L1 = 4, FP_BP = 8, FP_LD, FP_RD, FP_LC, FP_RC, FP_RS, FP_AMS, FP_KVS, FP_OL, FP_MODE, FP_FC, FP_FF,
       FP_DET, FP_OP = 21, FP_PR1 = 126, FP_PL1 = 130, FP_ALG = 134, FP_FB, FP_OKS, FP_LFS, FP_LFD, FP_LPMD, FP_LAMD,
       FP_LKS, FP_LFW, FP_LPMS, FP_TRNSP, FP_NAME, FP_SIZE = 155 };
enum { FN_PBUP, FN_PBDN, FN_PBSTEP, FN_VNORM, FN_PMODE, FN_PTIME, FN_GLISS, FN_ENGINE, FN_MWR, FN_MWA, FN_FCR, FN_FCA,
       FN_BCR, FN_BCA, FN_ATR, FN_ATA, FM6_NFN };
enum { FM6_MARK1 = 1, FM6_OADD = 2, FM6_FBOUT = 4 };
#define FM6_ON_ALL 0x3Fu
static const uint8_t FM6_ALG[32][6];
static uint8_t fm6_on[NTRK], fm6_fn[NTRK][FM6_NFN];
static volatile uint8_t fm6_lgen[NTRK], fm6_pgen[NTRK], fm6_rx_ready;
static uint8_t *fm6_patch_of(uint32_t k) { static uint8_t none[FP_SIZE + 1u]; (void)k; return none; }
static void fm6_put_patch(uint32_t tr, const uint8_t *v, int load) { (void)tr; (void)v; (void)load; }
static void fm6_set_patch(uint32_t tr, const uint8_t *v) { (void)tr; (void)v; }
static void fm6_sanitize(uint8_t *v) { (void)v; }
static void fm6_track_loaded(const track_t *t) { (void)t; }
static void fm6_name(char *d, const uint8_t *v) { (void)v; d[0] = 0; }
static uint32_t fm6_carriers(uint32_t a) { (void)a; return 1u; }
static void fm6_fn_set(uint32_t tr, uint32_t k, int32_t v) { (void)tr; (void)k; (void)v; }
static uint32_t fm6_pow2(int32_t x) { (void)x; return 1u; }
static const uint8_t FM6_INIT[FM6_PACKED];
static const uint8_t FM6_FNDEF[FM6_NFN];
static int fm6_fn_ok(const uint8_t *f) { (void)f; return 1; }
static void fm6_pack(const uint8_t *v, uint8_t *b) { (void)v; memset(b, 0, FM6_PACKED); }
static void fm6_unpack(const uint8_t *b, uint8_t *v) { (void)b; memset(v, 0, FP_SIZE + 1u); }
static void fm6_init(void) {}
static void fm6_init_voice(void) {}
static void fm6_store(uint32_t k) { (void)k; }
static void fm6_send(void) {}
/* the project's wire scratch, which Melodee FM-1 keeps beside FM6's SysEx in (its eng_fm6.c): a bank record, the
 * project's own (project.c: the archives, the transfers) */
#define WIRE_SCRATCH_BYTES 27904u
static union { uint32_t align; uint8_t raw[WIRE_SCRATCH_BYTES]; } proj_wire_u __attribute__((section(".pool")));
static uint8_t proj_wire_gen;
static volatile uint8_t wire_job;
static uint8_t fm6_rx_on;

/* CZ-1: a native tone's panel (its pages' cells), the factory tones, the A/B compare */
enum { LCZ_LINE, LCZ_MOD, LCZ_OCT, LCZ_SIGN, LCZ_DOCT, LCZ_NOTE, LCZ_FINE, LCZ_VWAVE, LCZ_VRATE, LCZ_VDEP, LCZ_VDELAY,
       LCZ_GLOBALS };
enum { LCZ_W1, LCZ_W2, LCZ_KW, LCZ_KA, LCZ_LEVEL, LCZ_VP, LCZ_VW, LCZ_VA };
#define LCZ_LBASE(l) (LCZ_GLOBALS + (l) * 8u)
#define LCZ_EBASE(l, e) (LCZ_GLOBALS + 16u + ((l) * 3u + (e)) * 18u)
#define LCZ_WIN(l) (LCZ_EBASE(2, 3) + (l))
static const uint8_t CZ_ENV_END[2][3];
static const param_desc_t CZ_ED[20];
static cz_patch_t cz_compare;
static uint8_t cz_compare_tr;
static cz_patch_t *cz_patch_of(uint32_t k) { static cz_patch_t none; (void)k; return &none; }
static int cz_patch_valid(const uint8_t *b) { (void)b; return 0; }
static void cz_compare_take(uint32_t tr) { (void)tr; }
static void cz_factory_loaded(track_t *t) { (void)t; }
static void cz_track_accept(track_t *t) { (void)t; }
static void cz_init(void) {}

/* the Prophet: its patches, their names */
enum { P5_CUTOFF, P5_RESONANCE, P5_ENV_FILTER, P5_ATTACK_AMP, P5_DECAY_AMP, P5_SUSTAIN_AMP, P5_RELEASE_AMP, P5_BEND,
       P5_UNISON, P5_UNISON_COUNT, P5_UNISON_DETUNE, P5_RETRIGGER, P5_NAME };
static p5_patch_t p5_patch[NPART];
static uint8_t p5_ready[NPART];
static p5_patch_t *p5_patch_of(const track_t *t) { (void)t; return &p5_patch[0]; }
static void p5_patch_init(p5_patch_t *p) { memset(p, 0, sizeof *p); }
static void p5_patch_name(char *name, const p5_patch_t *p) { (void)p; name[0] = 0; }
static int p5_patch_valid(const p5_patch_t *p) { (void)p; return 0; }
static void p5_preset_loaded(track_t *t, uint32_t pi) { (void)t; (void)pi; }
static void p5_track_accept(track_t *t) { (void)t; }
#define P5_FACTORY_N 0u
static void p5_factory(uint32_t k, p5_patch_t *p) { (void)k; p5_patch_init(p); }

/* the drum kits' engine (seq.c drum_track: never one) and the 909's snare */
static const engine_t ENG_DRUM = {.name = "-"};
static const preset_t DRUM_PRESETS[1] = {{.name = "-"}};
enum { D9_BD, D9_SD };

/* the drum kits: a kit's lanes and sounds (the drum grid's names); a GM drum note's lane is still the grid's */
static const uint8_t DRUM_LANE_NOTE[8] = {36, 38, 42, 46, 41, 45, 39, 49};
static volatile uint8_t drum_flash[NTRK];
static uint32_t drum_event_length;
static int32_t drum_event_pitch;
static int drum_is909(const track_t *t) { (void)t; return 0; }
static int drum_issid(const track_t *t) { (void)t; return 0; }
static uint32_t drum_lane(uint32_t note)
{
    uint32_t l;
    for (l = 0; l < 8u && DRUM_LANE_NOTE[l] != note; l++)
        ;
    return l;
}
static const char *drum_character(uint32_t sound) { (void)sound; return ""; }
static const char *drum_lane_name(const track_t *t, uint32_t l) { (void)t; (void)l; return ""; }
static const char *drum_lane_abbr(const track_t *t, uint32_t l) { (void)t; (void)l; return ""; }
static uint32_t drum_mpc_note(const track_t *t, uint32_t pad) { (void)t; return 36u + pad; }
static uint32_t drum_sound_count(const track_t *t) { (void)t; return 0; }
static const char *drum_sound_name(const track_t *t, uint32_t sound) { (void)t; (void)sound; return ""; }
static const char *drum_sound_abbr(const track_t *t, uint32_t sound) { (void)t; (void)sound; return ""; }
static uint32_t drum_sound_note(const track_t *t, uint32_t sound) { (void)t; return 36u + sound; }
static uint32_t drum_sound_of(const track_t *t, uint32_t note) { (void)t; return note >= 36u ? note - 36u : 0u; }
static uint32_t drum_sound_step(const track_t *t, uint32_t sound, int32_t delta) { (void)t; (void)delta; return sound; }
static uint32_t step_lanes(const step_t *s) { (void)s; return 0; }
static uint32_t step_accents(const step_t *s) { (void)s; return 0; }
static int32_t step_sound_note(const track_t *t, const step_t *s, uint32_t sound) { (void)t; (void)s; return (int32_t)sound; }
static void step_to_grid(step_t *s) { (void)s; }
