/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's list pages (its firmware/src/ui_list.c), ported to Melodee Remote: the pages one sets and leaves as a
 * list, a row a setting (SCL: Root, Scale, Quantize, Transpose ..): KNOB 2 the row, KNOB 1 its value, OCT+ a list
 * value's whole list (SCL's Scale: the SCALES page), OCT- back: all of it Melodee's (MEL_LIST, MEL_LROW: the rows shown,
 * the row picked, where they are in the list); the page's picture under its rows (SCL: the octave, the scale lit:
 * MEL_SCALE). Part of app.c */
#define LIST_Y0 Y_LABEL                                 /* the rows (large type, C): 32 px, 34 apart from y 32 */
#define LIST_ROW 34
#define LIST_RH 32
static struct { uint32_t sig, pic; } lst;

/* the octave under SCL's rows: the scale's notes lit in the track's colour, the root in text */
static void list_scale_pic(int32_t y, int32_t h)
{
    static const uint8_t WHITE[7] = {0, 2, 4, 5, 7, 9, 11}, BLACK[5][2] = {{1, 0}, {3, 1}, {6, 3}, {8, 4}, {10, 5}};
    uint32_t mask = V->mask, root = (uint32_t)V->root % 12u, i;
    for (uint32_t band = 0; band < (uint32_t)((uint32_t)h); band += CV_MAX / (240)) {
        cv_begin_band(240,(uint32_t)h,T_BG,band);                   /* (on the background: no panel) */
        for (i = 0; i < 7u; i++) {
            uint32_t n = WHITE[i], on = (mask >> ((n + 12u - root) % 12u)) & 1u;
            uint16_t c = n == root ? T_TEXT : on ? T_THEME : T_SURF;
            cv_rrect(20 + 29 * (int32_t)i, 4, 27, h - 8, 4, c, T_BG);
        }
        for (i = 0; i < 5u; i++) {
            uint32_t n = BLACK[i][0], on = (mask >> ((n + 12u - root) % 12u)) & 1u;
            uint16_t c = n == root ? T_TEXT : on ? ux_mix(T_BG, T_THEME, 75) : T_BG;
            int32_t x = 20 + 29 * BLACK[i][1] + 19;
            cv_rrect(x, 4, 16, (h - 8) * 58 / 100, 3, T_LINE, T_BG);
            cv_rrect(x + 1, 5, 14, (h - 8) * 58 / 100 - 2, 2, c, T_LINE);
        }
        cv_blit_band(0, (uint32_t)y);
    }
}

/* a row (large type, C: the lists, the menu, the slots, the sheets) at canvas row y: its name at the left and its value
 * at the right (the text face, capitals on the row's middle; a value ON / OFF a badge), "more" (a list behind it) a
 * ">" after the value. Row k of a list has a colour (T_KNOB(k), the four in turn): its value in it; the row picked
 * tinted in it, its name in text; the others' names secondary (vc T_DIM: dim). Returns the value's left edge */
static int32_t row_draw(int32_t y, const char *lb, const char *v, uint16_t vc, int on, int more, uint32_t k)
{
    const aafont_t *tf = TY_TX, *vf = tf, *nf = tf;
    uint16_t hc = T_KNOB(k), bg = on ? ux_mix(T_BG, hc, 18) : T_BG;
    int32_t cy = y + LIST_RH / 2, xr = more ? 218 : 228, vx = xr, vw, room = xr - 12 - 8;
    if (v[0] && binary_value(v) < 0 && text_w(tf, lb) + text_w(tf, v) > room)
        vf = TY_LB;                                     /* (both do not fit: the value smaller, then the name) */
    if (v[0] && binary_value(v) < 0 && text_w(tf, lb) + text_w(vf, v) > room)
        nf = TY_LB;
    if (on)
        cv_rrect(BOX_X, y, BOX_W, LIST_RH, 8, bg, T_BG);
    if (v[0] && binary_value(v) >= 0) {
        vw = text_w(TY_LB, v) + 12;
        binary_badge(xr - vw / 2, cy - 11, 22, v, bg);
        vx = xr - vw;
    } else if (v[0]) {
        vw = text_w(vf, v);
        if (vw > 150) vw = 150;
        vx = xr - vw;
        cv_free_text(vx, ty_y(cy, vf), vf, v, vc == T_DIM ? T_DIM : on ? hc : ux_mix(T_BG, hc, 80), bg, 150);
    }
    if (more)
        cv_text_on(222, ty_y(cy, tf), tf, ">", on ? hc : T_DIM, bg);
    nf = nf == tf ? ty_fit(tf, lb, vx - 8 - 12) : nf;
    cv_free_text(12, ty_y(cy, nf), nf, lb, on ? T_TEXT : T_SEC, bg, vx - 8 - 12);
    return vx;
}
/* the rows shown (Melodee's window: row i is the list's first + i), the row picked, where they are as a bar at the
 * right edge */
