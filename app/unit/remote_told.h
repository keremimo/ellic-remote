/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The selected track's sound pages as Melodee tells them: the set (remote_told.c), before the page table that shows it
 * (params.c SC_TOLD, page_desc). */
#define TOLD_PAGES 32u                   /* (the page table's told slots) */
#define TOLD_K 120u                      /* told values a set holds */
#define TOLD_TEXT 8192u                  /* the set's titles, labels, units and value names */
#define TOLD_NAMES 1024u                  /* .. the value names' pointers, each list ended by 0 */
#define TOLD_WAVE_W 224u                 /* a wave's columns (WAVE) */
#define TOLD_MARKS 64u
#define TOLD_ID 4096u                    /* PARAM's ids from here: told value k */
enum { TF_HOME, TF_EDIT, TF_ENV, TF_LFO, TF_FX, TF_N };   /* PAGES' families */
enum { TV_CARDS, TV_WAVE, TV_ENV };                       /* .. its views */
enum { TM_START = 1, TM_END, TM_LOOP_START, TM_LOOP_END, TM_CUT, TM_WARP, TM_GAIN, TM_FADE_IN, TM_FADE_OUT, TM_PLAYING };
#define TM_SELECTED 0x40u                /* (a mark's kind: the one selected) */

static struct {
    uint8_t track;                       /* whose set it is (0xFF: none told yet) */
    uint8_t keys, pads;                  /* the keys: 0 the layout, 1 pads from the note pads */
    uint8_t n;                           /* pages told */
    struct { uint8_t fam, view, k[4]; const char *title; } page[TOLD_PAGES];
    param_desc_t desc[TOLD_K];
    uint8_t action[TOLD_K / 8u];         /* bit k: value k is an action (DESC format 14) */
    int16_t val[TOLD_K], lk[TOLD_K];     /* the values here; as Melodee last told them (what differs is the knobs') */
    char text[TOLD_TEXT];
    uint16_t used;
    const char *names[TOLD_NAMES];
    uint16_t nnames;
    uint16_t wave_gen, marks_gen;
    uint8_t wave[TOLD_WAVE_W][2];        /* each column's low and high, 0..127 (64 the middle) */
    uint8_t nmarks;
    struct { uint8_t kind; int16_t pos, value; } mark[TOLD_MARKS];
    uint32_t gen;                        /* bumped with each change the pages draw */
} told;

static int told_on(void) { return told.track == song.sel; }
static int32_t told_v14(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 7) - 8192; }
static int told_is_action(uint32_t k) { return k < TOLD_K && (told.action[k >> 3] >> (k & 7u) & 1u); }

