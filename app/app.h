/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee Remote: the app's half of the contract with fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "MELODEE"               /* the splash (the big face holds seven letters); "Melodee Remote" elsewhere */
#define APP_SPLASH_RGB 0xFBA0u           /* Saadet's amber */
#define APP_BLOCK 256u                   /* the audio runs, silent: nothing to play */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */
#define APP_SYSEX                        /* Melodee's SysEx over USB (Bluetooth's comes a byte at a time) */
#define APP_BLE_AT_BOOT (!mel_ble_off()) /* Bluetooth at power-on: as SETUP left it */
int mel_ble_off(void);
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

/* the host simulator: checks (expect, text), Melodee's side played from a script (sysex, blesysex: hex bytes) */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
#define APP_HOST_FAILS
int app_host_fails(void);
