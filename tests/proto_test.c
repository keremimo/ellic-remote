/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The protocol's half without hardware (app/mel_proto.c): Melodee's messages read into the screen, malformed ones
 * changing nothing, SysEx gathered a byte at a time, and the FM-1's HELLO as USB-MIDI packets.
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
    m[6] = 0x01;                         /* (m[4] its length, m[5] 'A') */
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
    uint8_t f[16];
    uint32_t pk[8], n = mel_hello(f, 0, 1, 2, 3, -1, MEL_K_BUTTONS), k;
    CHECK(n == 13 && f[0] == 0xF0 && f[12] == 0xF7);
    CHECK(f[4] == MEL_HELLO && f[5] == MEL_PROTO && f[7] == 1 && f[9] == 3 && f[10] == 63 && f[11] == MEL_K_BUTTONS);
    k = mel_packets(f, n, pk, 8);
    CHECK(k == 5);
    CHECK(pk[0] == (4u | 0xF0u << 8 | (uint32_t)MEL_ID0 << 16 | (uint32_t)MEL_ID1 << 24));
    CHECK((pk[4] & 0xFu) == 5u && ((pk[4] >> 8) & 0xFFu) == 0xF7u);   /* 13 = 4 x 3 + 1: the end alone */
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

int main(void)
{
    test_header();
    test_list();
    test_cells_lines_popup();
    test_lights_keys();
    test_other();
    test_sx();
    test_hello_packets();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails != 0;
}
