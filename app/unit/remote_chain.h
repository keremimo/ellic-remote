/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The selected track's chain, what a chain can take, Ellic Studio's samples and a take's meter, as Ellic Studio tells
 * them (docs/PROTOCOL.md version 5: CHAIN, CATALOGUE, SAMPLES, METER; remote_chain.c reads them). The CHAIN page
 * (FX's first) shows the chain a row each; the pads and LOAD pages draw a sampler's bank and the samples. */
#define CHAIN_MAX 16u                    /* devices a chain shows */
#define CHAIN_NAME 13u                   /* a device's name (12 letters and the 0) */
#define CHAIN_READ 9u                    /* .. its first value as it reads (8 and the 0) */
enum { CK_NOTE, CK_MIDI, CK_INST, CK_FX, CK_LOOPER, CK_PLUGIN, CK_N };   /* CHAIN's kinds */
enum { CG_NOTE, CG_INST, CG_FX, CG_PLUGIN, CG_N };                       /* CATALOGUE's groups */
#define CAT_MAX 64u                      /* names a group keeps (Ellic Studio tells up to 128) */
#define CAT_TEXT 3072u
#define SMP_FOLDERS 8u                   /* folders SAMPLES keeps, names a folder keeps */
#define SMP_MAX 96u
#define SMP_TEXT 8192u
#define METER_FRESH_MS 700u              /* a METER older than this is not drawn */

typedef struct { uint8_t kind, on; char name[CHAIN_NAME], read[CHAIN_READ]; } chain_dev_t;
static struct {
    uint8_t track;                       /* whose chain it is (0xFF: none told yet) */
    uint8_t chosen;                      /* Ellic Studio's chosen device (127 none) */
    uint8_t n;
    chain_dev_t dev[CHAIN_MAX];
    uint8_t row;                         /* the row picked on CHAIN: a device, or n (+ Add) */
    uint32_t gen;                        /* bumped with each change the pages draw */
} chn;
static struct {
    uint8_t n[CG_N];
    uint16_t name[CG_N][CAT_MAX];        /* each name's place in text */
    char text[CAT_TEXT];
    uint16_t used;
} cat __attribute__((section(".pool")));
static struct {
    uint8_t nf;                          /* folders told */
    uint8_t n[SMP_FOLDERS];
    uint16_t folder[SMP_FOLDERS];        /* each folder's name's place in text */
    uint16_t name[SMP_FOLDERS][SMP_MAX];
    char text[SMP_TEXT];
    uint16_t used;
    uint32_t gen;
} smp __attribute__((section(".pool")));
static struct { uint8_t track, level; uint32_t ms; } meter;   /* a take's source level, when last told */

static int chain_on(void) { return chn.track == song.sel; }
static const param_desc_t *chain_desc(uint32_t slot, int16_t **valp);   /* (remote_chain.c: the CHAIN page's knobs) */
