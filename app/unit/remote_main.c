/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's start and main loop (its main.c's melodee_init and fm1_main) on the remote, as app.c calls them:
 * fm1-base owns the hardware, the boot and the loop; the unit runs a pass at a time. */

/* power-on: the parts with their default sounds; the sequencers empty (main.c melodee_init) */
static void melodee_init(void)
{
    uint32_t i;
    trk_voices_bind();
    midi_insert_init();
    punch_reset();
    chain_defaults(&chain_config);
    pattern_init();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    undo_depth++;                             /* (no undo copy of the power-on loads) */
    fm6_init();
    cz_init();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, trk_def_engine(i));
        apply_preset_to(t, trk_def_preset(i));
        t->engine = t->eng_req;
        track_defaults_steps(t);
        pat_sig[i] = steps_sig(t);
        pat_last[i] = 0;
    }
    undo_depth--;
    song.sel = 0;
    song.master_q12 = 2048;
    ui.home = 1;
    ui.force = 1;
}

void fm1u_start(void)
{
    fm1_ms = plat_ms();
    remote_engines_init();
    settings_init();
    panel_init();
    panel = PANEL_DEFAULT;                    /* (the FM-1's own calibration is fm1-base's: labels both ways) */
    hal_btn = panel.btn;
    hal_enc = (const uint8_t *)panel.enc;
    hal_dir = panel.dir;
    takes_none();
    melodee_init();
    link_start();
    lcd_fill(0, 0, 240, 240, T_BG);
}

/* a pass: the panel read, the LEDs, the screen, at ~60 frames a second; between frames the panel alone
 * (main.c fm1_main's loop) */
void fm1u_frame(void)
{
    static uint32_t frame_ms;
    fm1_ms = plat_ms();
    hal_poll();
    if (told_sync())                          /* (the selected track's told pages: remote_told.c) */
        ui.force = 1;
    keys_block();
    ui_input();
    model_poll();
    if (fm1_ms - frame_ms < 15u)
        return;
    frame_ms = fm1_ms;
    {                                         /* MASTER: the pot, then MIDI's attenuation (main.c) */
        uint32_t k10 = plat_master() >> 2;
        midi_master_physical((k10 * k10) >> 8);
    }
    pattern_poll();
    melodee_dbg.ui_frames++;
    link_poll();
    ui_leds();
    hal_lights();
    ui_draw();
}

void fm1u_redraw(void)
{
    ui.force = 1;
}

void fm1u_midi(uint32_t pkt, int port) { (void)pkt; (void)port; }   /* (Ellic Studio says its state as SysEx: link.c) */

int fm1u_sysex(const uint8_t *p, uint32_t n, int port)
{
    return link_sysex(p, n, port);
}

void fm1u_lost(int port)
{
    link_lost(port);
}

#ifdef PLAT_HOST
/* ------------------------------------------------------------ simulator --- */
static int host_fails;
int fm1u_expect(const char *what, int *got)
{
    int v;
    if (!strcmp(what, "home")) v = ui.home;
    else if (!strcmp(what, "home_pg")) v = ui.home_pg;
    else if (!strcmp(what, "page")) v = ui.page;
    else if (!strcmp(what, "menu")) v = ui.menu;
    else if (!strcmp(what, "pop")) v = pop.on;
    else if (!strcmp(what, "confirm")) v = ui.confirm;
    else if (!strcmp(what, "layer")) v = ui.layer;
    else if (!strcmp(what, "sel")) v = song.sel;
    else if (!strcmp(what, "playing")) v = song.playing;
    else if (!strcmp(what, "rec")) v = song.rec;
    else if (!strcmp(what, "bpm")) v = song.g[G_BPM];
    else if (!strcmp(what, "octave")) v = song.octave;
    else if (!strcmp(what, "tracks")) v = (int)trk_n();
    else if (!strcmp(what, "list_row")) v = lst.row;
    else if (!strcmp(what, "list_first")) v = lst.first;
    else if (!strcmp(what, "host")) v = lk.host;
    else if (!strcmp(what, "frames_in")) v = (int)lk.frames_in;
    else if (!strcmp(what, "frames_out")) v = (int)lk.frames_out;
    else if (!strcmp(what, "lost")) v = (int)lk.lost;
    else if (!strcmp(what, "gen")) v = lk.gen;
    else if (!strcmp(what, "last_req")) v = lk.last_req;
    else if (!strcmp(what, "last_cmd")) v = lk.last_cmd;
    else if (!strcmp(what, "instruments")) v = (int)remote_eng_n;
    else if (!strcmp(what, "pattern")) v = trk[song.sel].pattern;
    else if (!strncmp(what, "key", 3) && what[3] >= '0' && what[3] <= '9')   /* keyN: what key N's light shows (bit 0
                                                                            * lit, 1 the LIGHTS plane, 2 the buttons') */
        v = hal_shown[14u + (uint32_t)strtoul(what + 3, 0, 10) % 27u];
    else if (!strcmp(what, "lights_mask")) v = fm1_led_dim_mask[0];
    else if (!strcmp(what, "told_n")) v = told_on() ? told.n : -1;
    else if (!strncmp(what, "told", 4) && what[4] >= '0' && what[4] <= '9')   /* toldK: told value K here */
        v = told.val[strtoul(what + 4, 0, 10) % TOLD_K];
    else if (!strcmp(what, "keys")) v = told.keys;
    else if (!strcmp(what, "lights")) v = settings_lights;
    else if (!strcmp(what, "act")) v = ui.act;
    else return 0;
    *got = v;
    return 1;
}
/* text page TITLE: the page shown (HOME on HOME) */
int fm1u_host_cmd(const char *cmd, const char *a, const char *b)
{
    const char *got;
    if (strcmp(cmd, "text"))
        return 0;
    if (!strcmp(a, "page"))
        got = ui.home ? "HOME" : cur_page()->title;
    else if (!strncmp(a, "name", 4))                   /* nameK: track K's name as told */
        got = trk_name[strtoul(a + 4, 0, 10) % NTRK];
    else if (!strcmp(a, "toast"))                      /* what the top bar says now */
        got = ui.msg;
    else if (!strcmp(a, "toast_title"))                /* (a sheet's title) */
        got = pop.title;
    else
        return 0;
    if (strcmp(got, b)) {
        printf("FAIL text %s: \"%s\", not \"%s\"\n", a, got, b);
        host_fails++;
    }
    return 1;
}
int fm1u_host_fails(void) { return host_fails; }
#endif
