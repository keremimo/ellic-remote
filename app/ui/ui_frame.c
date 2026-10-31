/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* A frame of the remote's screen (Melodee FM-1's ui_draw.c ui_draw_frame and draw_page, for the pages the remote has):
 * the page Melodee shows (MEL_PAGE), or the generic screen (version 1, the remote's own); then a popup over it (a
 * question, a sheet: the page dimmed under it; a picker: the page as it is) or the toast. The page is drawn whole
 * (ui.force) when it changes, when a popup goes, when the selected track's colour (THEME) changes. Part of app.c */
static void ui_page_draw(uint32_t page, int popup)
{
    switch (page) {
    case MEL_PG_STAGE:
    case MEL_PG_WHEEL:
        if (ui.force)
            draw_frame(1);
        draw_head();
        if (page == MEL_PG_WHEEL)
            view_draw();
        else
            stage_draw();
        break;
    case MEL_PG_KNOBS:
        pv_draw();
        break;
    case MEL_PG_LIST:
        list_draw();
        break;
    case MEL_PG_SCALES:
        scales_draw();
        break;
    case MEL_PG_RACK:
        rack_draw();
        break;
    case MEL_PG_MIXER:
        mixer_draw();
        break;
    case MEL_PG_GLO:
        layer_draw();
        break;
    case MEL_PG_PATTERNS:
        patterns_draw();
        break;
    default:
        generic_draw(popup);
        break;
    }
}

/* the screen s (own: the remote's, drawn as the generic screen), popup: version 1's popup is up; say: the remote's own
 * message (app_say) or "" */
static void ui_draw_screen(const mel_screen_t *s, int own, int popup, const char *say)
{
    uint32_t page, overlay = 0, k;
    S = s;
    page = own ? MEL_PG_GENERIC : V->page;
    if (page == MEL_PG_GENERIC) {                       /* (THEME: version 1's colour, the title's) */
        if (palette_colour(0, S->colour) | palette_track(0))
            ui.force = 1;
    } else {
        for (k = 0; k < NTRK; k++)
            if (palette_colour(k, V->trk[k].colour))
                ui.force = 1;
        if (palette_track(V->sel))                      /* THEME is the selected track's colour */
            ui.force = 1;
        overlay = V->ask_on ? 2u : V->sheet_on && V->sheet_n ? 1u : V->pick_on && V->pick_n ? 3u : 0u;
    }
    if (page != ui.page || (uint8_t)own != ui.layer) {
        ui.page = (uint8_t)page;
        ui.layer = (uint8_t)own;
        ui.force = 1;
        ui.hot_t = 0;
        ui.toast = 0;
        stage.knob_ms = 0;
    }
    if (overlay != ui.popup) {                          /* (a popup goes: the page whole again; one comes: over it) */
        ui.force = 1;
        ui.popup = (uint8_t)overlay;
    }
    if (!str_eq(ui.msg, say))
        str_cpy(ui.msg, say, sizeof ui.msg);
    if (ui.toast && !head_toast(BPM_X + BPM_W + 6)) {   /* (a toast going: what it covered drawn again) */
        ui.toast = 0;
        ui.force = 1;
    }
    ui.frame++;
    gfx_frame(1);
    if (overlay == 1u || overlay == 2u) {               /* over the page, dimmed (each canvas darkened as it goes) */
        if (ui.force) {
            cv_dim = cv_under = 1;
            ui_page_draw(page, popup);
            cv_dim = cv_under = 0;
            ui.force = 1;
        }
        if (overlay == 2u) {
            ask_draw();
            draw_head();                                /* (the transport stays visible over a question) */
        } else {
            sheet_draw();
        }
    } else if (overlay == 3u) {                         /* a knob's picker: over the page as it is */
        if (ui.force) {
            cv_under = 1;
            ui_page_draw(page, popup);
            cv_under = 0;
            ui.force = 1;
        }
        pick_draw();
    } else {
        ui_page_draw(page, popup);
        if (page != MEL_PG_SCALES && page != MEL_PG_GENERIC)
            draw_toast();
    }
    gfx_frame(0);
    if (ui.hot_t)
        ui.hot_t--;
    ui.force = 0;
}

/* a knob K1..K4 turned (app.c): its card hot a moment; on Stage, the knobs drop in */
static void ui_knob_turned(uint32_t k)
{
    ui.hot_col = (uint8_t)(k & 3u);
    ui.hot_t = HOT_FRAMES;
    if (ui.page == MEL_PG_STAGE || ui.page == MEL_PG_WHEEL)
        stage_knob_touch();
}
static void ui_redraw(void)                              /* everything again (the panel's memory not to be trusted) */
{
    gfx_forget();
    ui.force = 1;
}
