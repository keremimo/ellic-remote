/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The Melodee Remote protocol (docs/PROTOCOL.md), the half that needs no hardware: Melodee's SysEx read into a
 * screen (what the FM-1 shows and lights), and the FM-1's own messages written. The app draws the screen and plays
 * the panel; tests/proto_test.c checks this half on its own. */
#pragma once
#include <stdint.h>

#define MEL_PROTO 1u                     /* the protocol's version, in HELLO both ways */
#define MEL_ID0 0x7Du                    /* F0 7D 4D 52: the non-commercial id, then "MR" */
#define MEL_ID1 0x4Du
#define MEL_ID2 0x52u

/* Melodee to the FM-1 (and HELLO, the FM-1's answer) */
enum {
    MEL_HELLO = 0x01,                    /* who is there: the FM-1 answers with its own HELLO */
    MEL_BYE = 0x02,                      /* Melodee lets the FM-1 go: its own screen again */
    MEL_TITLE = 0x10,                    /* the header's name and colour */
    MEL_INFO = 0x11,                     /* the header's right side and the transport */
    MEL_BODY = 0x12,                     /* which body shows: text, list or cells */
    MEL_ROW = 0x13,                      /* a list row */
    MEL_LINE = 0x14,                     /* a text line */
    MEL_CELL = 0x15,                     /* a body cell, or K1..K4 at the foot */
    MEL_POPUP = 0x16,                    /* two lines over the body for a while */
    MEL_CLEAR = 0x17,                    /* everything blank */
    MEL_LIGHTS = 0x20,                   /* the buttons' and keys' lights */
    MEL_KEYS = 0x21,                     /* how the keys play: channel, as notes or as buttons, octave */
};

enum { MEL_BODY_TEXT, MEL_BODY_LIST, MEL_BODY_CELLS, MEL_NBODY };

#define MEL_ROWS 6                       /* a list's rows on the screen (Melodee scrolls) */
#define MEL_LINES 3                      /* the text body: big, then medium, then small */
#define MEL_CELLS 8                      /* the body's cells, two rows of four */
#define MEL_KNOBS 4                      /* the foot's cells, K1..K4: cells 8..11 */

#define MEL_TITLE_N 23                   /* longest texts kept (longer is cut) */
#define MEL_INFO_N 15
#define MEL_ROW_N 23
#define MEL_VALUE_N 9
#define MEL_LINE_N 23
#define MEL_NAME_N 9
#define MEL_POPUP_N 23

/* flags: a row's (MEL_ROW) and a cell's (MEL_CELL) */
#define MEL_F_SELECTED 1u                /* where the player is: highlighted */
#define MEL_F_DIM 2u                     /* shown, but not in play */
#define MEL_F_MARK 4u                    /* a row: a square of its colour; a cell: its bar from the middle */
#define MEL_F_ON 8u                      /* a row: its value lit; a cell: no bar, the value alone */
#define MEL_F_BIPOLAR MEL_F_MARK
#define MEL_F_NOBAR MEL_F_ON

/* MEL_INFO's transport bits */
#define MEL_T_PLAYING 1u
#define MEL_T_RECORDING 2u
#define MEL_T_COUNTIN 4u

/* MEL_KEYS' flags */
#define MEL_K_BUTTONS 1u                 /* the keys are buttons: remote-channel notes 32 + key, no notes played */
#define MEL_K_OCT_MELODEE 2u             /* OCT- / OCT+ are Melodee's: the FM-1 no longer moves the octave itself */
#define MEL_K_NO_PRESSURE 4u             /* no poly pressure from the keys */
#define MEL_OCT_SAME 0x7Fu               /* MEL_KEYS' octave byte: leave it as it is */

/* MEL_LIGHTS' flags */
#define MEL_L_DIM 3u                     /* how bright the dim ones are: 0 off, 1 low, 2 high */
#define MEL_L_HELD_KEYS 4u               /* held keys light as well */
#define MEL_L_HELD_BUTTONS 8u            /* held buttons light as well */

