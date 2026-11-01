/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The sound designer (Melodee FM-1's ui_designer.c: guided sounds of its own engines) is not on the remote: never
 * open (ui.designer_mode stays 0). */
static void designer_open(void) {}
static void designer_draw(void) {}
static int designer_screen(void) { return 0; }
static int designer_input(uint32_t pressed, uint32_t notes) { (void)pressed; (void)notes; return 0; }
static int designer_head_chip(char *b) { b[0] = 0; return 0; }
static void designer_pages(uint32_t *n, uint32_t *k) { *n = 0; *k = 0; }
static int designer_tabs_on(void) { return 0; }
static void designer_tabs(void) {}
static int designer_foot(void) { return 0; }
