/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The end of the app (gfx_names.h): fm1-base's gfx.c names back for the platform after it */
#undef cv_px
#undef cv_w
#undef cv_h
#undef cv_oy
#undef palette_set
#undef swap16
#undef cv_begin
#undef cv_blit
#undef cv_blit_from
#undef cv_pset
#undef cv_rect
#undef cv_line
#undef glyph
#undef cv_text
#undef text_w
#undef draw_text_box
#undef lcd_fill
#undef CV_MAX
#define CV_MAX APP_CANVAS_PX
#undef NPALETTES
#define NPALETTES (sizeof(PALETTES) / sizeof(PALETTES[0]))
