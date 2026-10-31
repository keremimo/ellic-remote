/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The Melodee Remote protocol (docs/PROTOCOL.md), the half that needs no hardware: Melodee's SysEx read into a
 * screen (what the FM-1 shows and lights: version 1's generic screen, version 2's mirror of Melodee FM-1's pages), and
 * the FM-1's own messages written. The app draws the screen and plays the panel; tests/proto_test.c checks this half
 * on its own. */
#pragma once
#include <stdint.h>

#define MEL_PROTO 2u                     /* the protocol's version, in HELLO both ways (1: the generic screen alone) */
#define MEL_ID0 0x7Du                    /* F0 7D 4D 52: the non-commercial id, then "MR" */
#define MEL_ID1 0x4Du
#define MEL_ID2 0x52u

/* Melodee to the FM-1 (and HELLO, the FM-1's answer) */
enum {
    MEL_HELLO = 0x01,                    /* who is there: the FM-1 answers with its own HELLO */
    MEL_BYE = 0x02,                      /* Melodee lets the FM-1 go: its own screen again */
    MEL_STATE = 0x03,                    /* 2: the FM-1's: its screen's generation, whose it is (PING's answer) */
    MEL_PING = 0x04,                     /* 2: is the FM-1 still Melodee's, its screen still painted? (STATE) */
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
    /* version 2: Melodee FM-1's pages, their parts mirrored (docs/PROTOCOL.md "Version 2") */
    MEL_PAGE = 0x30,                     /* which page shows */
    MEL_HEAD = 0x31,                     /* the header: transport, BPM, REC, octave, song row, dots, chip, status */
    MEL_TRACK = 0x32,                    /* a place's track: numeral, colour, flags, pattern chips, sound, role */
    MEL_STEPS = 0x33,                    /* a place's pattern: its steps (rest, hit, tie) */
    MEL_PLAY = 0x34,                     /* the playheads, a step a place */
    MEL_SEL = 0x35,                      /* the selected place, the places shown */
    MEL_NOTES = 0x36,                    /* the notes sounding, each with its place */
    MEL_METER = 0x37,                    /* the places' output levels */
    MEL_SCOPE = 0x38,                    /* the output's waveform */
    MEL_CARD = 0x39,                     /* a knob's card: label, value, unit */
    MEL_LIST = 0x3A,                     /* a list page: its length, the window, the row picked, its picture */
    MEL_LROW = 0x3B,                     /* a list page's row */
    MEL_SCALE = 0x3C,                    /* the selected track's key: QNT, root, scale, transpose, its name */
    MEL_PICTURE = 0x3D,                  /* the picture under the cards */
    MEL_FOOT = 0x3E,                     /* the sound bar */
    MEL_SHEET = 0x3F,                    /* an action sheet: shown, its rows, the row picked, its title */
    MEL_SROW = 0x40,                     /* a sheet's row */
    MEL_ASK = 0x41,                      /* a question: its words, No and Yes */
    MEL_RACK = 0x42,                     /* an FX rack's row */
    MEL_MIX = 0x43,                      /* a MIXER strip */
    MEL_GRID = 0x44,                     /* PATTERNS: a place's row of patterns */
    MEL_GRIDTOP = 0x45,                  /* PATTERNS: the page of patterns, the song's sections */
    MEL_SCALES = 0x46,                   /* SCALES: the family, the scale's place in it */
    MEL_SCROW = 0x47,                    /* SCALES: a scale around the one picked */
    MEL_LANES = 0x48,                    /* a drum track's lanes' names (Stage's band) */
    MEL_PICK = 0x49,                     /* a knob's picker: the names around its value */
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

/* ---------------------------------------------------------------- version 2 --- */
enum { MEL_PG_GENERIC, MEL_PG_STAGE, MEL_PG_WHEEL, MEL_PG_KNOBS, MEL_PG_LIST, MEL_PG_SCALES, MEL_PG_RACK,
       MEL_PG_MIXER, MEL_PG_GLO, MEL_PG_PATTERNS, MEL_NPAGE };
#define MEL_PF_TAP_DIM 1u                /* MEL_PAGE's flags: GLO's Tap key dim (an outside clock) */

#define MEL_PLACES 4                     /* the tracks a page shows (Melodee FM-1's group of four) */
#define MEL_NSTEP 64                     /* steps a pattern shows */
#define MEL_PH_NONE 127u                 /* MEL_PLAY: no playhead */

/* MEL_HEAD */
enum { MEL_TR_STOPPED, MEL_TR_PLAYING, MEL_TR_SONG };   /* its transport icon */
enum { MEL_REC_NONE, MEL_REC_ON, MEL_REC_RING };       /* its REC mark: filled (the selected track), a ring (another) */
#define MEL_HF_CHIP_DIM 1u               /* the chip in MID (PATTERNS), not the track's colour */
#define MEL_HF_BPM_HOT 2u                /* the BPM being turned: in the accent */
#define MEL_HF_STATUS_RED 4u             /* the status in REC's red (ERASING, WAIT LOOP), else a message */
#define MEL_BPM_N 5
#define MEL_CHIP_N 19
#define MEL_STATUS_N 39

/* MEL_TRACK's flags */
#define MEL_TF_MUTED 1u
#define MEL_TF_ARMED 2u
#define MEL_TF_WAITING 4u                /* armed and waiting (stopped, or for the loop): its border breathes */
#define MEL_TF_DRUM 8u                   /* its notes are lanes (MEL_LANES), not pitches */
#define MEL_TF_IDLE 16u                  /* no pattern at all: one track's card shows no steps */
#define MEL_TF_SOLO 32u                  /* GLO: soloed */
#define MEL_TF_PENDING 64u               /* its sound is being browsed, not loaded yet: the name in the accent */
#define MEL_SOUND_N 23
#define MEL_ROLE_N 11
#define MEL_PAT_N 3
#define MEL_EXTRA_N 39

/* MEL_CARD's flags: bits 0-1 the value's colour */
enum { MEL_CV_TEXT, MEL_CV_DIM, MEL_CV_ACCENT, MEL_CV_KNOB };
#define MEL_CF_TRACK 4u                  /* the label in its place's track colour (GLO's levels) */
#define MEL_CF_HOT 8u                    /* the knob turning (the FM-1 marks it itself when K1..K4 turn) */
#define MEL_LABEL_N 11
#define MEL_VAL_N 15
#define MEL_UNIT_N 7

/* MEL_LIST, MEL_LROW */
#define MEL_LIST_ROWS 5                  /* rows on the screen: 5, or 4 over a picture */
enum { MEL_LP_NONE, MEL_LP_SCALE };
#define MEL_RF_MORE 1u                   /* a list behind it: ">" */
#define MEL_RF_DIM 2u
#define MEL_ROWL_N 23

/* MEL_SCALE */
enum { MEL_Q_OFF, MEL_Q_SNAP, MEL_Q_WHITE, MEL_Q_ALL, MEL_NQ };
#define MEL_SCALE_N 31

/* MEL_PICTURE's kinds */
enum { MEL_PIC_NONE, MEL_PIC_SCOPE, MEL_PIC_FILTER, MEL_PIC_ENV, MEL_PIC_ARP, MEL_PIC_FX, MEL_PIC_CHORD, MEL_PIC_LINES,
       MEL_PIC_STEPS, MEL_PIC_KEYS, MEL_NPIC };
#define MEL_PIC_NOTES 16                 /* ARP's held notes, CHORD's notes kept */
#define MEL_PIC_TEXT_N 23
/* MEL_PIC_FX's effects (Melodee FM-1's FX_* numbers, its core.h; 6 is retired): the pictures the FM-1 draws */
enum { MEL_FX_OFF, MEL_FX_PHASER, MEL_FX_WIDTH, MEL_FX_FORMANT, MEL_FX_TAPE, MEL_FX_SPIT, MEL_FX_RETIRED, MEL_FX_ROOM,
       MEL_FX_REVERB, MEL_FX_CHORUS, MEL_FX_NOTE_TAIL, MEL_FX_NOTE_ECHO, MEL_FX_SOFT, MEL_FX_HARD, MEL_FX_FOLD,
       MEL_FX_FUZZ, MEL_FX_CRUSH, MEL_FX_FLANGER, MEL_FX_PEDAL_PHASER, MEL_FX_PEDAL_CHORUS, MEL_NFX };
#define MEL_FXF_MEMORY 1u                /* MEL_PIC_FX's flags: its buffer could not be had ("MEMORY FULL") */

/* MEL_FOOT's kinds */
enum { MEL_FT_TRACK, MEL_FT_ALL, MEL_FT_NONE };
#define MEL_FOOT_N 23
#define MEL_RIGHT_N 11

/* MEL_SHEET, MEL_SROW, MEL_ASK */
#define MEL_SHEET_ROWS 12
#define MEL_SF_RED 1u                    /* a row that destroys something (it asks first) */
#define MEL_SF_DOT 2u                    /* the value now */
#define MEL_SUB_N 19
#define MEL_ASK_N 47
#define MEL_BUTTON_N 11
enum { MEL_AI_WARN, MEL_AI_MOTION, MEL_AI_NONE };       /* MEL_ASK's icon */

/* MEL_RACK's flags */
#define MEL_KF_SELECTED 1u
#define MEL_KF_OFF 2u                    /* an empty slot: "--", dim */
#define MEL_KF_SHARED 4u                 /* the track's shared row: an arrow, the shared effects (comma-separated) */
#define MEL_KF_RETURN 8u                 /* SHARED's RETURN: the four tracks' colours */
#define MEL_KF_SPLIT 16u                 /* the shared effects as commas, not a chain's arrows */
#define MEL_KF_NO_AMOUNT 32u             /* no bar, no amount */
#define MEL_TAG_N 3
#define MEL_RACKN_N 31

/* MEL_MIX's flags: 1 muted, 2 armed; bits 4-6 the control turning (1 LEVEL, 2 PAN, 4 MUTE) */
#define MEL_MF_MUTED 1u
#define MEL_MF_ARMED 2u
#define MEL_MIXV_N 7

/* MEL_GRID: a cell's state (bits 0-1) and marks */
enum { MEL_GC_EMPTY, MEL_GC_USED, MEL_GC_PLAYING };
#define MEL_GC_QUEUED 4u                 /* waiting for the bar: dashed */
#define MEL_GC_TARGET 8u                 /* Copy to: where it goes, framed */
#define MEL_GRID_COLS 8
#define MEL_GF_LEFT 1u                   /* the row's pattern on the other page: a tick at the left */
#define MEL_GF_RIGHT 2u                  /* .. at the right */
#define MEL_SONG_N 6
#define MEL_GTF_JAM 1u                   /* the jam's rows, not a song's */

/* MEL_SCALES, MEL_SCROW */
#define MEL_SCF_STAR 1u
#define MEL_SCF_WORDS 2u                 /* row 2 (the card's place) is words alone ("No favourites") */
#define MEL_FAMILY_N 15
#define MEL_DETAIL_N 23

/* MEL_LANES, MEL_PICK */
#define MEL_LANES_N 8
#define MEL_LANE_N 4
#define MEL_PICK_ROWS 5
#define MEL_PICK_N 15
#define MEL_PKF_FX 1u                    /* the effects' picker: wider, each name its family's square */

/* the remote's STATE (and its HELLO's tail): whose the screen is, why its generation last changed */
enum { MEL_ST_FREE, MEL_ST_YOURS, MEL_ST_TAKEN };
enum { MEL_WHY_NONE, MEL_WHY_POWER, MEL_WHY_LINK, MEL_WHY_STRAY, MEL_WHY_BYE, MEL_WHY_TAKEN, MEL_WHY_SILENT };

#define MEL_DIRTY_HEAD 1u
#define MEL_DIRTY_BODY 2u
#define MEL_DIRTY_FOOT 4u
#define MEL_DIRTY_LIGHTS 8u
#define MEL_DIRTY_KEYS 16u
#define MEL_DIRTY_POPUP 32u
#define MEL_DIRTY_ALL 63u
/* version 2's parts, each its own mark (mel_screen_t.v2) */
#define MEL_V2_PAGE (1u << 0)
#define MEL_V2_HEAD (1u << 1)
#define MEL_V2_TRACKS (1u << 2)          /* a track (name, colour, flags, chips) */
#define MEL_V2_STEPS (1u << 3)
#define MEL_V2_PLAY (1u << 4)
#define MEL_V2_NOTES (1u << 5)
#define MEL_V2_METER (1u << 6)
#define MEL_V2_SCOPE (1u << 7)
#define MEL_V2_CARDS (1u << 8)
#define MEL_V2_LIST (1u << 9)
#define MEL_V2_SCALE (1u << 10)
#define MEL_V2_PICTURE (1u << 11)
#define MEL_V2_FOOT (1u << 12)
#define MEL_V2_SHEET (1u << 13)
#define MEL_V2_ASK (1u << 14)
#define MEL_V2_RACK (1u << 15)
#define MEL_V2_MIX (1u << 16)
#define MEL_V2_GRID (1u << 17)
#define MEL_V2_SCALES (1u << 18)
#define MEL_V2_LANES (1u << 19)
#define MEL_V2_PICK (1u << 20)
#define MEL_V2_SEL (1u << 21)
#define MEL_V2_ALL ((1u << 22) - 1u)

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

/* version 2's mirror: what Melodee FM-1's pages read from its model, as Melodee sent it */
typedef struct {
    uint8_t numeral;                     /* 1..99 (0: the place is free: "Add track") */
    uint8_t flags;                       /* MEL_TF_* */
    uint16_t colour;                     /* RGB565; 0: the palette's */
    char pattern[MEL_PAT_N + 1], next[MEL_PAT_N + 1];
    char name[MEL_SOUND_N + 1], role[MEL_ROLE_N + 1], extra[MEL_EXTRA_N + 1];
    uint8_t len;                         /* its pattern's steps, 1..64 */
    uint8_t steps[MEL_NSTEP / 4];        /* 2 bits a step: 0 rest, 1 a hit, 2 a tie */
    uint8_t ph;                          /* its playhead (MEL_PH_NONE) */
    uint8_t meter;                       /* its output, 0..127 */
    uint8_t mix_flags, mix_level;        /* MIXER (MEL_MIX) */
    char mix_db[MEL_MIXV_N + 1], mix_pan[MEL_MIXV_N + 1];
    uint8_t grid_flags;                  /* PATTERNS (MEL_GRID) */
    uint8_t cell[MEL_GRID_COLS][4];      /* state, then three bars 0..10 */
} mel_track_t;

typedef struct {
    char label[MEL_LABEL_N + 1], value[MEL_VAL_N + 1], unit[MEL_UNIT_N + 1];
    uint8_t flags;                       /* MEL_CV_* | MEL_CF_* */
} mel_card_t;

typedef struct {
    char label[MEL_ROWL_N + 1], value[MEL_VAL_N + 1];
    uint8_t flags;
} mel_lrow_t;

typedef struct {
    uint8_t flags, amount;
    char tag[MEL_TAG_N + 1], name[MEL_RACKN_N + 1], value[MEL_MIXV_N + 5];
} mel_rack_t;

typedef struct {
    uint8_t shade, flags;
    char name[MEL_SCALE_N + 1], detail[MEL_DETAIL_N + 1];
} mel_scrow_t;

typedef struct {
    uint8_t page, page_flags;
    /* the header */
    uint8_t transport, rec, octave, row, dots, dot, head_flags;
    char bpm[MEL_BPM_N + 1], chip[MEL_CHIP_N + 1], status[MEL_STATUS_N + 1];
    /* the tracks */
    mel_track_t trk[MEL_PLACES];
    uint8_t sel, shown;                  /* the selected place, the places shown (1..4) */
    uint8_t nnotes, note[16], note_of[16];   /* the notes sounding, each one's place */
    uint8_t nscope, scope[120];
    char lane[MEL_LANES_N][MEL_LANE_N + 1];
    uint8_t nlanes;
    /* the cards, the list, the picture, the foot */
    mel_card_t card[4];
    uint8_t list_n, list_first, list_sel, list_pic;
    mel_lrow_t lrow[MEL_LIST_ROWS];
    uint8_t q, root, trans;              /* MEL_SCALE: QNT, the root, 64 + transpose */
    uint16_t mask;                       /* the scale's notes, bit i: i semitones over the root */
    char scale[MEL_SCALE_N + 1];
    uint8_t pic, pic_v[6], pic_n, pic_note[MEL_PIC_NOTES];
    char pic_text[3][MEL_PIC_TEXT_N + 1];
    uint8_t foot;
    char foot_text[MEL_FOOT_N + 1], foot_right[MEL_RIGHT_N + 1];
    /* the popups */
    uint8_t sheet_on, sheet_n, sheet_sel;
    char sheet_title[MEL_ROWL_N + 1], sheet_sub[MEL_SUB_N + 1];
    mel_lrow_t srow[MEL_SHEET_ROWS];
    uint8_t ask_on, ask_icon;
    char ask_q[MEL_ASK_N + 1], ask_d[MEL_ASK_N + 1], ask_no[MEL_BUTTON_N + 1], ask_yes[MEL_BUTTON_N + 1];
    uint8_t pick_on, pick_knob, pick_n, pick_now, pick_flags, pick_fam[MEL_PICK_ROWS];
    char pick[MEL_PICK_ROWS][MEL_PICK_N + 1];
    /* the pages of their own */
    mel_rack_t rack[4];
    uint8_t grid_page, grid_pages, song_n, song_cur, grid_flags;
    char song[MEL_SONG_N + 1];
    uint8_t sc_rank, sc_total;
    char sc_family[MEL_FAMILY_N + 1];
    mel_scrow_t scrow[5];
    uint32_t dirty;                      /* MEL_V2_*: what changed since the app last looked */
} mel_v2_t;

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
    mel_v2_t v2;                         /* version 2's pages */
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

/* the FM-1's HELLO, the whole frame (F0 .. F7) into f (16 bytes are enough): its length. Version 2's tail: the
 * screen's generation (14 bits) and whose it is (MEL_ST_*) */
uint32_t mel_hello(uint8_t *f, uint32_t major, uint32_t minor, uint32_t patch, uint32_t keys_ch, int32_t octave,
                  uint32_t keys_flags, uint32_t gen, uint32_t state);
/* the FM-1's STATE (PING's answer, or unasked when the generation changes): its length (10) */
uint32_t mel_state(uint8_t *f, uint32_t gen, uint32_t state, uint32_t why);
/* step i of place k's pattern: 0 rest, 1 a hit, 2 a tie */
static inline uint32_t mel_step(const mel_track_t *t, uint32_t i) { return (t->steps[i >> 2] >> (2u * (i & 3u))) & 3u; }
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
