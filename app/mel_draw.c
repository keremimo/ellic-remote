/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The screen: Melodee's (mel.melodee: a page of version 2, or version 1's generic screen) or the FM-1's own (mel.own:
 * waiting, SETUP: generic screens), drawn by Melodee FM-1's drawing (ui/ui_frame.c ui_draw_screen) as they change.
 * Then the frame, the screen reader and the simulator's checks. Part of app.c. */

static void text_set(char *d, const char *s, uint32_t max)
{
    uint32_t i;
    for (i = 0; i < max && s && s[i]; i++)
        d[i] = s[i];
    d[i] = 0;
}

/* --------------------------------------------------- the FM-1's own screens --- */
static void own_cell(mel_screen_t *s, int k, const char *name, const char *value, int32_t pos, uint32_t flags)
{
    mel_cell_t *c = &s->cell[MEL_CELLS + k];
    text_set(c->name, name, MEL_NAME_N);
    text_set(c->value, value, MEL_VALUE_N);
    c->pos = (uint8_t)clamp(pos, 0, 127);
    c->flags = (uint8_t)flags;
    c->colour = 0;
}

static void signed_text(int32_t v, char *t)   /* +1, 0, -2 */
{
    if (v > 0)
        *t++ = '+';
    else if (v < 0) {
        *t++ = '-';
        v = -v;
    }
    if (v >= 10)
        *t++ = (char)('0' + v / 10);
    *t++ = (char)('0' + v % 10);
    *t = 0;
}

static void own_row(mel_screen_t *s, int r, const char *text, const char *value)
{
    text_set(s->row[r].text, text, MEL_ROW_N);
    text_set(s->row[r].value, value, MEL_VALUE_N);
    s->row[r].flags = (uint8_t)(r == mel.setup_row ? MEL_F_SELECTED : 0u);
    s->row[r].colour = 0;
}

static const char *ble_words(void)       /* the waiting screen's last line */
{
    int st = plat_ble_state();
    if (!FM1B_BLE)
        return "CONNECT OVER USB";
    if (set.ble_off)
        return "BLUETOOTH OFF (SETUP)";
    if (plat_ble_connected())
        return "LINKED: OPEN MELODEE";
    if (st == BLE_FAILED)
        return "BLUETOOTH FAILED";
    if (st != BLE_READY)
        return "BLUETOOTH STARTING";
    return "PAIR IN MELODEE'S MIDI";
}

/* what the FM-1 shows of its own now, into t */
static void own_fill(mel_screen_t *t)
{
    char ch[4], oct[4], vel[4];
    uint32_t c = set.keys_ch + 1u;
    ch[0] = (char)(c >= 10u ? '1' : '0' + c);
    ch[1] = c >= 10u ? (char)('0' + c % 10u) : 0;
    ch[2] = 0;
    signed_text(mel.octave, oct);
    signed_text(set.curve, vel);
    if (mel.setup) {
        text_set(t->title, "SETUP", MEL_TITLE_N);
        text_set(t->info, "HOME: DONE", MEL_INFO_N);
        t->body = MEL_BODY_LIST;
        own_row(t, ROW_CH, "KEYS CHANNEL", ch);
        own_row(t, ROW_VEL, "VELOCITY CURVE", vel);
        own_row(t, ROW_PRS, "PRESSURE", set.pressure ? "ON" : "OFF");
        own_row(t, ROW_BLE, "BLUETOOTH", set.ble_off ? "OFF" : "ON");
        own_row(t, ROW_THEME, "THEME", UI_PALETTES[palette_from_stored(set.theme)].name);
        own_row(t, ROW_VERSION, "VERSION", APP_VERSION);
        own_cell(t, 0, "CH", ch, (int32_t)set.keys_ch * 127 / 15, mel.setup_row == ROW_CH ? MEL_F_SELECTED : 0u);
        own_cell(t, 1, "VEL", vel, (set.curve + 5) * 127 / 10, mel.setup_row == ROW_VEL ? MEL_F_SELECTED : 0u);
        own_cell(t, 2, "PRS", set.pressure ? "ON" : "OFF", 0, MEL_F_NOBAR);
        own_cell(t, 3, "BLE", set.ble_off ? "OFF" : "ON", 0, MEL_F_NOBAR);
    } else {
        text_set(t->title, "MELODEE REMOTE", MEL_TITLE_N);
        text_set(t->info, plat_ble_connected() ? "BLE" : plat_midi_out_room(TO_USB) ? "USB" : "", MEL_INFO_N);
        t->body = MEL_BODY_TEXT;
        text_set(t->line[0], "WAITING", MEL_LINE_N);
        text_set(t->line[1], "FOR MELODEE", MEL_LINE_N);
        text_set(t->line[2], ble_words(), MEL_LINE_N);
        own_cell(t, 0, "CH", ch, 0, MEL_F_NOBAR | MEL_F_DIM);
        own_cell(t, 1, "OCT", oct, 0, MEL_F_NOBAR | (mel.octave ? 0u : MEL_F_DIM));
        own_cell(t, 2, "VEL", vel, 0, MEL_F_NOBAR | MEL_F_DIM);
        own_cell(t, 3, "SETUP", "OCT-+", 0, MEL_F_NOBAR | MEL_F_DIM);
    }
}