/* the FM-1 to Melodee, as plain MIDI on the remote channel */
#define MEL_CH 15u                       /* channel 16 */
#define MEL_NOTE_BUTTON 0u               /* button b (plat.h B_*): note b, on 127 / off */
#define MEL_NOTE_KEY 32u                 /* key k as a button (MEL_K_BUTTONS): note 32 + k, with its velocity */
#define MEL_CC_ENC 16u                   /* encoder e (plat.h EN_*): CC 16 + e, 64 + detents (65 = one clockwise) */
#define MEL_CC_MASTER 7u                 /* MASTER: CC 7, then CC 39 its low seven bits */
#define MEL_CC_MASTER_LSB 39u

#define MEL_DIRTY_HEAD 1u
#define MEL_DIRTY_BODY 2u
#define MEL_DIRTY_FOOT 4u
#define MEL_DIRTY_LIGHTS 8u
#define MEL_DIRTY_KEYS 16u
#define MEL_DIRTY_POPUP 32u
#define MEL_DIRTY_ALL 63u

typedef struct {
    char text[MEL_ROW_N + 1];
    char value[MEL_VALUE_N + 1];
    uint8_t flags;
    uint16_t colour;                     /* RGB565; 0: the theme's */
} mel_row_t;

typedef struct {
    char name[MEL_NAME_N + 1];
    char value[MEL_VALUE_N + 1];
    uint8_t pos;                         /* its bar, 0..127 */
    uint8_t flags;
    uint16_t colour;
} mel_cell_t;

typedef struct {
    char title[MEL_TITLE_N + 1];
    char info[MEL_INFO_N + 1];
    uint16_t colour;
    uint8_t transport;
    uint8_t body;
    mel_row_t row[MEL_ROWS];
    char line[MEL_LINES][MEL_LINE_N + 1];
    mel_cell_t cell[MEL_CELLS + MEL_KNOBS];
    char popup[2][MEL_POPUP_N + 1];
    uint8_t popup_tenths;                /* how long the last popup stays, in 100 ms (0: until another) */
    uint8_t popup_shown;                 /* a popup is up */
    uint32_t btn_lit, btn_dim;           /* bit B_*; bit NB is PLAY's green LED (B_PLAY its red) */
    uint32_t keys_lit, keys_dim;         /* bit k = key k */
    uint8_t lights;                      /* MEL_L_* */
    uint8_t lights_set;                  /* Melodee has lit the panel */
    uint8_t keys_ch;                     /* 0..15 */
    uint8_t keys_flags;                  /* MEL_K_* */
    uint8_t octave;                      /* MEL_KEYS' last octave byte (64 = 0; MEL_OCT_SAME: none) */
    uint8_t dirty;                       /* MEL_DIRTY_*: what changed since the app last looked */
} mel_screen_t;

void mel_reset(mel_screen_t *s);         /* blank, as a new Melodee finds it (the keys as the FM-1 had them) */
/* one message, the bytes between F0 and F7: the command it was (MEL_*), or 0 when it is not this protocol's or is
 * malformed (then nothing changes) */
int mel_apply(mel_screen_t *s, const uint8_t *p, uint32_t n);
int mel_ours(const uint8_t *p, uint32_t n);   /* the protocol's header: 1 */

/* SysEx that comes a byte at a time (Bluetooth): mel_sx_byte gives 1 when a whole frame is in buf[0..n), the bytes
 * between F0 and F7; a frame too long, or cut short by another status, is dropped */
#define MEL_SX_MAX 512u
typedef struct {
    uint8_t buf[MEL_SX_MAX];
    uint32_t n;
    uint8_t open, over;
} mel_sx_t;
int mel_sx_byte(mel_sx_t *a, uint8_t b);

/* the FM-1's HELLO, the whole frame (F0 .. F7) into f (16 bytes are enough): its length */
uint32_t mel_hello(uint8_t *f, uint32_t major, uint32_t minor, uint32_t patch, uint32_t keys_ch, int32_t octave,
                  uint32_t keys_flags);
/* a SysEx frame (F0 .. F7) as USB-MIDI packets (CIN 4, then 5..7 for its end), cable 0: how many */
uint32_t mel_packets(const uint8_t *f, uint32_t n, uint32_t *pkts, uint32_t max);

/* RGB565 from three seven-bit components */
static inline uint16_t mel_rgb7(uint32_t r, uint32_t g, uint32_t b)
{
    r = r << 1 | r >> 6;
    g = g << 1 | g >> 6;
    b = b << 1 | b >> 6;
    return (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | b >> 3);
}
