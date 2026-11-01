/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's user sounds (its upreset.c and the banks it keeps on the FM-1: general, FM6, CZ-1 and Prophet
 * slots, their favourites, imports) are not on the remote: the user's sounds are Melodee's. Its pages still ask;
 * these answer that no slot holds a sound, and refuse to keep one here. */
static uint32_t up_gen;                              /* (no slot ever changes) */
static int up_used(uint32_t k) { (void)k; return 0; }
static int up_old(uint32_t k) { (void)k; return 0; }
static int up_old_bank(uint32_t k) { (void)k; return 0; }
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
static void up_auto_name(char *b, uint32_t e, uint32_t k) { (void)e; up_slot_label(b, k); }
static int up_load(uint32_t k) { (void)k; return 0; }
static uint32_t up_count(void) { return 0; }
static uint32_t up_nth(uint32_t n) { return n; }
static uint32_t up_rank(uint32_t slot) { (void)slot; return 0; }
static void up_ui_named(uint32_t op, uint32_t k, const char *name)   /* 0 load, 1 erase, 2 save, 3 rename */
{
    (void)op; (void)k; (void)name;
    ui_say("SOUNDS ", "ON MELODEE");
}
static void up_ui(uint32_t op, uint32_t k) { up_ui_named(op, k, 0); }
static void up_boot(void) {}
static void up_cache_reset(void) {}
static int up_favorite(uint32_t k) { (void)k; return 0; }
static int up_favorite_set(uint32_t k, int on) { (void)k; (void)on; return 0; }
static void up_favorite_poll(void) {}
static int shared_wipe(void) { return 0; }
static int shared_import(uint32_t e, uint32_t first, uint32_t count, const uint8_t *raw, uint32_t stride, uint32_t *saved)
{
    (void)e; (void)first; (void)count; (void)raw; (void)stride;
    *saved = 0;
    return 0;
}

/* the engines' own slots (FM6 voices, CZ-1 tones, Prophet programs): none */
static uint32_t native_limit(uint32_t e) { (void)e; return 0; }
static int native_used(uint32_t e, uint32_t k) { (void)e; (void)k; return 0; }
static int native_old(uint32_t e, uint32_t k) { (void)e; (void)k; return 0; }
static void native_name(uint32_t e, uint32_t k, char *out) { (void)e; (void)k; out[0] = 0; }
static int native_load(uint32_t e, uint32_t k, uint32_t tr) { (void)e; (void)k; (void)tr; return 0; }
static const uint8_t *native_raw(uint32_t e, uint32_t k) { (void)e; (void)k; return 0; }
static int native_store(uint32_t e, uint32_t k, uint32_t tr, const char *name) { (void)e; (void)k; (void)tr; (void)name; return 0; }
static int native_put(uint32_t e, uint32_t k, const uint8_t *raw) { (void)e; (void)k; (void)raw; return 0; }
static uint32_t native_size(uint32_t e) { (void)e; return 0; }
static void native_cache_reset(void) {}
static int p5_favorite_has(uint32_t slot, int factory) { (void)slot; (void)factory; return 0; }
static int p5_favorite_set(uint32_t slot, int on, int factory) { (void)slot; (void)on; (void)factory; return 0; }
static void p5_send(uint32_t tr) { (void)tr; }
static int general_sysex_send(uint32_t k) { (void)k; return 0; }
static int general_sysex_save(uint32_t k, const uint8_t *raw) { (void)k; (void)raw; return 0; }
static void preset_old_allow(uint32_t allow) { (void)allow; }
