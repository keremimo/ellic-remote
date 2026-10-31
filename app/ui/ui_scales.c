/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's SCL > SCALES (its firmware/src/ui_scales.c), ported to Melodee Remote: a screen of its own, in the
 * large type: at the top the track's numeral and the family (KNOB 1) in its colour, the scale's place in it at the
 * right; under it the family's scales (KNOB 2): the one picked a card tinted in its family's shade (its name, its notes
 * and period, its star), two before and two after it, fading; the place as a bar at the right edge. Melodee's
 * MEL_SCALES and MEL_SCROW (each scale's name, its shade, the card's words and star). Part of app.c */
#define SC_BODY_Y 32                                    /* the scales: rows 32 .. 237 (as the browser's) */
#define SC_ROW 34
#define SC_CARD 60
static struct { uint32_t top, list; } scv;

static void scales_draw(void)
{
    uint32_t total = V->sc_total, rank = V->sc_rank, sig;
    int message = ui.msg[0] && !head_toast(BPM_X + BPM_W + 6);   /* longer statuses use draw_toast */
    const aafont_t *tf = TY_TX, *lf = TY_LB;
    char b[32];
    if (ui.force) {
        lcd_fill(0, 0, 240, 240, T_BG);
        scv.top = scv.list = 0;
    }
    sig = str_hash(total * 7u + rank * 3001u + V->sel * 131u + ux.gen * 977u + ux.pal * 31u + 1u + trk_numeral(V->sel) * 13u +
                   (message ? str_hash(3u, ui.msg) : 0u), V->sc_family);
    if (V->head_flags & MEL_HF_STATUS_RED) {            /* (an ongoing status: the shared header) */
        draw_head();
    } else if (sig != scv.top) {                        /* the numeral, the family, the place */
        char d[3];
        int32_t cy = H_HEAD / 2;
        numeral_text(d, V->sel);
        scv.top = sig;
        cv_begin(240, H_HEAD, T_BG);
        cv_rrect(4, cy - 11, 22, 22, 6, T_THEME, T_BG);
        cv_text_c(15, ty_y(cy, lf), lf, d, T_INK, T_THEME);
        cv_text_on(34, ty_y(cy, tf), tf, message ? ui.msg : V->sc_family, message ? T_ACCENT : T_THEME, T_BG);
        if (!message && total && rank < total) {
            fmt_int(b, (int32_t)rank + 1);
            str_cpy(b + str_len(b), "/", 2);
            fmt_int(b + str_len(b), (int32_t)total);
            cv_text_r(236, ty_y(cy, lf), lf, b, T_MID, T_BG);
        }
        cv_blit(0, 0);
    }
    sig = mem_hash(rank * 7u + total * 40503u + ux.gen * 977u + ux.pal * 31u + T_THEME, V->scrow, sizeof V->scrow);
    if (sig != scv.list) {                              /* the scales: two before the card, two after it */
        int32_t d, top, h = 240 - SC_BODY_Y - 2;
        const mel_scrow_t *card = &V->scrow[2];
        scv.list = sig;
        for (top = 0; top < h; top += 124) {            /* (in two slices: a canvas holds 124 rows) */
            int32_t sh = h - top < 124 ? h - top : 124;
            for (uint32_t band = 0; band < (uint32_t)((uint32_t)sh); band += CV_MAX / (240)) {
                cv_begin_band(240,(uint32_t)sh,T_BG,band);
                cv_oy = -top;
                if (!total || (card->flags & MEL_SCF_WORDS))   /* ("No favourites", "Not in this family") */
                    cv_text_c(120, ty_y(h / 2, tf), tf, card->name[0] ? card->name : "No scales", T_MID, T_BG);
                for (d = -2; d <= 2 && total && !(card->flags & MEL_SCF_WORDS); d++) {
                    const mel_scrow_t *w = &V->scrow[d + 2];
                    int32_t y = d < 0 ? (d + 2) * SC_ROW : 2 * SC_ROW + SC_CARD + (d - 1) * SC_ROW;
                    uint16_t hc = T_KNOB(w->shade);         /* (each family its shade of the track's colour) */
                    if (!w->name[0])
                        continue;
                    str_cpy(b, w->name, sizeof b);
                    if (!d) {                               /* the card */
                        uint16_t bg = ux_mix(T_BG, hc, 18);
                        y = 2 * SC_ROW;
                        cv_rrect(BOX_X, y, BOX_W, SC_CARD - 2, 8, bg, T_BG);
                        cv_free_text(12, ty_y(y + 18, ty_fit(tf, b, 222)), ty_fit(tf, b, 222), b, T_TEXT, bg, 222);
                        cv_text_on(12, ty_y(y + 42, lf), lf, w->detail, hc, bg);
                        cv_icon_mid(212, y + 42, 16, (w->flags & MEL_SCF_STAR) ? ICON_X_STAR : ICON_X_STAR_O,
                                    (w->flags & MEL_SCF_STAR) ? hc : T_DIM, bg);
                        continue;
                    }
                    cv_free_text(12, ty_y(y + SC_ROW / 2 - 1, ty_fit(tf, b, 220)), ty_fit(tf, b, 220), b,
                                 ux_mix(T_BG, hc, d == 1 || d == -1 ? 78 : 45), T_BG, 220);
                }
                if (total > 1u && rank < total) {           /* where it is */
                    int32_t th = 24, ty = (int32_t)rank * (h - th) / (int32_t)(total - 1u);
                    cv_rect(238, 0, 2, h, T_LINE);
                    cv_rect(238, ty, 2, th, T_THEME);
                }
                cv_oy = 0;
                cv_blit_band(0, (uint32_t)(SC_BODY_Y + top));
            }
        }
    }
}
