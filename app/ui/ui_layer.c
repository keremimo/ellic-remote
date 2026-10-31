/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's GLO layer (its firmware/src/ui_layer.c), ported to Melodee Remote: GLO held, its map over the page:
 * the header names it ("[GLO] SET", Melodee's status), the four places' levels as the strip at the top, each label in
 * its track's colour (Melodee's MEL_CARD with MEL_CF_TRACK; muted, not sounding: dim), the keys as a keyboard under them:
 * the black keys MUTE 1..4 (muted: red), the white keys SOLO 1..4 (soloed: the track's colour), C unmutes all, the high
 * F taps the tempo (an outside clock: dim, MEL_PF_TAP_DIM). Melodee plays the keys (KEYS flag 1: buttons). Part of
 * app.c */
static struct { uint32_t keys; } lyv;

/* a panel h rows tall at y, drawn by fn in slices (ui_pages.c pv_panel: fn draws in panel coordinates) */
#define PV_SLICE 124                                    /* rows of a panel slice (236 x 124 fits CV_MAX) */
static void pv_panel(int32_t y, int32_t h, void (*fn)(int32_t h))
{
    int32_t top;
    for (top = 0; top < h; top += PV_SLICE) {
        int32_t sh = h - top < PV_SLICE ? h - top : PV_SLICE;
        for (uint32_t band = 0; band < (uint32_t)((uint32_t)sh); band += CV_MAX / (PV_PANEL_W)) {
            cv_begin_band(PV_PANEL_W,(uint32_t)sh,T_BG,band);
            cv_oy = -top;
            cv_rrect(0, 0, PV_PANEL_W, h, 6, T_PANEL, T_BG);
            cv_bg = T_PANEL;
            fn(h);
            cv_oy = 0;
            cv_blit_band(PV_PANEL_X, (uint32_t)(y + top));
        }
    }
}
/* GLO held (docs/design: mock/r6_pages GLO): the keys as a keyboard: the black keys MUTE 1..4 (muted: red), the white
 * keys SOLO 1..4 (held: the track's colour), C unmutes all, the high F taps the tempo (an outside clock: dim) */
static void layer_glo_keys(int32_t h)
{
    static const char *const WL[8] = {"Solo", "Solo", "Solo", "Solo", "Mute", "", "", "Tap"};
    static const uint8_t BX[NPART] = {0, 1, 2, 4};      /* black key t: between white BX and BX + 1 (F# G# A# C#) */
    const int32_t kw = 28, x0 = 1, wy = 50, by = 6, bh = 40;
    uint32_t i;
    for (i = 0; i < 8u; i++) {                          /* the white keys: the solos' numerals, All (unmute), Tap */
        int32_t x = x0 + (int32_t)i * (kw + 1);
        int on = i < NPART && (TRK(i)->flags & MEL_TF_SOLO), none = !WL[i][0] || (i < NPART && trk_free(i)),
            dim = i == 7u && (V->page_flags & MEL_PF_TAP_DIM);
        uint16_t f = on ? T_TRK(i) : none ? ux_mix(T_PANEL, T_SURF, 50) : T_SURF, ink = on ? T_INK : dim ? T_DIM :
                     i < NPART ? T_TRK(i) : T_TEXT;
        char d[3];
        if (i < NPART)
            numeral_text(d, i);
        cv_rrect(x, wy, kw, h - wy - 6, 4, f, T_PANEL);
        if (!none)
            cv_text_c(x + kw / 2, ty_y(h - 26, i < NPART ? TY_TX : TY_LB), i < NPART ? TY_TX : TY_LB,
                      i < NPART ? d : i == 4u ? "All" : "Tap", ink, f);
    }
    for (i = 0; i < NPART; i++) {                       /* the black keys: the mutes (muted: red) */
        int32_t x = x0 + (int32_t)(BX[i] + 1u) * (kw + 1) - 14;
        int m = (TRK(i)->flags & MEL_TF_MUTED) != 0u;
        uint16_t f = m ? T_REC : T_LIFT;
        char n[3];
        if (trk_free(i)) {                              /* (a track not in use: its key blank) */
            cv_rrect(x, by, 26, bh, 4, ux_mix(T_PANEL, T_SURF, 50), T_PANEL);
            continue;
        }
        numeral_text(n, i);
        cv_rrect(x, by, 26, bh, 4, f, T_PANEL);
        cv_icon_mid(x + 5, by + 12, 16, ICON_MUTE, m ? T_TEXT : T_SEC, f);
        cv_text_c(x + 13, ty_y(by + bh - 10, TY_LB), TY_LB, n, m ? T_TEXT : T_SEC, f);
    }
}
/* the four levels as strips at the top, each track's colour; the keys as a keyboard under them */
static void layer_draw(void)
{
    uint32_t sig = 7919u + ux.gen * 977u + ux.pal * 31u + V->page_flags * 5u, k;
    if (ui.force)
        lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    draw_head();
    card_y = Y_LABEL;
    draw_columns();
    for (k = 0; k < NPART; k++)
        sig = (sig ^ (TRK(k)->flags * 7u + TRK(k)->numeral * 131u + T_TRK(k) * 3u)) * 16777619u;
    if (ui.force || sig != lyv.keys) {                  /* (R6: levels at the top, the keys as a keyboard under them) */
        lyv.keys = sig;
        pv_panel(Y_GRAPH, 240 - Y_GRAPH - 2, layer_glo_keys);
    }
}