static int bytes_same(const void *a, const void *b, uint32_t n)
{
    const uint8_t *p = a, *q = b;
    while (n--)
        if (*p++ != *q++)
            return 0;
    return 1;
}

/* the own screen made again; what changed is marked to draw */
static void own_build(void)
{
    static mel_screen_t t;
    mel_screen_t *o = &mel.own;
    mel_reset(&t);
    t.dirty = 0;
    own_fill(&t);
    if (!bytes_same(t.title, o->title, sizeof t.title) || !bytes_same(t.info, o->info, sizeof t.info))
        t.dirty |= MEL_DIRTY_HEAD;
    if (t.body != o->body || !bytes_same(t.row, o->row, sizeof t.row) || !bytes_same(t.line, o->line, sizeof t.line))
        t.dirty |= MEL_DIRTY_BODY;
    if (!bytes_same(&t.cell[MEL_CELLS], &o->cell[MEL_CELLS], sizeof t.cell[0] * MEL_KNOBS))
        t.dirty |= MEL_DIRTY_FOOT;
    t.dirty |= o->dirty;
    *o = t;
}

/* ------------------------------------------------------------- the frame --- */
void app_say(const char *a, const char *b)
{
    char t[2 * MEL_POPUP_N + 4];
    uint32_t i = 0;
    text_set(mel.say[0], a, MEL_POPUP_N);
    text_set(mel.say[1], b, MEL_POPUP_N);
    mel.say_until = plat_ms() + SAY_MS;
    if (ann_on()) {                      /* and to a screen reader (announce.c, fm1-reader) */
        const char *p;
        for (p = mel.say[0]; *p; p++)
            t[i++] = *p;
        t[i++] = ' ';
        for (p = mel.say[1]; *p; p++)
            t[i++] = *p;
        t[i] = 0;
        ann_say(t, 0, ANN_NOW);
    }
}

