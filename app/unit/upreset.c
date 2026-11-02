/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's user sounds (its upreset.c and the banks it keeps on the FM-1: general, FM6, CZ-1 and Prophet
 * slots, their favourites, imports) are not on the remote: the user's sounds are Ellic Studio's. Its pages still ask;
 * these answer that no slot holds a sound, and refuse to keep one here. */
static uint32_t up_gen;                              /* (no slot ever changes) */
static int up_used(uint32_t k) { (void)k; return 0; }
static uint32_t up_engine(uint32_t k) { (void)k; return 0; }
static void up_name(uint32_t k, char *b) { (void)k; b[0] = 0; }
static void up_slot_label(char *b, uint32_t k)       /* "U007" (as Melodee FM-1 labels them) */
{
    b[0] = 'U';
    b[1] = (char)('0' + (k + 1u) / 100u % 10u);
    b[2] = (char)('0' + (k + 1u) / 10u % 10u);
    b[3] = (char)('0' + (k + 1u) % 10u);
    b[4] = 0;
}
static int up_load(uint32_t k) { (void)k; return 0; }
static int up_favorite(uint32_t k) { (void)k; return 0; }
static int up_favorite_set(uint32_t k, int on) { (void)k; (void)on; return 0; }

/* the engines' own slots (FM6 voices, CZ-1 tones, Prophet programs): none */
static int native_used(uint32_t e, uint32_t k) { (void)e; (void)k; return 0; }
static void native_name(uint32_t e, uint32_t k, char *out) { (void)e; (void)k; out[0] = 0; }
static int native_load(uint32_t e, uint32_t k, uint32_t tr) { (void)e; (void)k; (void)tr; return 0; }
static int p5_favorite_has(uint32_t slot, int factory) { (void)slot; (void)factory; return 0; }
static int p5_favorite_set(uint32_t slot, int on, int factory) { (void)slot; (void)on; (void)factory; return 0; }
