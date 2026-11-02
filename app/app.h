/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Ellic Remote: the app's half of the contract with fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "ELLIC"                 /* the splash (the big face holds seven letters); "Ellic Remote" elsewhere */
#define APP_SPLASH_RGB 0xFBA0u           /* Saadet's amber */
#define APP_BLOCK 256u                   /* the audio runs, silent: Ellic Studio plays */
#define APP_SYSEX                        /* Ellic Studio's SysEx over USB: the frames fm1-base keeps (the simulator's) */
#define APP_USB_SYSEX_BYTE               /* .. and each byte as it comes (the overlay's bsp/chip/usb.c): a burst whole */
void app_usb_sysex_byte(uint8_t b);      /* (USB's interrupt) */
void mel_ble_sysex(uint8_t byte);        /* a byte of SysEx from Bluetooth (the overlay's ble/midi/le_midi.c) */

/* the store: the panel calibration (the platform's) where Saadet keeps it, so the two share it; Bluetooth's address
 * and pairing, and the settings, apart from Saadet's (README, Storage) */
enum { OBJ_PANEL, OBJ_BLE, OBJ_SETTINGS, OBJ_NOBJ };
#define APP_STORE_MAP {0xA2000u, 0xB0000u, 0xB2000u}

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
int app_sysex(const uint8_t *p, uint32_t n);
#define APP_REDRAW                       /* the screen again, whole (a screen mirror that lost a part, or begins) */
void app_redraw(void);

/* the host simulator: checks (expect, text) of Melodee FM-1's state in the unit */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
#define APP_HOST_FAILS
int app_host_fails(void);
