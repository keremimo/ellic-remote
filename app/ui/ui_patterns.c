/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's SEQ > PATTERNS (its firmware/src/ui_patterns.c), ported to Melodee Remote: the header, then a row per
 * track: its number in a chip (the selected one filled), a cell per pattern, eight at a time (two pages: their dots
 * over the numerals; a row's pattern on the other page: a tick on that side): the one playing filled in the track's
 * colour (how far through it at its foot: its playhead, MEL_PLAY), the one waiting for the bar dashed, the others with
 * something in them as three bars (their thirds' notes), empty ones an outline; Copy to's place framed. At the foot the
 * song as its sections' letters, the one playing (else the one picked) lit; no song yet: the jam's. All of it Melodee's
 * (MEL_GRID, MEL_GRIDTOP). Each part remembers what it drew. Part of app.c */
#define PT_NUM_Y H_HEAD                                 /* the patterns' numbers: rows 30 .. 49 */
#define PT_ROW_Y 50                                     /* a track a row: 38 px, to 201 */
#define PT_ROW_H 38
#define PT_SONG_Y (PT_ROW_Y + 4 * PT_ROW_H)             /* the song: 202 .. 239 */
#define PT_COLS 8u                                      /* a page of patterns (a part's 16: two) */
#define PT_CELL_X(b) (30 + 26 * (int32_t)((b) % PT_COLS))   /* a cell: 23 x 32 at row y 2 */
static struct { uint32_t num, row[NTRK], prog[NTRK], song; } ptv;

static uint32_t pt_playing_cell(const mel_track_t *t)  /* the cell playing on this page, PT_COLS none */
{
    uint32_t b;
    for (b = 0; b < PT_COLS; b++)
        if ((t->cell[b][0] & 3u) == MEL_GC_PLAYING)
            return b;
    return PT_COLS;
}
static uint32_t pt_prog(const mel_track_t *t)           /* px through the pattern playing (of 17), 0 stopped */
{
    uint32_t len = (uint32_t)t->len;
    return song_playing() && len && t->ph != MEL_PH_NONE ? (t->ph % len + 1u) * 17u / len : 0u;
}
static void pt_prog_bar(int32_t x, int32_t y, uint32_t prog, uint16_t tc)   /* at the playing cell's foot */
{
    uint16_t tr = ux_mix(tc, T_BG, 35);
    cv_rrect(x, y, 17, 3, 1, tr, tc);
    cv_rrect(x, y, prog < 2u ? 2 : (int32_t)prog, 3, 1, T_BG, tr);
}
/* a pattern's notes as three bars, its thirds (Melodee's heights 0..10: up to 14 px; none: a dot) */
static void pt_bars(const uint8_t *cell, int32_t x, int32_t y, uint16_t c)
{
    uint32_t i;
    for (i = 0; i < 3u; i++) {
        int32_t h = cell[1u + i] ? 4 + (int32_t)cell[1u + i] : 2;
        cv_rrect(x + 5 + 5 * (int32_t)i, y + 26 - h, 3, h, 1, c, T_SURF);
    }
}
/* the page's eight numbers (1 .. 8, 9 .. 16); two pages: their dots over the tracks' numerals, the page's a pill */
static void pt_numbers(uint32_t pg)
{
    uint32_t b, two = V->grid_pages > 1u, sig = ux.gen * 977u + ux.pal * 31u + 1u + 2u * two + 4u * pg + T_THEME * 3u;
    int32_t x;
    if (!ui.force && sig == ptv.num)
        return;
    ptv.num = sig;
    cv_begin(240, PT_ROW_Y - PT_NUM_Y, T_BG);
    for (b = pg * PT_COLS; b < pg * PT_COLS + PT_COLS; b++) {
        char n[3] = {(char)('0' + (b + 1u) / 10u), (char)('0' + (b + 1u) % 10u), 0};
        cv_text_c(PT_CELL_X(b) + 11, ty_y((PT_ROW_Y - PT_NUM_Y) / 2, TY_LB), TY_LB, b < 9u ? n + 1 : n, T_MID, T_BG);
    }
    for (b = 0, x = 4; two && b < 2u; b++) {           /* (as the header's, head_pages: over the numerals) */
        int32_t w = b == pg ? 12 : 6;
        cv_rrect(x, (PT_ROW_Y - PT_NUM_Y) / 2 - 3, w, 6, 3, b == pg ? T_THEME : T_LINE, T_BG);
        x += w + 2;
    }
    cv_blit(0, PT_NUM_Y);
}
static void pt_row(uint32_t k, uint32_t pg)
{
    const mel_track_t *t = TRK(k);
    uint16_t tc = (t->flags & MEL_TF_MUTED) ? T_DIM : T_TRK(k);
    uint32_t b, play = pt_playing_cell(t), prog = play < PT_COLS ? pt_prog(t) : 0u, sel = k == V->sel, h;
    int32_t y0 = PT_ROW_Y + (int32_t)(k % NPART) * PT_ROW_H;
    char n[3];
    numeral_text(n, k);
    h = mem_hash(2166136261u + t->grid_flags * 131u + sel * 8192u + tc * 16384u + ux.gen * 977u + ux.pal * 31u + 2u * pg +
                 (uint32_t)song_playing() + str_hash(7u, n), t->cell, sizeof t->cell);
    if (!ui.force && h == ptv.row[k]) {
        if (prog != ptv.prog[k] && play < PT_COLS) {    /* playing: the bar at the cell's foot only */
            ptv.prog[k] = prog;
            cv_begin(17, 3, tc);
            pt_prog_bar(0, 0, prog, tc);
            cv_blit((uint32_t)(PT_CELL_X(play) + 3), (uint32_t)(y0 + 2 + 25));
        }
        return;
    }
    ptv.row[k] = h;
    ptv.prog[k] = prog;
    cv_begin(240, PT_ROW_H, T_BG);
    if (sel) {                                          /* the track: its numeral in a chip, the selected one filled */
        cv_rrect(2, 2, 24, 32, 6, tc, T_BG);
        cv_text_c(14, ty_y(18, n[1] ? TY_LB : TY_TX), n[1] ? TY_LB : TY_TX, n, T_INK, tc);
    } else {
        cv_rrect(2, 2, 24, 32, 6, tc, T_BG);
        cv_rrect(4, 4, 20, 28, 4, T_BG, tc);
        cv_text_c(14, ty_y(18, n[1] ? TY_LB : TY_TX), n[1] ? TY_LB : TY_TX, n, tc, T_BG);
    }
    if (t->grid_flags & (MEL_GF_LEFT | MEL_GF_RIGHT))   /* its pattern on the other page: a tick on that side */
        cv_rrect((t->grid_flags & MEL_GF_LEFT) ? 27 : 236, 12, 2, 12, 1, tc, T_BG);
    for (b = 0; b < PT_COLS; b++) {
        const uint8_t *c = t->cell[b];
        uint32_t st = c[0] & 3u;
        int32_t x = PT_CELL_X(b), y = 2;
        int dst = (c[0] & MEL_GC_TARGET) != 0u;
        if (dst)                                        /* Copy to: the place, framed in the theme's colour */
            cv_rrect(x - 2, y - 2, 27, 36, 6, T_THEME, T_BG);
        if (st == MEL_GC_PLAYING) {                     /* playing: filled, how far through it */
            cv_rrect(x, y, 23, 32, 4, tc, dst ? T_THEME : T_BG);
            if (prog)
                pt_prog_bar(x + 3, y + 25, prog, tc);
            continue;
        }
        if (st == MEL_GC_USED || (c[0] & MEL_GC_QUEUED)) {
            cv_rrect(x, y, 23, 32, 4, T_SURF, dst ? T_THEME : T_BG);
            if (st == MEL_GC_USED)
                pt_bars(c, x, y, ux_mix(T_SURF, tc, 60));
        } else {                                        /* empty: an outline */
            cv_rrect(x, y, 23, 32, 4, dst ? T_BG : T_LINE, dst ? T_THEME : T_BG);
            cv_rrect(x + 1, y + 1, 21, 30, 3, T_BG, dst ? T_BG : T_LINE);
            if (dst)                                    /* (Copy to: an empty place) */
                cv_text_c(x + 11, ty_y(y + 16, TY_TX), TY_TX, "+", T_THEME, T_BG);
        }
        if (c[0] & MEL_GC_QUEUED)                       /* waiting for the bar: dashed */
            cv_dashed(x, y, 23, 32, 4, tc);
    }
    cv_blit(0, (uint32_t)y0);
}
/* the song's sections as letters (six around the one playing, else the one picked: lit); no song: the jam's */
static void pt_song(void)
{
    uint32_t n = V->song_n, cur = V->song_cur, jam = (V->grid_flags & MEL_GTF_JAM) != 0u, i, sig;
    sig = str_hash(cur * 7919u + jam * 3u + ux.gen * 977u + ux.pal * 31u, V->song);
    if (!ui.force && sig == ptv.song)
        return;
    ptv.song = sig;
    for (uint32_t band = 0; band < (uint32_t)(240 - PT_SONG_Y); band += CV_MAX / (240)) {
        cv_begin_band(240,240 - PT_SONG_Y,T_BG,band);
        cv_text_on(8, ty_y(19, TY_LB), TY_LB, jam ? "JAM" : "SONG", T_MID, T_BG);
        if (!n) {
            cv_text_on(62, ty_y(19, TY_LB), TY_LB, "Empty", T_DIM, T_BG);
        } else {
            for (i = 0; i < n && i < MEL_SONG_N; i++) {
                int32_t x = 62 + 29 * (int32_t)i;
                int on = i == cur;
                char l[2] = {V->song[i], 0};
                cv_rrect(x, 7, 26, 24, 6, on ? T_TEXT : T_SURF, T_BG);
                cv_text_c(x + 13, ty_y(19, TY_LB), TY_LB, l, on ? T_BG : T_MID, on ? T_TEXT : T_SURF);
            }
        }
        cv_blit_band(0, PT_SONG_Y);
    }
}
static void patterns_draw(void)
{
    uint32_t k, pg = V->grid_page, n = stage_rows();
    if (ui.force)
        lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    draw_head();
    pt_numbers(pg);
    pt_song();
    for (k = 0; k < n; k++)                             /* (the selected track's four) */
        if (!trk_free(k))
            pt_row(k, pg);
}
