/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Between fm1-base's side (app.c, in its unity build) and Melodee FM-1's (unit.c, a unit of its own: their names
 * never meet). The unit calls the few of fm1-base's plat.h below by hand: plat.h's panel names are Melodee FM-1's
 * own (panel.c B_* and EN_*), not to be met twice. app.c checks the numbers against plat.h's. */
#pragma once
#include <stdint.h>

/* the unit's, called by app.c (main loop only) */
void fm1u_start(void);                  /* Melodee FM-1's power-on: its settings, the tracks, Stage */
void fm1u_frame(void);                  /* a pass of Melodee FM-1's main loop: the panel, the LEDs, the screen */
void fm1u_redraw(void);                 /* the whole screen again at the next frame (the mirror lost a part) */
void fm1u_midi(uint32_t pkt, int port); /* a USB-MIDI packet from Melodee (port U_PORT_*): clock, notes */
int fm1u_sysex(const uint8_t *p, uint32_t n, int port);   /* a whole SysEx frame (F0 .. F7): 1 = the unit's */
void fm1u_lost(int port);               /* a frame cut short on a port (bytes lost): Melodee tells it all again */

#ifdef PLAT_HOST
/* the simulator's checks of Melodee FM-1's state (app.h APP_HOST_EXPECT, APP_HOST_CMD) */
int fm1u_expect(const char *what, int *got);
int fm1u_host_cmd(const char *cmd, const char *a, const char *b);
int fm1u_host_fails(void);
#endif

/* app.c's, called by the unit */
void app_lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c);
void app_lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px);
void app_lcd_sync(void);
void app_sysex_out(const uint8_t *f, uint32_t n, int port);   /* a whole SysEx frame to Melodee on port U_PORT_* */

/* fm1-base's (plat.h) */
#define U_PORT_USB 0                    /* PORT_USB, PORT_BLE, PORT_TRS */
#define U_PORT_BLE 1
#define U_PORT_TRS 2
#define U_TO_USB (1u << U_PORT_USB)     /* TO_USB, TO_BLE */
#define U_TO_BLE (1u << U_PORT_BLE)
uint32_t plat_ms(void);
uint32_t plat_buttons(void);            /* held: bit = label (panel.c B_*) */
uint32_t plat_keys(void);               /* held: bit k = key k (0 = F3) */
uint32_t plat_master(void);             /* MASTER's position, 0..4096 */
int32_t plat_enc(int role);             /* detents since the last call, + = clockwise; role panel.c EN_* */
void plat_lights(uint32_t buttons, uint32_t keys, uint32_t dim_buttons, uint32_t dim_keys, int glow);
void plat_lights_mid(uint32_t buttons, uint32_t keys);
void plat_lights_planes(const uint32_t *buttons, const uint32_t *keys, const uint8_t *mask);   /* (the overlay's) */
void plat_midi_send(uint32_t pkt, uint32_t to);
uint32_t plat_midi_out_room(uint32_t to);
void plat_yield(void);
int plat_ble_connected(void);           /* a central is connected and listening */
uint32_t plat_midi_out_link(void);      /* changes at each new USB connection */
