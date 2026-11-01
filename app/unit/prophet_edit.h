/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Editing a Prophet patch on its pages (Melodee FM-1's prophet_edit.h) is not on the remote: no track is one. */
#pragma once
static const param_desc_t P5_PANEL[88], P5_STORE[4], P5_BEND_DISPLAY, P5_TUNE_DISPLAY[2];
static int16_t p5_cell[4], p5_store_slot;
static int p5_ed_active(const track_t *t, uint32_t id) { (void)t; (void)id; return 0; }
static void p5_edit_value(track_t *t, uint32_t id, int32_t value) { (void)t; (void)id; (void)value; }
static int p5_keyed_tune(const track_t *t, uint32_t id) { (void)t; (void)id; return 0; }
static int32_t p5_panel_raw(const track_t *t, uint32_t id, int32_t value) { (void)t; (void)id; return value; }
