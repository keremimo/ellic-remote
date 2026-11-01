/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee Remote: the FM-1 as Melodee's remote, over Bluetooth MIDI or USB. It runs Melodee FM-1's own pages and
 * controls (its sequencer, its tracks, its notes), a unit of their own (unit/unit.c, from fm1/), and Melodee makes
 * the sound: the keys' notes go to it as MIDI, the model is kept with its own (unit/link.c, docs/PROTOCOL.md). This is
 * fm1-base's side: the unit's screen, panel and MIDI on the platform, the SysEx gathered for it. No sound: the audio
 * runs silent. Part of the unity build after the platform (device/unity.c, host/plat_host.c). */
#include "unit/unit.h"

_Static_assert(U_PORT_USB == PORT_USB && U_PORT_BLE == PORT_BLE && U_PORT_TRS == PORT_TRS, "unit.h's ports");
_Static_assert(U_TO_USB == TO_USB && U_TO_BLE == TO_BLE, "unit.h's ports");

/* ----------------------------------------------------------------- audio --- */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i;
    (void)master_q12;
    for (i = 0; i < 2u * n; i++)
        lr[i] = 0;
}

void app_panic(void) {}

void app_init(void) {}

/* ------------------------------------------------------- the unit's calls --- */
void app_lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { lcd_fill(x, y, w, h, c); }
void app_lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px) { lcd_blit(x, y, w, h, px); }
void app_lcd_sync(void) { lcd_sync(); }

/* a SysEx frame (F0 .. F7) to Melodee on a port, as USB-MIDI packets: sent whole or not at all */
void app_sysex_out(const uint8_t *f, uint32_t n, int port)
{
    uint32_t to = port == PORT_BLE ? TO_BLE : TO_USB, k = (n + 2u) / 3u, i;
    if ((to == TO_BLE && !plat_ble_connected()) || plat_midi_out_room(to) < k)
        return;
    for (i = 0; i < n; i += 3u) {
        uint32_t left = n - i, cin = left > 3u ? 4u : 4u + left;
        uint32_t pkt = cin | (uint32_t)f[i] << 8;
        if (left > 1u)
            pkt |= (uint32_t)f[i + 1u] << 16;
        if (left > 2u)
            pkt |= (uint32_t)f[i + 2u] << 24;
        plat_midi_send(pkt, to);
    }
}

/* --------------------------------------------------------- SysEx coming in --- */
/* a frame gathered a byte at a time (Bluetooth's, and USB's: the overlay hands every byte over, felucca-bsp keeps
 * one frame only and a burst would lose the rest) */
#define SX_MAX 1024u
typedef struct {
    uint8_t buf[SX_MAX];
    uint32_t n;
    uint8_t on;
} sx_t;
static int sx_byte(sx_t *a, uint8_t b)
{
    if (b >= 0xF8u)
        return 0;                                  /* (realtime may come anywhere) */
    if (b == 0xF0u) {
        a->n = 0;
        a->on = 1;
    } else if (!a->on) {
        return 0;
    } else if (b & 0x80u && b != 0xF7u) {
        a->on = 0;                                 /* another status cuts it short */
        return 0;
    }
    if (a->n >= SX_MAX) {
        a->on = 0;
        return 0;
    }
    a->buf[a->n++] = b;
    if (b == 0xF7u) {
        a->on = 0;
        return 1;
    }
    return 0;
}
static sx_t sx_ble PLAT_POOL, sx_usb PLAT_POOL;
void mel_ble_sysex(uint8_t byte)
{
    if (sx_byte(&sx_ble, byte))
        (void)fm1u_sysex(sx_ble.buf, sx_ble.n, PORT_BLE);
}

#define USB_Q 4096u
static volatile uint8_t usb_q[USB_Q] PLAT_POOL;
static volatile uint32_t usb_w, usb_r;
static volatile uint8_t usb_over;                 /* bytes lost: the frame being gathered is dropped */
void app_usb_sysex_byte(uint8_t b)
{
    uint32_t w = usb_w;
    if (w - usb_r >= USB_Q) {
        usb_over = 1;
        return;
    }
    usb_q[w % USB_Q] = b;
    __asm__ volatile("" ::: "memory");             /* (the byte before the index) */
    usb_w = w + 1u;
}
static void usb_take(void)
{
    while (usb_r != usb_w) {
        uint8_t b = usb_q[usb_r % USB_Q];
        __asm__ volatile("" ::: "memory");
        usb_r++;
        if (usb_over && b != 0xF0u)                /* (lost bytes: nothing until the next frame's start) */
            continue;
        usb_over = 0;
        if (sx_byte(&sx_usb, b))
            (void)fm1u_sysex(sx_usb.buf, sx_usb.n, PORT_USB);
    }
}
#ifdef PLAT_HOST
int app_sysex(const uint8_t *p, uint32_t n) { return fm1u_sysex(p, n, PORT_USB); }   /* (the simulator: whole frames) */
#else
/* (had byte by byte: usb_take) Melodee's frames are the app's; every other one the platform's (the installer, the
 * backup, the mirror, a screen reader) */
int app_sysex(const uint8_t *p, uint32_t n) { return n >= 5u && p[1] == 0x7D && p[2] == 0x4D && p[3] == 0x52; }
#endif

/* ------------------------------------------------------------- the frame --- */
void app_say(const char *a, const char *b) { (void)a; (void)b; }

void app_ui_init(void) { fm1u_start(); }

void app_redraw(void) { fm1u_redraw(); }

static void midi_in(void)
{
    uint32_t pkt;
    int port;
    usb_take();
    while (plat_midi_in(&pkt, &port))
        if ((pkt & 0xFu) < 4u || (pkt & 0xFu) > 7u)   /* (SysEx comes whole, above) */
            fm1u_midi(pkt, port);
}

void app_frame(void)
{
    midi_in();
    fm1u_frame();
}

void app_input_only(void)
{
    midi_in();
}

/* ------------------------------------------------------------ simulator --- */
#ifdef PLAT_HOST
int app_expect(const char *what, int *got) { return fm1u_expect(what, got); }
/* sysex HEX..., blesysex HEX...: a frame from Melodee on USB or Bluetooth (the whole line: plat_host_line) */
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    (void)c;
    (void)d;
    if (!strcmp(cmd, "sysex") || !strcmp(cmd, "blesysex")) {
        static uint8_t f[1024];
        const char *q = plat_host_line + strlen(cmd);
        uint32_t n = 0;
        unsigned v;
        int used;
        while (n < sizeof f && sscanf(q, " %x%n", &v, &used) == 1) {
            f[n++] = (uint8_t)v;
            q += used;
        }
        fm1u_sysex(f, n, cmd[0] == 'b' ? PORT_BLE : PORT_USB);
        return 1;
    }
    return fm1u_host_cmd(cmd, a, b);
}
int app_host_fails(void) { return fm1u_host_fails(); }
#endif