/* a screen reader that has just started listening (or asks) hears the screen: its name, then its body */
static void speak(const mel_screen_t *s)
{
    char t[160];
    uint32_t n = 0, i;
    if (ann_on() && ann_where()) {
        const char *part[1 + MEL_LINES + MEL_ROWS * 2];
        uint32_t np = 0;
        part[np++] = s->title;
        if (s->body == MEL_BODY_LIST) {
            for (i = 0; i < MEL_ROWS; i++)
                if (s->row[i].flags & MEL_F_SELECTED) {
                    part[np++] = s->row[i].text;
                    part[np++] = s->row[i].value;
                }
        } else {
            for (i = 0; i < MEL_LINES; i++)
                part[np++] = s->line[i];
        }
        for (i = 0; i < np; i++) {
            const char *p = part[i];
            if (!*p)
                continue;
            if (n)
                t[n++] = '.', t[n++] = ' ';
            while (*p && n < sizeof t - 3u)
                t[n++] = *p++;
        }
        t[n] = 0;
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

void app_ui_init(void)
{
    touch_on(1);                         /* velocity and pressure: the keys' readings on */
    touch_curve(set.curve);
    palette_set(palette_from_stored(set.theme));
    lcd_fill(0, 0, 240, 240, T_BG);
    ui_redraw();
    mel.usb_link = plat_midi_out_link();
    mel.redraw = MEL_DIRTY_ALL;
    mel.shown_own = 1;
}

void app_redraw(void) { mel.redraw_all = 1; }

void app_frame(void)
{
    uint32_t now = plat_ms();
    char say[2 * MEL_POPUP_N + 2];
    int own;
    mel_screen_t *s;
    input();
    links();
    own_build();
    own = mel.setup || !mel.host;
    if (own != mel.shown_own) {
        mel.shown_own = (uint8_t)own;
        mel.redraw = MEL_DIRTY_ALL;
    }
    s = own ? &mel.own : &mel.melodee;
    if (mel.popup_until && now >= mel.popup_until) {
        mel.popup_until = 0;
        mel.melodee.popup_shown = 0;
    }
    if (mel.say_until && now >= mel.say_until)
        mel.say_until = 0;
    mel.popup_up = (uint8_t)(!own && mel.melodee.popup_shown);
    say[0] = 0;
    if (mel.say_until) {                 /* the platform's word: the header's message (a toast when long) */
        text_set(say, mel.say[0], MEL_POPUP_N);
        if (mel.say[1][0]) {
            uint32_t n = (uint32_t)str_len(say);
            say[n] = ' ';
            text_set(say + n + 1u, mel.say[1], MEL_POPUP_N);
        }
    }
    if (mel.redraw & MEL_DIRTY_ALL)
        ui.force = 1;
    if (mel.redraw_all) {                /* (the panel's memory as well: everything sent again) */
        mel.redraw_all = 0;
        ui_redraw();
    }
    ui_draw_screen(s, own, mel.popup_up, say);
    mel.melodee.dirty = 0;
    mel.melodee.v2.dirty = 0;
    mel.own.dirty = 0;
    mel.redraw = 0;
    lights();
    speak(s);
}

/* ------------------------------------------------------------- simulator --- */
#ifdef PLAT_HOST
static void run_ms(uint32_t ms);         /* (host/plat_host.c's, further on) */
static int host_fails;

int app_host_fails(void) { return host_fails; }

int app_expect(const char *what, int *got)
{
    const mel_screen_t *s = mel.shown_own ? &mel.own : &mel.melodee;
    int i;
    if (!strcmp(what, "host"))           /* 0 none, 1 USB, 2 Bluetooth */
        *got = mel.host;
    else if (!strcmp(what, "octave"))
        *got = mel.octave;
    else if (!strcmp(what, "setup"))
        *got = mel.setup;
    else if (!strcmp(what, "own"))       /* the FM-1's own screen shows */
        *got = mel.shown_own;
    else if (!strcmp(what, "body"))
        *got = s->body;
    else if (!strcmp(what, "keys_ch"))
        *got = (int)keys_ch();
    else if (!strcmp(what, "frames"))
        *got = (int)mel.frames_in;
    else if (!strcmp(what, "popup"))
        *got = mel.popup_up;
    else if (!strcmp(what, "selected")) {
        *got = -1;
        for (i = 0; i < MEL_ROWS; i++)
            if (s->row[i].flags & MEL_F_SELECTED)
                *got = i;
    } else if (!strcmp(what, "set_ch"))
        *got = set.keys_ch;
    else if (!strcmp(what, "set_curve"))
        *got = set.curve;
    else if (!strcmp(what, "set_pressure"))
        *got = set.pressure;
    else if (!strcmp(what, "set_ble_off"))
        *got = set.ble_off;
    else if (!strcmp(what, "set_dirty"))
        *got = mel.set_dirty;
    else if (!strcmp(what, "theme"))
        *got = (int)palette_from_stored(set.theme);
    else if (!strcmp(what, "btn_lit"))
        *got = (int)mel.melodee.btn_lit;
    else if (!strcmp(what, "keys_lit"))
        *got = (int)mel.melodee.keys_lit;
    else if (!strcmp(what, "page"))      /* version 2: the page shown (MEL_PG_*; the FM-1's own: 0) */
        *got = mel.shown_own ? 0 : mel.melodee.v2.page;
    else if (!strcmp(what, "gen"))       /* the screen's generation */
        *got = mel.gen;
    else if (!strcmp(what, "why"))
        *got = mel.why;
    else if (!strcmp(what, "hellos"))    /* HELLOs and STATEs the remote sent */
        *got = (int)mel.n_hello;
    else if (!strcmp(what, "states"))
        *got = (int)mel.n_state;
    else if (!strcmp(what, "sel"))
        *got = mel.melodee.v2.sel;
    else if (!strcmp(what, "qnt"))
        *got = mel.melodee.v2.q;
    else if (!strcmp(what, "sheet"))
        *got = mel.melodee.v2.sheet_on;
    else if (!strcmp(what, "knobs_out"))  /* Stage's knobs dropped in */
        *got = stage.out;
    else if (!strcmp(what, "hot"))       /* the card lit by its knob, -1 none */
        *got = ui.hot_t ? ui.hot_col : -1;
    else if (!strcmp(what, "theme_rgb")) /* THEME, RGB565 */
        *got = T_THEME;
    else
        return 0;
    return 1;
}

/* a script's bytes: hex, and "TEXT" (its length, then its characters) */
static uint32_t host_bytes(const char *line, uint8_t *out, uint32_t max)
{
    const char *p = line;
    uint32_t n = 0;
    while (*p && *p != ' ' && *p != '\t')   /* past the command */
        p++;
    while (*p && n < max) {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        } else if (*p == '"') {
            uint32_t at = n++, len = 0;
            for (p++; *p && *p != '"' && n < max; p++, len++)
                out[n++] = (uint8_t)*p;
            if (*p == '"')
                p++;
            out[at] = (uint8_t)len;
        } else {
            char *end;
            unsigned long v = strtoul(p, &end, 16);
            if (end == p)
                break;
            out[n++] = (uint8_t)v;
            p = end;
        }
    }
    return n;
}

static const char *host_text(const char *where)   /* text WHERE: a text on the screen shown */
{
    const mel_screen_t *s = mel.shown_own ? &mel.own : &mel.melodee;
    int i = atoi(where + strcspn(where, "0123456789"));
    if (!strcmp(where, "title"))
        return s->title;
    if (!strcmp(where, "info"))
        return s->info;
    if (!strcmp(where, "say0") || !strcmp(where, "say1"))
        return mel.say_until ? mel.say[where[3] - '0'] : "";
    if (!strncmp(where, "popup", 5) && i < 2)
        return mel.popup_up && !mel.shown_own ? s->popup[i] : "";
    if (!strncmp(where, "line", 4) && i < MEL_LINES)
        return s->line[i];
    if (!strncmp(where, "rowv", 4) && i < MEL_ROWS)
        return s->row[i].value;
    if (!strncmp(where, "row", 3) && i < MEL_ROWS)
        return s->row[i].text;
    if (!strncmp(where, "cellv", 5) && i < MEL_CELLS + MEL_KNOBS)
        return s->cell[i].value;
    if (!strncmp(where, "cell", 4) && i < MEL_CELLS + MEL_KNOBS)
        return s->cell[i].name;
    /* version 2's: the header, the tracks, the cards, a list's and a sheet's rows, the question */
    if (!strcmp(where, "chip"))
        return s->v2.chip;
    if (!strcmp(where, "status"))
        return s->v2.status;
    if (!strcmp(where, "bpm"))
        return s->v2.bpm;
    if (!strcmp(where, "scale"))
        return s->v2.scale;
    if (!strncmp(where, "trk", 3) && i < MEL_PLACES)
        return s->v2.trk[i].name;
    if (!strncmp(where, "kval", 4) && i < 4)
        return s->v2.card[i].value;
    if (!strncmp(where, "knob", 4) && i < 4)
        return s->v2.card[i].label;
    if (!strncmp(where, "lrow", 4) && i < MEL_LIST_ROWS)
        return s->v2.lrow[i].label;
    if (!strncmp(where, "srow", 4) && i < MEL_SHEET_ROWS)
        return s->v2.srow[i].label;
    if (!strcmp(where, "ask"))
        return s->v2.ask_on ? s->v2.ask_q : "";
    if (!strcmp(where, "foot"))
        return s->v2.foot_text;
    return 0;
}

int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    static uint8_t buf[700];
    uint32_t n, i;
    (void)b, (void)c, (void)d;
    if (!strcmp(cmd, "sysex")) {         /* sysex F0 7D 4D 52 ... F7: from USB */
        n = host_bytes(plat_host_line, buf, sizeof buf);
        if (n >= 2u && buf[0] == 0xF0u && buf[n - 1] == 0xF7u)
            app_sysex(buf + 1, n - 2);
        else {
            printf("FAIL sysex: not a frame\n");
            host_fails++;
        }
        run_ms(2);
        return 1;
    }
    if (!strcmp(cmd, "blesysex")) {      /* blesysex F0 ... F7: from Bluetooth, a byte at a time */
        n = host_bytes(plat_host_line, buf, sizeof buf);
        for (i = 0; i < n; i++)
            mel_ble_sysex(buf[i]);
        run_ms(2);
        return 1;
    }
    if (!strcmp(cmd, "quiet")) {         /* quiet: no MIDI out since the last "out" (clock aside) */
        for (i = 0; i < n_out; i++)
            if (((out_pkt[i] >> 8) & 0xFFu) != 0xF8u) {
                printf("FAIL quiet: %02X %02X %02X went out\n", (out_pkt[i] >> 8) & 0xFFu, (out_pkt[i] >> 16) & 0xFFu,
                       out_pkt[i] >> 24);
                host_fails++;
                return 1;
            }
        printf("  ok quiet\n");
        return 1;
    }
    if (!strcmp(cmd, "text")) {          /* text WHERE WORDS: a text shown is WORDS (the rest of the line; none: empty) */
        const char *want = plat_host_line, *have = host_text(a);
        char w[64];
        uint32_t k = 0;
        want += strspn(want, " \t");
        want += strcspn(want, " \t");    /* past "text" */
        want += strspn(want, " \t");
        want += strcspn(want, " \t");    /* past WHERE */
        want += strspn(want, " \t");
        while (want[k] && want[k] != '\n' && want[k] != '\r' && k < sizeof w - 1u) {
            w[k] = want[k];
            k++;
        }
        w[k] = 0;
        if (!have || strcmp(have, w)) {
            printf("FAIL text %s: \"%s\", not \"%s\"\n", a, have ? have : "(no such text)", w);
            host_fails++;
        } else {
            printf("  ok text %s \"%s\"\n", a, w);
        }
        return 1;
    }
    return 0;
}
#endif
