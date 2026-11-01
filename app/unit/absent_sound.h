/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's master punch effects (SLOOP's) and its inserts' running state are not on the remote: Melodee's
 * effects are Melodee's. Its pages and keys still name them (GLO's FX layer, the song's punch lane, an insert's
 * picture); these are the remote's answers: nothing playing, nothing failed, the curves straight. */
#pragma once
#define PUNCH_NFX 27u
enum { PX_LOOP4, PX_LOOP8, PX_LOOP16, PX_LOOP32, PX_STUT, PX_REV, PX_STOP, PX_HALF, PX_LPF, PX_HPF, PX_TEL, PX_CRUSH,
       PX_DOWN, PX_GATE, PX_ECHO, PX_WOBBLE, PX_KLOW, PX_KMID, PX_KHIGH, PX_RWD, PX_OCT, PX_FLANGE, PX_PAN, PX_FREEZE,
       PX_LOOP8T, PX_MONO, PX_FADE };
static const char *const PUNCH_NAME[PUNCH_NFX];
static struct { int8_t req; uint32_t keybit; } punch = {-1, 0};
static volatile uint32_t punch_panel_mask, punch_claimed;
static volatile uint8_t punch_panel_lock, punch_memory_failed;
static int8_t punch_song_req = -1;
static uint8_t punch_song_buffered;
static uint32_t punch_song_gate;
static void punch_reset(void) { punch.req = -1; punch.keybit = 0; }
static int32_t punch_fx_of_key(uint32_t k) { (void)k; return -1; }
static uint32_t punch_key_of_fx(uint32_t fx) { (void)fx; return 0; }
static int punch_needs_ring(int32_t fx) { (void)fx; return 0; }

enum { IT_OFF, IT_SOFT };
static inline int32_t ins_shape(uint32_t ty, int32_t v, uint32_t sh) { (void)ty; (void)sh; return v; }
static struct { uint8_t type, mono, decim, failed; } fx_state[FX_STATE_COUNT];
static void fx_clear_all(void) {}
