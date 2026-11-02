/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's audio interrupt (its audio.c) on the remote: no sound here. What its pages read of the sound, the
 * scope (HOME's oscilloscope), is Ellic Studio's, as it sends it (link.c SCOPE). */
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;

/* CPU1's worker (audio_worker.h): never online, nothing to hand it */
static uint8_t audio_worker_online;
typedef void (*audio_worker_fn)(void *);
