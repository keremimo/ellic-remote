/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's click (its click.c) is not on the remote: Melodee counts in and clicks. The sequencer still asks
 * for one; nobody plays it. */
enum { CLICK_OFF, CLICK_REC, CLICK_ON };
static volatile uint8_t click_req;
static struct { int32_t env; } clk;
