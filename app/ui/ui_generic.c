/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The generic screen (the protocol's version 1: TITLE, INFO, BODY, ROW, LINE, CELL, POPUP), drawn in Melodee FM-1's
 * language: the header as its header (the transport, INFO where the BPM goes, TITLE as the chip, in its colour), K1..K4
 * as its strip of cards under it, a list as its rows (ui_list.c row_draw), the text as its words centred in the panel's
 * place, the cells as cards. The remote's own screens (waiting for Melodee, SETUP) are this screen too (app.c). A popup
 * is a card under the header (as Melodee FM-1's toast). Part of app.c */
static struct { uint32_t head, body, pop; } gen;

static int gen_has_foot(void)
{
    uint32_t i;
    for (i = 0; i < MEL_KNOBS; i++)
        if (S->cell[MEL_CELLS + i].name[0] || S->cell[MEL_CELLS + i].value[0])
            return 1;
    return 0;
}
static void gen_head(void)
{
    uint32_t sig = str_hash(str_hash(S->transport * 7u + ux.gen * 977u + ux.pal * 31u + T_THEME, S->title), S->info);
    int32_t x = BPM_X;
    const aafont_t *cf = TY_TX;
    if (!ui.force && sig == gen.head)
        return;
    gen.head = sig;
    cv_begin(240, H_HEAD, T_BG);
    cv_icon_mid(5, H_HEAD / 2, 16, (S->transport & MEL_T_PLAYING) ? ICON_X_PLAY : ICON_X_STOP,
                (S->transport & MEL_T_PLAYING) ? T_TEXT : T_MID, T_BG);
    if (text_w(cf, S->title) > 120)
        cf = TY_LB;
    if (S->info[0])
        x = cv_free_text(x, ty_y(H_HEAD / 2, TY_TX), TY_TX, S->info, T_TEXT, T_BG, 236 - text_w(cf, S->title) - 12 - x) + 4;
    if (S->transport & (MEL_T_RECORDING | MEL_T_COUNTIN))
        cv_icon_mid(x + 2, H_HEAD / 2, 16, (S->transport & MEL_T_RECORDING) ? ICON_X_REC : ICON_X_REC_O, T_REC, T_BG);
    cv_text_r(236, ty_y(H_HEAD / 2, cf), cf, S->title, T_THEME, T_BG);
    cv_blit(0, Y_HEAD);
}
/* a cell as a card (draw_column's look, drawn whole): its label in its colour (or the knob's), its value, framed when
 * selected */
static void gen_card(int32_t x, int32_t y, uint32_t c, const mel_cell_t *k)
{
    uint16_t lc = (k->flags & MEL_F_DIM) ? T_DIM : k->colour ? k->colour : T_KNOB(c);
    uint16_t vc = (k->flags & MEL_F_DIM) ? T_DIM : (k->flags & MEL_F_SELECTED) ? lc : T_TEXT;
    const aafont_t *vf = text_w(TY_TX, k->value) > CARD_W ? TY_LB : TY_TX;
    char v[16];
    gen_words(v, k->value, sizeof v);
    if (k->flags & MEL_F_SELECTED)
        cv_rrect(x + CARD_W / 2 - 14, y + CARD_H - 4, 28, 3, 1, lc, T_BG);
    if (k->name[0])
        cv_text_fit(x + CARD_W / 2 - (text_w(TY_LB, k->name) < CARD_W - 2 ? text_w(TY_LB, k->name) : CARD_W - 2) / 2, y + 3,
                    TY_LB, k->name, lc, T_BG, CARD_W - 2);
    if (binary_value(v) >= 0)
        binary_badge(x + CARD_W / 2, y + 21, 22, v, T_BG);
    else if (v[0])
        cv_free_text(x + CARD_W / 2 - (text_w(vf, v) < CARD_W ? text_w(vf, v) : CARD_W) / 2, y + ROLL_Y + TY_TX->asc - vf->asc,
                     vf, v, vc, T_BG, CARD_W);
}
/* a row (ui_list.c row_draw), with version 1's square of its colour before its text when it has one */
static void gen_row(int32_t y, const mel_row_t *w, uint32_t k)
{
    uint16_t hc = w->colour ? w->colour : T_KNOB(k), bg = (w->flags & MEL_F_SELECTED) ? ux_mix(T_BG, hc, 18) : T_BG;
    int32_t cy = y + LIST_RH / 2, x = 12, vx = 228;
    const aafont_t *tf = TY_TX;
    char v[MEL_VALUE_N + 1];
    int dim = (w->flags & MEL_F_DIM) != 0u, sel = (w->flags & MEL_F_SELECTED) != 0u;
    gen_words(v, w->value, sizeof v);
    if (sel)
        cv_rrect(BOX_X, y, BOX_W, LIST_RH, 8, bg, T_BG);
    if (w->flags & MEL_F_MARK) {
        cv_rrect(12, cy - 6, 12, 12, 3, dim ? T_DIM : hc, bg);
        x = 32;
    }
    if (v[0] && binary_value(v) >= 0) {
        int32_t vw = text_w(TY_LB, v) + 12;
        binary_badge(228 - vw / 2, cy - 11, 22, v, bg);
        vx = 228 - vw;
    } else if (v[0]) {
        const aafont_t *vf = text_w(tf, w->text) + text_w(tf, v) > 228 - x - 8 ? TY_LB : tf;
        vx = 228 - (text_w(vf, v) < 120 ? text_w(vf, v) : 120);
        cv_free_text(vx, ty_y(cy, vf), vf, v, dim ? T_DIM : (sel || (w->flags & MEL_F_ON)) ? hc : ux_mix(T_BG, hc, 80), bg, 120);
    }
    tf = ty_fit(tf, w->text, vx - 8 - x);
    cv_free_text(x, ty_y(cy, tf), tf, w->text, dim ? T_DIM : sel ? T_TEXT : T_SEC, bg, vx - 8 - x);
}
static void gen_body(void)
{
    uint32_t foot = gen_has_foot(), i, sig;
    int32_t y0 = foot ? Y_GRAPH : Y_LABEL, h = 240 - y0;
    sig = mem_hash(mem_hash(mem_hash(S->body * 7u + foot * 3u + ux.gen * 977u + ux.pal * 31u + T_THEME, S->row, sizeof S->row),
                            S->line, sizeof S->line), S->cell, sizeof S->cell[0] * MEL_CELLS);
    if (!ui.force && sig == gen.body)
        return;
    gen.body = sig;
    if (S->body == MEL_BODY_LIST) {                     /* the rows: as many as fit around the one selected */
        uint32_t vis = (uint32_t)(h / LIST_ROW), first = 0, sel = 0;
        for (i = 0; i < MEL_ROWS; i++)
            if (S->row[i].flags & MEL_F_SELECTED)
                sel = i;
        if (sel >= vis)
            first = sel + 1u - vis;
        for (uint32_t band = 0; band < (uint32_t)h; band += CV_MAX / 240u) {
            cv_begin_band(240, (uint32_t)h, T_BG, band);
            for (i = first; i < MEL_ROWS && i < first + vis; i++)
                if (S->row[i].text[0] || S->row[i].value[0])
                    gen_row((int32_t)(i - first) * LIST_ROW, &S->row[i], i);
            cv_blit_band(0, (uint32_t)y0);
        }
    } else if (S->body == MEL_BODY_CELLS) {             /* two rows of four cards */
        for (uint32_t band = 0; band < (uint32_t)h; band += CV_MAX / 240u) {
            cv_begin_band(240, (uint32_t)h, T_BG, band);
            for (i = 0; i < MEL_CELLS; i++)
                if (S->cell[i].name[0] || S->cell[i].value[0])
                    gen_card(CARD_X(i % 4u), 8 + (int32_t)(i / 4u) * (CARD_H + 12), i % 4u, &S->cell[i]);
            cv_blit_band(0, (uint32_t)y0);
        }
    } else {                                            /* the words: big, then in the track's colour, then small */
        static const uint8_t ROLE[MEL_LINES] = {2, 1, 0};   /* (TY_VL, TY_TX, TY_LB) */
        const aafont_t *F[3] = {TY_LB, TY_TX, TY_VL};
        int32_t th = 0, y;
        for (i = 0; i < MEL_LINES; i++)
            if (S->line[i][0])
                th += (int32_t)F[ROLE[i]]->h + 4;
        y = (h - th) / 2;
        for (uint32_t band = 0; band < (uint32_t)h; band += CV_MAX / 240u) {
            int32_t yy = y;
            cv_begin_band(240, (uint32_t)h, T_BG, band);
            for (i = 0; i < MEL_LINES; i++) {
                const aafont_t *f = F[ROLE[i]];
                if (!S->line[i][0])
                    continue;
                if (text_w(f, S->line[i]) > 228)
                    f = ty_fit(f == TY_VL ? TY_TX : f, S->line[i], 228);
                cv_free_text(st_mid_x(120, f, S->line[i], 228), yy, f, S->line[i], i == 0 ? T_TEXT : i == 1 ? T_THEME : T_MID,
                             T_BG, 228);
                yy += (int32_t)F[ROLE[i]]->h + 4;
            }
            cv_blit_band(0, (uint32_t)y0);
        }
    }
}
/* K1..K4: the strip of cards under the header (draw_column, the knobs' colours) */
static void gen_foot(void)
{
    uint32_t c;
    if (!gen_has_foot())
        return;
    for (c = 0; c < MEL_KNOBS; c++) {
        const mel_cell_t *k = &S->cell[MEL_CELLS + c];
        card_tc[c] = k->colour;
        draw_column(c, k->name, k->value, "", (k->flags & MEL_F_DIM) ? T_DIM : (k->flags & MEL_F_SELECTED) ? T_ACCENT : T_TEXT);
    }
    memset(card_tc, 0, sizeof card_tc);
}
/* version 1's popup: its two lines on a card under the header (Melodee FM-1's toast) */
static void gen_popup(int up)
{
    uint32_t sig = up ? str_hash(str_hash(ux.gen * 977u + 1u, S->popup[0]), S->popup[1]) : 0u;
    int32_t h = 58;
    if (!ui.force && sig == gen.pop)
        return;
    gen.pop = sig;
    if (!up)
        return;
    for (uint32_t band = 0; band < (uint32_t)h; band += CV_MAX / BOX_W) {
        cv_begin_band(BOX_W, (uint32_t)h, T_BG, band);
        cv_rrect(0, 0, BOX_W, h, 8, T_LINE, T_BG);
        cv_rrect(1, 1, BOX_W - 2, h - 2, 7, T_SURF, T_LINE);
        cv_free_text(st_mid_x(BOX_W / 2, TY_TX, S->popup[0], BOX_W - 20), 5, TY_TX, S->popup[0], T_TEXT, T_SURF, BOX_W - 20);
        cv_free_text(st_mid_x(BOX_W / 2, TY_LB, S->popup[1], BOX_W - 20), 31, TY_LB, S->popup[1], T_THEME, T_SURF, BOX_W - 20);
        GFX_HOOK_COVER(BOX_X, H_HEAD + 2, BOX_X + BOX_W, H_HEAD + 2 + h);
        cv_blit_band(BOX_X, H_HEAD + 2);
    }
}
static void generic_draw(int popup)
{
    if (ui.force)
        lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    gen_head();
    gen_foot();
    gen_body();
    gen_popup(popup);
}
