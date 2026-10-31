/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's popups (its firmware/src/ui_popup.c and ui_draw.c's question), ported to Melodee Remote, over the page
 * dimmed (ui_frame.c: drawn under cv_dim).
 * An action sheet (SAVE's, a page's, a pattern's: MEL_SHEET, MEL_SROW): a card from the bottom, its title and rows; the
 * row picked tinted in the track's colour, a row that destroys something red, the value now a dot. Melodee moves it
 * (KNOB 2 the row, KNOB 1 a row's value, OCT+ does it, OCT- closes).
 * A question (MEL_ASK): a card in the middle, its words, No (OCT-) and Yes (OCT+, the track's colour).
 * A picker (MEL_PICK): a knob whose value is a name from a list turning: the names around its value in a card under its
 * cell, over the page as it is. Part of app.c */
#define POP_X BOX_X                                     /* a sheet (large type): 236 wide, its title row 34, its rows
                                                         * 32 px, 34 apart (ui_list.c row_draw), five at most shown */
#define POP_W BOX_W
#define POP_ROW 34
#define POP_VIS 5
#define POP_PICK_W 116                                  /* a picker: up to five names, 28 px each; two whole columns */
#define POP_FX_W 166                                    /* the effects' picker: "Pedal phaser" whole */
#define DLG_X 8                                         /* (large type: 224 wide, as tall as its words, centred) */
#define DLG_W 224

static int32_t pop_title_h(const char *title, const char *sub)
{
    int32_t sw = sub && sub[0] ? text_w(TY_LB, sub) + 12 : 0;
    return sw && text_w(TY_TX, title) > POP_W - 20 - sw ? POP_ROW + 20 : POP_ROW;
}
/* a card from the bottom (h rows tall under its title rows, slices of whole rows): its title, then rows drawn by
 * row(i, y) for first .. first + vis - 1 */
static void pop_card(int32_t h, const char *title, const char *sub, uint32_t first, uint32_t vis,
                     void (*row)(uint32_t i, int32_t y))
{
    int32_t th = pop_title_h(title, sub), y0, top;
    uint32_t i;
    h += th - POP_ROW;
    y0 = 238 - h;
    for (top = 0; top < h; ) {                          /* in slices between rows (a canvas holds 124 rows) */
        int32_t sh = h - top <= 124 ? h - top : th + (top + 124 - th) / POP_ROW * POP_ROW - top;
        for (uint32_t band = 0; band < (uint32_t)((uint32_t)sh); band += CV_MAX / (POP_W)) {
            cv_begin_band(POP_W,(uint32_t)sh,T_BG,band);
            cv_oy = -top;
            cv_rrect(0, 0, POP_W, h, 10, T_LINE, T_BG);
            cv_rrect(1, 1, POP_W - 2, h - 2, 9, T_SURF, T_LINE);
            if (top == 0) {
                int32_t sw = sub && sub[0] ? text_w(TY_LB, sub) + 12 : 0;
                const aafont_t *f = ty_fit(TY_TX, title, POP_W - 20 - (th > POP_ROW ? 0 : sw));
                cv_free_text(10, ty_y(POP_ROW / 2, f), f, title, T_TEXT, T_SURF, POP_W - 20 - (th > POP_ROW ? 0 : sw));
                if (sw && th > POP_ROW)
                    cv_text_on(10, ty_y(POP_ROW + 4, TY_LB), TY_LB, sub, T_MID, T_SURF);
                else if (sw)
                    cv_text_r(POP_W - 10, ty_y(POP_ROW / 2, TY_LB), TY_LB, sub, T_MID, T_SURF);
            }
            for (i = first; i < first + vis; i++) {
                int32_t y = th + (int32_t)(i - first) * POP_ROW;
                if (y + POP_ROW <= top || y >= top + sh)
                    continue;
                row(i, y);
            }
            cv_oy = 0;
            cv_blit_band(POP_X, (uint32_t)(y0 + top));
        }
        top += sh;
    }
}
/* a sheet's or a list's row on the card (row_draw's look, on the card's surface): picked tinted in the track's colour;
 * red: it would destroy something (it asks first); dot: the value now */
static void pop_row(int32_t y, const char *lb, const char *v, int on, int red, int dot)
{
    const aafont_t *tf = TY_TX;
    uint16_t bg = on ? ux_mix(T_SURF, T_THEME, 22) : T_SURF;
    int32_t cy = y + (POP_ROW - 2) / 2, vx = POP_W - 10;
    if (on)
        cv_rrect(4, y, POP_W - 8, POP_ROW - 2, 7, bg, T_SURF);
    if (dot)
        cv_disc(POP_W - 16, cy, 8, T_THEME, 0), vx = POP_W - 28;
    if (v && v[0])
        vx = cv_text_r(vx, ty_y(cy, tf), tf, v, on ? T_THEME : T_MID, bg) - 8;
    tf = ty_fit(tf, lb, vx - 12);
    cv_free_text(12, ty_y(cy, tf), tf, lb, red ? T_REC : on ? T_TEXT : T_SEC, bg, vx - 12);
}
static void sheet_row(uint32_t i, int32_t y)
{
    const mel_lrow_t *r = &V->srow[i % MEL_SHEET_ROWS];
    pop_row(y, r->label, r->value, i == V->sheet_sel, (r->flags & MEL_SF_RED) != 0u, (r->flags & MEL_SF_DOT) != 0u);
}
static void sheet_draw(void)
{
    uint32_t n = V->sheet_n, vis = n < POP_VIS ? n : POP_VIS, first, sig;
    sig = mem_hash(str_hash(str_hash(V->sheet_sel * 7u + n * 131u + ux.gen * 977u + ux.pal * 31u, V->sheet_title),
                            V->sheet_sub), V->srow, sizeof V->srow);
    if (!ui.force && sig == ui.pop_sig)
        return;
    ui.pop_sig = sig;
    first = (uint32_t)clamp((int32_t)V->sheet_sel - (int32_t)vis / 2, 0, (int32_t)(n - vis));
    pop_card(POP_ROW + (int32_t)vis * POP_ROW + 4, V->sheet_title, V->sheet_sub, first, vis, sheet_row);
}

/* the question (docs/design: mock/r5_pages, ref/dialog): a card over the page dimmed: its icon, the question (the text
 * face, centred, wrapped), what it is about (the label face), No (OCT-) and Yes (OCT+, the track's colour) */
static void ask_draw(void)
{
    const char *a = V->ask_q, *b = V->ask_d, *no = V->ask_no[0] ? V->ask_no : "No", *yes = V->ask_yes[0] ? V->ask_yes : "Yes";
    char q[TL_MAX][32], d[TL_MAX][32];
    const aafont_t *tf = TY_TX, *lf = TY_LB;
    uint32_t nq, nd, i, nc = 0, k, icon = V->ask_icon != MEL_AI_NONE, sig;
    int32_t top, h, y0, sh, cut[2 * TL_MAX + 2], ty = icon ? 34 : 14;   /* (ty: the words' first row) */
    sig = str_hash(str_hash(str_hash(str_hash(V->ask_icon * 7u + ux.gen * 977u + ux.pal * 31u + 5u, a), b), no), yes);
    if (!ui.force && sig == ui.pop_sig)
        return;
    ui.pop_sig = sig;
    nq = text_lines(a, q, 3u, DLG_W - 20, tf);
    nd = b[0] ? text_lines(b, d, 3u, DLG_W - 20, lf) : 0u;
    h = ty + (int32_t)nq * 24 + (nd ? 6 + (int32_t)nd * 20 : 0) + 14 + 34 + 12;
    y0 = H_HEAD + (240 - H_HEAD - h) / 2;
    for (i = 0; i < nq; i++)                            /* (where a slice may end: between lines, over the buttons) */
        cut[nc++] = ty + (int32_t)i * 24;
    for (i = 0; i < nd; i++)
        cut[nc++] = ty + (int32_t)nq * 24 + 6 + (int32_t)i * 20;
    cut[nc++] = h - 48;
    cut[nc++] = h;
    for (top = 0; top < h; top += sh) {                 /* (in slices: a canvas holds 132 rows of it) */
        int32_t y = ty;
        for (k = 0, sh = 0; k < nc; k++)
            if (cut[k] > top && cut[k] - top <= (int32_t)(CV_LOGICAL_MAX / DLG_W))
                sh = cut[k] - top;
        if (!sh)
            sh = h - top < (int32_t)(CV_LOGICAL_MAX / DLG_W) ? h - top : (int32_t)(CV_LOGICAL_MAX / DLG_W);
        for (uint32_t band = 0; band < (uint32_t)((uint32_t)sh); band += CV_MAX / (DLG_W)) {
            cv_begin_band(DLG_W,(uint32_t)sh,T_BG,band);y=ty;
            cv_oy = -top;
            cv_rrect(0, 0, DLG_W, h, 12, T_LINE, T_BG);
            cv_rrect(1, 1, DLG_W - 2, h - 2, 11, T_SURF, T_LINE);
            if (icon)
                cv_icon_mid(DLG_W / 2 - 8, 18, 16, V->ask_icon == MEL_AI_MOTION ? ICON_X_MOTION_DEL : ICON_X_WARN, T_THEME,
                            T_SURF);
            for (i = 0; i < nq; i++, y += 24)
                cv_text_c(DLG_W / 2, y, tf, q[i], T_TEXT, T_SURF);
            for (i = 0, y += 6; i < nd; i++, y += 20)
                cv_text_c(DLG_W / 2, y, lf, d[i], T_MID, T_SURF);
            {   /* OCT-: no, OCT+: yes (the track's colour) */
                int32_t by = h - 46, cy = by + 17;
                cv_rrect(12, by, 96, 34, 8, T_LINE, T_SURF);
                cv_rrect(13, by + 1, 94, 32, 7, T_PANEL, T_LINE);
                cv_icon_mid(60 - text_w(tf, no) / 2 - 12, cy, 16, ICON_X_UNDO, T_MID, T_PANEL);
                cv_text_on(60 - text_w(tf, no) / 2 + 8, ty_y(cy, tf), tf, no, T_MID, T_PANEL);
                cv_rrect(116, by, 96, 34, 8, T_THEME, T_SURF);
                cv_icon_mid(164 - text_w(tf, yes) / 2 - 12, cy, 16, ICON_X_CHECK, T_INK, T_THEME);
                cv_text_on(164 - text_w(tf, yes) / 2 + 8, ty_y(cy, tf), tf, yes, T_INK, T_THEME);
            }
            cv_oy = 0;
            cv_blit_band(DLG_X, (uint32_t)(y0 + top));
        }
    }
}

/* an effect family's colour (ui_draw.c fx_fam_col; Melodee iOS's families: Space, Motion, Dirt, Colour, then Notes or
 * Dynamics), from the palette: Space and Dirt knob shades, Motion, Colour and the last track colours */
static uint16_t fx_fam_col(uint32_t f)
{
    const ui_pal_t *p = &UI_PALETTES[ux.pal];
    return f == 1u ? p->knob[3][2] : f == 2u ? p->trk[3] : f == 3u ? p->knob[0][2] : f == 4u ? p->trk[1] :
           f == 5u ? p->trk[2] : T_DIM;
}
/* a knob's picker: the names around its value (up to five), the one now tinted; the effects': each its family's square,
 * a rule between families. Under the knob's cell (its column and the next; the last: the two last), whole */
static void pick_draw(void)
{
    int32_t x, i, rows = V->pick_n, h = 12 + rows * 28, now = V->pick_now;
    int fx = (V->pick_flags & MEL_PKF_FX) != 0u;
    int32_t pw = fx ? POP_FX_W : POP_PICK_W, tx = fx ? 26 : 10;
    uint32_t sig = mem_hash(mem_hash(V->pick_knob * 131u + V->pick_now * 7919u + V->pick_flags + ux.gen * 977u + ux.pal * 31u,
                                     V->pick, sizeof V->pick), V->pick_fam, sizeof V->pick_fam);
    if (!rows || (!ui.force && sig == ui.pop_sig))
        return;
    ui.pop_sig = sig;
    x = CARD_X(V->pick_knob < 3u ? V->pick_knob : 2u);
    if (fx && x + pw > 240)
        x = 240 - pw;
    for (uint32_t band = 0; band < (uint32_t)((uint32_t)h); band += CV_MAX / ((uint32_t)pw)) {
        cv_begin_band((uint32_t)pw,(uint32_t)h,T_BG,band);
        cv_rrect(0, 0, pw, h, 10, T_LINE, T_BG);
        cv_rrect(1, 1, pw - 2, h - 2, 9, T_SURF, T_LINE);
        for (i = 0; i < rows; i++) {
            int32_t y = 6 + i * 28, on = i == now, far = i - now > 1 || now - i > 1;
            uint16_t hc = fx ? fx_fam_col(V->pick_fam[i]) : T_THEME, bg = on ? ux_mix(T_SURF, hc, 22) : T_SURF;
            char b[16];
            menu_words(b, V->pick[i], sizeof b);
            if (on)
                cv_rrect(4, y, pw - 8, 26, 6, bg, T_SURF);
            if (fx) {
                if (i && V->pick_fam[i] != V->pick_fam[i - 1])
                    cv_rect(10, y - 2, pw - 20, 1, T_LIFT);
                cv_rrect(10, y + 9, 9, 9, 2, hc, bg);
            }
            cv_free_text(tx, ty_y(y + 13, ty_fit(TY_TX, b, pw - tx - 8)), ty_fit(TY_TX, b, pw - tx - 8), b,
                         on ? hc : far ? T_MID : T_SEC, bg, pw - tx - 8);
        }
        cv_blit_band((uint32_t)x, Y_LABEL + CARD_H + 2);
    }
}
