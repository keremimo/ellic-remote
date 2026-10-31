/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* fm1-base's gfx.c (the platform's own screens: boot, crash, update) and Melodee FM-1's (ui/gfx.c, the remote's screen)
 * share names in the one unity build. The app takes Melodee FM-1's under these names (ux_*), and gives the platform its
 * own back at the end of app.c (gfx_names_end.h): main_fm1.c, after the app, draws with fm1-base's. */
#undef CV_MAX
#undef NPALETTES
#define cv_px ux_cv_px
#define cv_w ux_cv_w
#define cv_h ux_cv_h
#define cv_oy ux_cv_oy
#define palette_set ux_palette_set
#define swap16 ux_swap16
#define cv_begin ux_cv_begin
#define cv_blit ux_cv_blit
#define cv_blit_from ux_cv_blit_from
#define cv_pset ux_cv_pset
#define cv_rect ux_cv_rect
#define cv_line ux_cv_line
#define glyph ux_glyph
#define cv_text ux_cv_text
#define text_w ux_text_w
#define draw_text_box ux_draw_text_box
