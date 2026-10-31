/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Bluetooth MIDI SysEx as the overlay makes it (tools/base_overlay.py): the patched midi_parse and
 * ble_midi_send_many, cut out of the build's le_midi.c by tests/ble_midi_test.py, on stand-ins for JieLi's stack.
 * In: SysEx whole, split over packets, with real time inside, cut short; out: SysEx packed into notifications that
 * the parser reads back byte for byte, at the smallest MTU and a larger one. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define BLE_MIDI_CENTRAL 0
#define H_MIDI 6
#define H_CCC 7
#define ATT_OP_AUTO_READ_CCC 0
static int con_handle = 1;
static int periph_ready(void) { return 1; }
static int mc_ready(void) { return 0; }
int ble_midi_connected(void) { return 1; }
static void ble_op_att_get_remain(u32 *r) { *r = 1000; }

static u8 note[64][128];                 /* the notifications sent */
static u32 note_len[64], nnote;
static int SEND(int h, int att, const u8 *d, u32 n, int op)
{
    (void)h, (void)att, (void)op;
    memcpy(note[nnote], d, n);
    note_len[nnote++] = n;
    return 0;
}

static uint32_t rx[256];                 /* what the parser gave the platform, and the app */
static u32 nrx;
void ble_midi_receive(uint32_t pkt) { rx[nrx++] = pkt; }
static u8 sx[2048];
static u32 nsx;
void mel_ble_sysex(u8 b) { sx[nsx++] = b; }

#include "le_midi_cut.c"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void parse(const u8 *b, u16 n) { midi_parse(b, n); }

static void reset(void)
{
    nrx = nsx = nnote = 0;
    sx_in = sx_out = 0;
}

static void test_in(void)
{
    static const u8 whole[] = {0x80, 0x80, 0xF0, 0x7D, 0x4D, 0x52, 0x01, 0x01, 0x80, 0xF7};
    static const u8 want[] = {0xF0, 0x7D, 0x4D, 0x52, 0x01, 0x01, 0xF7};
    static const u8 p1[] = {0x80, 0x81, 0xF0, 0x7D, 0x4D}, p2[] = {0x80, 0x52, 0x01, 0x01, 0x82, 0xF7};
    static const u8 then_note[] = {0x80, 0x80, 0xF0, 0x01, 0x80, 0xF7, 0x83, 0x90, 0x3C, 0x64, 0x3E, 0x40};
    static const u8 rt[] = {0x80, 0x80, 0xF0, 0x01, 0x81, 0xF8, 0x02, 0x82, 0xF7};
    static const u8 cut[] = {0x80, 0x80, 0xF0, 0x01, 0x81, 0x90, 0x3C, 0x64};
    reset();
    parse(whole, sizeof whole);
    CHECK(nsx == sizeof want && !memcmp(sx, want, sizeof want) && nrx == 0 && !sx_in);
    reset();
    parse(p1, sizeof p1);                /* split: the second packet goes on straight after its header */
    CHECK(sx_in);
    parse(p2, sizeof p2);
    CHECK(nsx == sizeof want && !memcmp(sx, want, sizeof want) && !sx_in);
    reset();
    parse(then_note, sizeof then_note);  /* a SysEx, then notes with running status */
    CHECK(nsx == 3 && sx[2] == 0xF7);
    CHECK(nrx == 2 && rx[0] == (0x9u | 0x90u << 8 | 0x3Cu << 16 | 0x64u << 24) && ((rx[1] >> 16) & 0xFF) == 0x3E);
    reset();
    parse(rt, sizeof rt);                /* clock in the middle of a SysEx */
    CHECK(nsx == 4 && sx[0] == 0xF0 && sx[1] == 0x01 && sx[2] == 0x02 && sx[3] == 0xF7);
    CHECK(nrx == 1 && rx[0] == (0xFu | 0xF8u << 8));
    reset();
    parse(cut, sizeof cut);              /* cut short by a note: the app hears the status, the note still plays */
    CHECK(nsx == 3 && sx[2] == 0x90 && !sx_in);
    CHECK(nrx == 1 && ((rx[0] >> 8) & 0xFF) == 0x90);
    {
        static const u8 notes[] = {0x80, 0x80, 0x90, 0x3C, 0x64, 0x3E, 0x64, 0x81, 0xB0, 0x07, 0x10};
        reset();
        parse(notes, sizeof notes);      /* nothing else changed: notes, running status, a controller */
        CHECK(nrx == 3 && nsx == 0 && ((rx[2] >> 8) & 0xFF) == 0xB0 && (rx[2] >> 24) == 0x10);
    }
}

/* a frame as USB-MIDI packets (cable 0) */
static u32 packets(const u8 *f, u32 n, uint32_t *pk)
{
    u32 i = 0, k = 0;
    while (i < n) {
        u32 left = n - i, take = left > 3 ? 3 : left, j, p = left <= 3 ? 4 + take : 4;
        for (j = 0; j < take; j++)
            p |= (uint32_t)f[i + j] << (8 + 8 * j);
        pk[k++] = p;
        i += take;
    }
    return k;
}

/* send everything as the radio's drain does (what fits each time), then read every notification back */
static void round_trip(const uint32_t *pk, u32 n, u16 mtu)
{
    uint16_t ms[256] = {0};
    u32 at = 0, i, tries = 0;
    send_mtu = mtu;
    while (at < n && tries++ < 200) {
        int sent = ble_midi_send_many(pk + at, ms + at, (int)(n - at));
        if (sent <= 0)
            break;
        at += (u32)sent;
    }
    CHECK(at == n);
    for (i = 0; i < nnote; i++) {
        CHECK(note_len[i] <= mtu);
        parse(note[i], (u16)note_len[i]);
    }
}

static void test_out(void)
{
    u8 f[300];
    uint32_t pk[128];
    u32 n, k, i;
    /* HELLO, 13 bytes: one notification, F0 and F7 each after a timestamp */
    static const u8 hello[] = {0xF0, 0x7D, 0x4D, 0x52, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00, 0xF7};
    reset();
    k = packets(hello, sizeof hello, pk);
    round_trip(pk, k, 20);
    CHECK(nnote == 1 && note_len[0] == 1 + 2 + 11 + 2);
    CHECK(note[0][1] == 0x80 && note[0][2] == 0xF0 && note[0][note_len[0] - 2] == 0x80 && note[0][note_len[0] - 1] == 0xF7);
    CHECK(nsx == sizeof hello && !memcmp(sx, hello, sizeof hello) && !sx_out);
    /* a long one over several notifications, at the smallest MTU and at a larger one, then a note after it */
    for (n = 0; n < 200; n++)
        f[n] = (u8)(n & 0x7F);
    f[0] = 0xF0;
    f[199] = 0xF7;
    for (i = 0; i < 2; i++) {
        reset();
        k = packets(f, 200, pk);
        pk[k++] = 0x9u | 0x90u << 8 | 0x3Cu << 16 | 0x64u << 24;
        round_trip(pk, k, i ? 182 : 20);
        CHECK(nsx == 200 && !memcmp(sx, f, 200));
        CHECK(nrx == 1 && rx[0] == (0x9u | 0x90u << 8 | 0x3Cu << 16 | 0x64u << 24));
        CHECK(i ? nnote == 4 : nnote == 12);   /* (a notification holds 64 bytes at most: the stack's buffer) */
    }
}

int main(void)
{
    test_in();
    test_out();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails != 0;
}