static void list_rows(uint32_t n, uint32_t row, uint32_t first, uint32_t vis)
{
    uint32_t r;
    int32_t top;
    for (top = 0; top < (int32_t)vis * LIST_ROW; ) {   /* in slices between rows */
        int32_t sh = (int32_t)vis * LIST_ROW - top <= 3 * LIST_ROW ? (int32_t)vis * LIST_ROW - top : 3 * LIST_ROW;
        for (uint32_t band = 0; band < (uint32_t)((uint32_t)sh); band += CV_MAX / (240)) {
            cv_begin_band(240,(uint32_t)sh,T_BG,band);
            cv_oy = -top;
            for (r = first; r < n && r < first + vis; r++) {
                const mel_lrow_t *w = &V->lrow[r - first];
                int32_t y = (int32_t)(r - first) * LIST_ROW;
                char v[MEL_VAL_N + 1];
                if (y + LIST_ROW <= top || y >= top + sh || (!w->label[0] && !w->value[0]))
                    continue;
                gen_words(v, w->value, sizeof v);
                row_draw(y, w->label, v, (w->flags & MEL_RF_DIM) ? T_DIM : T_TEXT,
                         r == row, (w->flags & MEL_RF_MORE) != 0u, r);
            }
            if (n > vis) {
                int32_t th = (int32_t)vis * LIST_ROW * (int32_t)vis / (int32_t)n;
                int32_t ty = (int32_t)((vis * LIST_ROW - (uint32_t)th) * first / (n - vis));
                cv_rect(238, 0, 2, (int32_t)vis * LIST_ROW, T_LINE);
                cv_rect(238, ty, 2, th, T_THEME);
            }
            cv_oy = 0;
            cv_blit_band(0, (uint32_t)(LIST_Y0 + top));
        }
        top += sh;
    }
}
static void list_draw(void)
{
    uint32_t sig, vis, pic = V->list_pic == MEL_LP_SCALE, n = V->list_n;
    int32_t pic_y = pic ? LIST_Y0 + 4 * LIST_ROW + 2 : PV_SOUND_Y;
    if (ui.force) {
        lcd_fill(0, H_HEAD, 240, PV_SOUND_Y - H_HEAD, T_BG);
        lst.pic = 0;
    }
    draw_head();
    pv_sound();
    vis = (uint32_t)((pic_y - LIST_Y0 + 2) / LIST_ROW);
    sig = mem_hash(V->list_sel * 7u + V->list_first * 131u + n * 1031u + ux.gen * 977u + ux.pal * 31u + V->sel + vis,
                   V->lrow, sizeof V->lrow);
    if (ui.force || sig != lst.sig) {
        lst.sig = sig;
        if (ui.force || vis * LIST_ROW < (uint32_t)(pic_y - LIST_Y0))
            lcd_fill(0, (uint32_t)(LIST_Y0 + (int32_t)vis * LIST_ROW), 240, (uint32_t)(pic_y - LIST_Y0) - vis * LIST_ROW, T_BG);
        list_rows(n, V->list_sel, V->list_first, vis);
    }
    if (pic) {
        uint32_t s2 = V->mask * 13u + (uint32_t)V->root + ux.gen * 977u + ux.pal * 31u + V->sel * 7u + 1u;
        if (ui.force || s2 != lst.pic) {
            lst.pic = s2;
            list_scale_pic(pic_y, PV_SOUND_Y - pic_y);
        }
    }
}
