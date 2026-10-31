/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The protocol's half without hardware (app/mel_proto.c): Melodee's messages read into the screen (version 1's and
 * version 2's), malformed ones changing nothing, SysEx gathered a byte at a time, and the FM-1's HELLO and STATE.
 *   cc -I app -o build/proto_test tests/proto_test.c && build/proto_test */
#include <stdio.h>
#include <string.h>
#include "mel_proto.c"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* a message from its bytes after the header (cmd first); n of them */
static int put(mel_screen_t *s, const uint8_t *body, uint32_t n)
{
    uint8_t m[600] = {MEL_ID0, MEL_ID1, MEL_ID2};
    memcpy(m + 3, body, n);
    return mel_apply(s, m, n + 3);
}

/* a text into a message: its length, then its characters */
static uint32_t text(uint8_t *d, const char *t)
{
    uint32_t n = (uint32_t)strlen(t);
    d[0] = (uint8_t)n;
    memcpy(d + 1, t, n);
    return n + 1;
}

static void test_header(void)
{
    mel_screen_t s;
    uint8_t m[64] = {MEL_TITLE, 127, 0, 64};
    uint32_t n = 4;
    mel_reset(&s);
    CHECK(s.body == MEL_BODY_TEXT && s.dirty == MEL_DIRTY_ALL && s.octave == MEL_OCT_SAME);
    s.dirty = 0;
    n += text(m + n, "DRUMS");
    CHECK(put(&s, m, n) == MEL_TITLE);
    CHECK(!strcmp(s.title, "DRUMS"));
    CHECK(s.colour == mel_rgb7(127, 0, 64) && s.colour == ((31u << 11) | (0u << 5) | 16u));
    CHECK(s.dirty & MEL_DIRTY_HEAD);
    s.dirty = 0;
    CHECK(put(&s, m, n) == MEL_TITLE);   /* the same again: nothing to draw */
    CHECK(s.dirty == 0);
    m[0] = MEL_INFO;
    m[1] = MEL_T_PLAYING | MEL_T_RECORDING;
    n = 2 + text(m + 2, "120.0  3.2.1");
    CHECK(put(&s, m, n) == MEL_INFO);
    CHECK(!strcmp(s.info, "120.0  3.2.1") && s.transport == 3u && (s.dirty & MEL_DIRTY_HEAD));
    /* a title longer than kept: cut; a character outside printable ASCII: '?' */
    m[0] = MEL_TITLE;
    m[1] = m[2] = m[3] = 0;
    n = 4 + text(m + 4, "A VERY LONG TRACK NAME THAT GOES ON");
    m[6] = 0x0B;                         /* (m[4] its length, m[5] 'A'; 1..10: version 2's extra letters) */
    CHECK(put(&s, m, n) == MEL_TITLE);
    CHECK(strlen(s.title) == MEL_TITLE_N && s.title[0] == 'A' && s.title[1] == '?');
    CHECK(s.colour == 0);
}

static void test_list(void)
{
    mel_screen_t s;
    uint8_t m[64] = {MEL_BODY, MEL_BODY_LIST};
    uint32_t n;
    mel_reset(&s);
    CHECK(put(&s, m, 2) == MEL_BODY && s.body == MEL_BODY_LIST);
    m[0] = MEL_ROW;
    m[1] = 2;
    m[2] = MEL_F_SELECTED | MEL_F_MARK;
    m[3] = 0, m[4] = 127, m[5] = 0;
    n = 6 + text(m + 6, "Bass");
    n += text(m + n, "-6.0 dB");
    s.dirty = 0;
    CHECK(put(&s, m, n) == MEL_ROW);
    CHECK(!strcmp(s.row[2].text, "Bass") && !strcmp(s.row[2].value, "-6.0 dB"));
    CHECK(s.row[2].flags == (MEL_F_SELECTED | MEL_F_MARK) && s.row[2].colour == mel_rgb7(0, 127, 0));
    CHECK(s.dirty & MEL_DIRTY_BODY);
    m[1] = MEL_ROWS;                      /* no such row: nothing */
    CHECK(put(&s, m, n) == 0);
    m[1] = 2;
    CHECK(put(&s, m, n - 3) == 0);      /* cut short: nothing changes */
    CHECK(!strcmp(s.row[2].value, "-6.0 dB"));
    m[6] = 0x84;                         /* a byte with its top bit: malformed */
    CHECK(put(&s, m, n) == 0);
    CHECK(!strcmp(s.row[2].text, "Bass"));
}

static void test_cells_lines_popup(void)
{
    mel_screen_t s;
    uint8_t m[64] = {MEL_CELL, MEL_CELLS + 1, MEL_F_BIPOLAR, 100, 0, 0, 0};
    uint32_t n = 7;
    mel_reset(&s);
    s.dirty = 0;
    n += text(m + n, "CUTOFF");
    n += text(m + n, "2.4k");
    CHECK(put(&s, m, n) == MEL_CELL);
    CHECK(!strcmp(s.cell[MEL_CELLS + 1].name, "CUTOFF") && s.cell[MEL_CELLS + 1].pos == 100);
    CHECK(s.dirty == MEL_DIRTY_FOOT);     /* a knob at the foot: the foot alone */
    m[1] = 3;
    s.dirty = 0;
    CHECK(put(&s, m, n) == MEL_CELL);
    CHECK(s.dirty == 0);                 /* a body cell while the body is text: drawn when it shows */
    m[1] = MEL_CELLS + MEL_KNOBS;
    CHECK(put(&s, m, n) == 0);
    m[0] = MEL_LINE;
    m[1] = 0;
    n = 2 + text(m + 2, "Waiting");
    CHECK(put(&s, m, n) == MEL_LINE && !strcmp(s.line[0], "Waiting") && (s.dirty & MEL_DIRTY_BODY));
    m[1] = MEL_LINES;
    CHECK(put(&s, m, n) == 0);
    m[0] = MEL_POPUP;
    m[1] = 15;
    n = 2 + text(m + 2, "SAVED");
    n += text(m + n, "Song 3");
    CHECK(put(&s, m, n) == MEL_POPUP);
    CHECK(!strcmp(s.popup[0], "SAVED") && !strcmp(s.popup[1], "Song 3") && s.popup_tenths == 15);
    CHECK(s.dirty & MEL_DIRTY_POPUP);
}

static void test_lights_keys(void)
{
    mel_screen_t s;
    /* PLAY red (bit 10) and green (bit 14); keys 0 and 26; dim: FX; held keys lit, glow 2 */
    uint8_t m[] = {MEL_LIGHTS, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
                   MEL_L_HELD_KEYS | 2};
    mel_reset(&s);
    CHECK(put(&s, m, sizeof m) == MEL_LIGHTS);
    CHECK(s.btn_lit == ((1u << 10) | (1u << 14)) && s.btn_dim == 1u);
    CHECK(s.keys_lit == ((1u << 0) | (1u << 26)) && s.keys_dim == 0);
    CHECK(s.lights == (MEL_L_HELD_KEYS | 2) && s.lights_set && (s.dirty & MEL_DIRTY_LIGHTS));
    CHECK(put(&s, m, sizeof m - 1) == 0);
    {
        uint8_t k[] = {MEL_KEYS, 9, MEL_K_BUTTONS, 64 + 1};
        CHECK(put(&s, k, sizeof k) == MEL_KEYS);
        CHECK(s.keys_ch == 9 && s.keys_flags == MEL_K_BUTTONS && s.octave == 65 && (s.dirty & MEL_DIRTY_KEYS));
        k[1] = 16;
        CHECK(put(&s, k, sizeof k) == 0 && s.keys_ch == 9);
    }
    /* CLEAR keeps how the keys play */
    {
        uint8_t c[] = {MEL_CLEAR};
        CHECK(put(&s, c, 1) == MEL_CLEAR);
        CHECK(s.keys_ch == 9 && s.keys_flags == MEL_K_BUTTONS && !s.lights_set && s.btn_lit == 0);
    }
}

static void test_other(void)
{
    mel_screen_t s;
    uint8_t other[] = {0x7D, 0x46, 0x4C, 0x01};   /* another editor's frame on the same id */
    uint8_t hello[] = {MEL_ID0, MEL_ID1, MEL_ID2, MEL_HELLO, MEL_PROTO};
    mel_reset(&s);
    CHECK(mel_apply(&s, other, sizeof other) == 0);
    CHECK(mel_apply(&s, hello, 3) == 0);
    CHECK(mel_apply(&s, hello, sizeof hello) == MEL_HELLO);
    hello[3] = 0x55;
    CHECK(mel_apply(&s, hello, sizeof hello) == 0);
}

static void test_sx(void)
{
    static mel_sx_t a;
    uint32_t i, got = 0;
    const uint8_t in[] = {0xF0, 1, 2, 3, 0xF7, 0xF0, 4, 0x90, 5, 0xF7, 0xF0, 6, 0xF7};
    for (i = 0; i < sizeof in; i++)
        if (mel_sx_byte(&a, in[i])) {
            got++;
            if (got == 1)
                CHECK(a.n == 3 && a.buf[0] == 1 && a.buf[2] == 3);
            else
                CHECK(a.n == 1 && a.buf[0] == 6);   /* the one cut short by 0x90 never came */
        }
    CHECK(got == 2);
    mel_sx_byte(&a, 0xF0);                /* too long: dropped whole */
    for (i = 0; i < MEL_SX_MAX + 4u; i++)
        mel_sx_byte(&a, 0x11);
    CHECK(mel_sx_byte(&a, 0xF7) == 0);
    CHECK(mel_sx_byte(&a, 0xF7) == 0);    /* a stray end */
}

static void test_hello_packets(void)
{
    uint8_t f[20];
    uint32_t pk[8], n = mel_hello(f, 0, 1, 2, 3, -1, MEL_K_BUTTONS, 0x1234, MEL_ST_YOURS), k;
    CHECK(n == 16 && f[0] == 0xF0 && f[15] == 0xF7);
    CHECK(f[4] == MEL_HELLO && f[5] == MEL_PROTO && MEL_PROTO == 2u && f[7] == 1 && f[9] == 3 && f[10] == 63 &&
          f[11] == MEL_K_BUTTONS);
    CHECK(f[12] == 0x34 && f[13] == 0x24 && f[14] == MEL_ST_YOURS);   /* the generation, 14 bits, lowest first */
    k = mel_packets(f, n, pk, 8);
    CHECK(k == 6);
    CHECK(pk[0] == (4u | 0xF0u << 8 | (uint32_t)MEL_ID0 << 16 | (uint32_t)MEL_ID1 << 24));
    CHECK((pk[5] & 0xFu) == 5u && ((pk[5] >> 8) & 0xFFu) == 0xF7u);   /* 16 = 5 x 3 + 1: the end alone */
    n = mel_state(f, 0x3FFF, MEL_ST_TAKEN, MEL_WHY_TAKEN);
    CHECK(n == 10 && f[4] == MEL_STATE && f[5] == 0x7F && f[6] == 0x7F && f[7] == MEL_ST_TAKEN && f[8] == MEL_WHY_TAKEN &&
          f[9] == 0xF7);
    n = 13;
    f[12] = 0xF7;
    n = 12;
    f[11] = 0xF7;
    k = mel_packets(f, n, pk, 8);
    CHECK(k == 4 && (pk[3] & 0xFu) == 7u && (pk[3] >> 24) == 0xF7u);
    n = 11;
    f[10] = 0xF7;
    k = mel_packets(f, n, pk, 8);
    CHECK(k == 4 && (pk[3] & 0xFu) == 6u && ((pk[3] >> 16) & 0xFFu) == 0xF7u);
    CHECK(mel_packets(f, n, pk, 2) == 2);
}

/* ------------------------------------------------------------------------------------------ version 2 --- */
static void test_v2_page_head(void)
{
    mel_screen_t s;
    uint8_t m[96] = {MEL_PAGE, MEL_PG_STAGE, 0};
    uint32_t n;
    mel_reset(&s);
    CHECK(s.v2.page == MEL_PG_GENERIC && s.v2.octave == 64 && s.v2.shown == 1 && s.v2.trk[2].ph == MEL_PH_NONE);
    s.v2.dirty = 0;
    CHECK(put(&s, m, 3) == MEL_PAGE && s.v2.page == MEL_PG_STAGE && s.v2.dirty == MEL_V2_ALL);
    s.v2.dirty = 0;
    CHECK(put(&s, m, 3) == MEL_PAGE && s.v2.dirty == 0);   /* the same page: nothing */
    m[1] = MEL_NPAGE;
    CHECK(put(&s, m, 3) == 0 && s.v2.page == MEL_PG_STAGE);
    m[0] = MEL_HEAD;
    m[1] = MEL_TR_PLAYING, m[2] = MEL_REC_RING, m[3] = 66, m[4] = 3, m[5] = 5, m[6] = 2, m[7] = MEL_HF_CHIP_DIM;
    n = 8 + text(m + 8, "124");
    n += text(m + n, "PROPHET");
    n += text(m + n, "");
    CHECK(put(&s, m, n) == MEL_HEAD);
    CHECK(s.v2.transport == MEL_TR_PLAYING && s.v2.rec == MEL_REC_RING && s.v2.octave == 66 && s.v2.row == 3 &&
          s.v2.dots == 5 && s.v2.dot == 2 && s.v2.head_flags == MEL_HF_CHIP_DIM);
    CHECK(!strcmp(s.v2.bpm, "124") && !strcmp(s.v2.chip, "PROPHET") && !s.v2.status[0] && (s.v2.dirty & MEL_V2_HEAD));
    CHECK(put(&s, m, n - 1) == 0);       /* cut short: nothing */
    m[n - 1] = 0x80;                     /* a byte with its top bit set: nothing */
    CHECK(put(&s, m, n) == 0);
}

static void test_v2_tracks(void)
{
    mel_screen_t s;
    uint8_t m[96] = {MEL_TRACK, 1, 12, MEL_TF_ARMED | MEL_TF_DRUM, 127, 60, 0};
    uint32_t n = 7, i;
    mel_reset(&s);
    n += text(m + n, "P1");
    n += text(m + n, "P3");
    n += text(m + n, "808 KIT");
    n += text(m + n, "DRUMS");
    n += text(m + n, "");
    CHECK(put(&s, m, n) == MEL_TRACK);
    CHECK(s.v2.trk[1].numeral == 12 && s.v2.trk[1].flags == (MEL_TF_ARMED | MEL_TF_DRUM));
    CHECK(s.v2.trk[1].colour == mel_rgb7(127, 60, 0) && !strcmp(s.v2.trk[1].pattern, "P1") && !strcmp(s.v2.trk[1].next, "P3"));
    CHECK(!strcmp(s.v2.trk[1].name, "808 KIT") && !strcmp(s.v2.trk[1].role, "DRUMS"));
    m[1] = MEL_PLACES;
    CHECK(put(&s, m, n) == 0);
    m[1] = 0, m[2] = 100;                /* a numeral past 99: nothing */
    CHECK(put(&s, m, n) == 0 && s.v2.trk[0].numeral == 0);
    /* 20 steps, three a byte, the first the lowest: x . - x . . (3 = a rest too) ... */
    m[0] = MEL_STEPS, m[1] = 2, m[2] = 20;
    m[3] = 1 | 0 << 2 | 2 << 4, m[4] = 1 | 3 << 2, m[5] = 0, m[6] = 0, m[7] = 0, m[8] = 0, m[9] = 2 << 2;
    CHECK(put(&s, m, 10) == MEL_STEPS && s.v2.trk[2].len == 20);
    CHECK(mel_step(&s.v2.trk[2], 0) == 1 && mel_step(&s.v2.trk[2], 1) == 0 && mel_step(&s.v2.trk[2], 2) == 2 &&
          mel_step(&s.v2.trk[2], 3) == 1 && mel_step(&s.v2.trk[2], 4) == 0 && mel_step(&s.v2.trk[2], 19) == 2);
    for (i = 20; i < MEL_NSTEP; i++)
        CHECK(mel_step(&s.v2.trk[2], i) == 0);
    CHECK(put(&s, m, 9) == 0);           /* one byte short */
    m[2] = 0;
    CHECK(put(&s, m, 3) == 0);           /* no steps */
    m[0] = MEL_PLAY, m[1] = 3, m[2] = 127, m[3] = 63, m[4] = 64;
    CHECK(put(&s, m, 5) == MEL_PLAY && s.v2.trk[0].ph == 3 && s.v2.trk[1].ph == MEL_PH_NONE && s.v2.trk[2].ph == 63 &&
          s.v2.trk[3].ph == MEL_PH_NONE);
    m[0] = MEL_SEL, m[1] = 2, m[2] = 3;
    CHECK(put(&s, m, 3) == MEL_SEL && s.v2.sel == 2 && s.v2.shown == 3);
    m[2] = 0;
    CHECK(put(&s, m, 3) == 0);
    m[0] = MEL_NOTES, m[1] = 2, m[2] = 60, m[3] = 0, m[4] = 64, m[5] = 3;
    CHECK(put(&s, m, 6) == MEL_NOTES && s.v2.nnotes == 2 && s.v2.note[1] == 64 && s.v2.note_of[1] == 3);
    m[1] = 0;
    CHECK(put(&s, m, 2) == MEL_NOTES && s.v2.nnotes == 0);
    m[0] = MEL_SCOPE, m[1] = 3, m[2] = 64, m[3] = 127, m[4] = 0;
    CHECK(put(&s, m, 5) == MEL_SCOPE && s.v2.nscope == 3 && s.v2.scope[1] == 127);
    m[1] = 121;
    CHECK(put(&s, m, 5) == 0);
}

static void test_v2_parts(void)
{
    mel_screen_t s;
    uint8_t m[160];
    uint32_t n;
    mel_reset(&s);
    m[0] = MEL_CARD, m[1] = 3, m[2] = MEL_CV_DIM | MEL_CF_TRACK;
    n = 3 + text(m + 3, "LEVEL");
    n += text(m + n, "-4.0");
    n += text(m + n, "dB");
    CHECK(put(&s, m, n) == MEL_CARD && !strcmp(s.v2.card[3].label, "LEVEL") && !strcmp(s.v2.card[3].unit, "dB"));
    CHECK(s.v2.card[3].flags == (MEL_CV_DIM | MEL_CF_TRACK));
    m[1] = 4;
    CHECK(put(&s, m, n) == 0);
    /* the scale: A minor, WHITE, +2; its name with a middle dot (code 1) */
    m[0] = MEL_SCALE, m[1] = MEL_Q_WHITE, m[2] = 9, m[3] = 0x2D, m[4] = 0x0B, m[5] = 66;
    n = 6 + text(m + 6, "A\001B");
    CHECK(put(&s, m, n) == MEL_SCALE && s.v2.q == MEL_Q_WHITE && s.v2.root == 9 && s.v2.trans == 66);
    CHECK(s.v2.mask == 0x5AD && (uint8_t)s.v2.scale[1] == 0xB7);
    m[2] = 12;
    CHECK(put(&s, m, n) == 0 && s.v2.root == 9);
    /* a picture: ARP, up, two octaves, three held keys */
    m[0] = MEL_PICTURE, m[1] = MEL_PIC_ARP, m[2] = 1, m[3] = 2, m[4] = 3, m[5] = 48, m[6] = 52, m[7] = 55;
    CHECK(put(&s, m, 8) == MEL_PICTURE && s.v2.pic == MEL_PIC_ARP && s.v2.pic_n == 3 && s.v2.pic_note[2] == 55);
    CHECK(put(&s, m, 7) == 0);
    m[1] = MEL_PIC_CHORD, m[2] = 59, m[3] = 2, m[4] = 59, m[5] = 62;
    n = 6 + text(m + 6, "Bm7b5");
    n += text(m + n, "LAST");
    CHECK(put(&s, m, n) == MEL_PICTURE && !strcmp(s.v2.pic_text[0], "Bm7b5") && !strcmp(s.v2.pic_text[1], "LAST"));
    m[1] = MEL_NPIC;
    CHECK(put(&s, m, 2) == 0);
    /* a list, its row; a sheet, its row */
    m[0] = MEL_LIST, m[1] = 6, m[2] = 1, m[3] = 2, m[4] = MEL_LP_SCALE;
    CHECK(put(&s, m, 5) == MEL_LIST && s.v2.list_n == 6 && s.v2.list_sel == 2 && s.v2.list_pic == MEL_LP_SCALE);
    m[0] = MEL_LROW, m[1] = 4, m[2] = MEL_RF_MORE;
    n = 3 + text(m + 3, "Scale");
    n += text(m + n, "Min");
    CHECK(put(&s, m, n) == MEL_LROW && !strcmp(s.v2.lrow[4].label, "Scale") && s.v2.lrow[4].flags == MEL_RF_MORE);
    m[1] = MEL_LIST_ROWS;
    CHECK(put(&s, m, n) == 0);
    m[0] = MEL_SHEET, m[1] = 1, m[2] = 5, m[3] = 4;
    n = 4 + text(m + 4, "01 NIGHT DRIVE");
    n += text(m + n, "saved");
    CHECK(put(&s, m, n) == MEL_SHEET && s.v2.sheet_on && s.v2.sheet_n == 5 && s.v2.sheet_sel == 4);
    m[3] = 5;                            /* the row picked past its rows: nothing */
    CHECK(put(&s, m, n) == 0);
    m[0] = MEL_SROW, m[1] = 11, m[2] = MEL_SF_RED;
    n = 3 + text(m + 3, "Erase");
    n += text(m + n, "");
    CHECK(put(&s, m, n) == MEL_SROW && !strcmp(s.v2.srow[11].label, "Erase"));
    m[1] = MEL_SHEET_ROWS;
    CHECK(put(&s, m, n) == 0);
    /* the patterns: a row of eight, the page and the song's letters */
    m[0] = MEL_GRID, m[1] = 3, m[2] = MEL_GF_RIGHT;
    for (n = 0; n < 32u; n++)
        m[3 + n] = n % 4u ? 11 : MEL_GC_USED | MEL_GC_QUEUED;
    CHECK(put(&s, m, 35) == MEL_GRID && s.v2.trk[3].cell[7][0] == (MEL_GC_USED | MEL_GC_QUEUED) && s.v2.trk[3].cell[7][3] == 10);
    CHECK(put(&s, m, 34) == 0);
    m[0] = MEL_GRIDTOP, m[1] = 1, m[2] = 2, m[3] = 1, m[4] = 0;
    n = 5 + text(m + 5, "ABAC");
    CHECK(put(&s, m, n) == MEL_GRIDTOP && s.v2.grid_page == 1 && s.v2.grid_pages == 2 && s.v2.song_n == 4);
    m[1] = 2;
    CHECK(put(&s, m, n) == 0);
    /* a picker: two names, the second now */
    m[0] = MEL_PICK, m[1] = 1, m[2] = 0, m[3] = MEL_PKF_FX, m[4] = 2, m[5] = 1, m[6] = 3;
    n = 7 + text(m + 7, "DRIVE");
    m[n++] = 4;
    n += text(m + n, "TAPE");
    CHECK(put(&s, m, n) == MEL_PICK && s.v2.pick_n == 2 && s.v2.pick_now == 1 && s.v2.pick_fam[1] == 4 &&
          !strcmp(s.v2.pick[1], "TAPE"));
    m[5] = 2;
    CHECK(put(&s, m, n) == 0);
    /* a command past version 2's: nothing; CLEAR blanks version 2's too */
    m[0] = 0x4A;
    CHECK(put(&s, m, 1) == 0);
    m[0] = MEL_CLEAR;
    CHECK(put(&s, m, 1) == MEL_CLEAR && s.v2.page == MEL_PG_GENERIC && !s.v2.card[3].label[0] && !s.v2.sheet_on);
}

int main(void)
{
    test_header();
    test_list();
    test_cells_lines_popup();
    test_lights_keys();
    test_other();
    test_sx();
    test_hello_packets();
    test_v2_page_head();
    test_v2_tracks();
    test_v2_parts();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails != 0;
}
