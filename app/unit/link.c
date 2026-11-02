/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The link with Ellic Studio (docs/PROTOCOL.md, version 4): SysEx F0 7D 4D 52 cmd .. F7 over USB or Bluetooth MIDI.
 *   the session: the remote says HELLO until Ellic Studio claims it (its HELLO back), answers PING with STATE (its
 *     generation: a new one each time it loses what Ellic Studio told, so Ellic Studio tells it all again); BYE lets it go;
 *   the mirror, Ellic Studio's: its instruments, the tracks (their names) and their parameters, the selected track's sound
 *     pages (remote_told.c), the steps, the patterns, where the music is, the song's rows, the scope, the remote's
 *     preferences (remote_model.c, remote_sounds.c);
 *   from the remote: what the controls changed in place (a value, a step, the track selected, a sound chosen, a
 *     preference), found against the mirror as Ellic Studio last told it, what they asked (link_request) and the page shown
 *     (PAGE: Ellic Studio follows it, and the remote Ellic Studio's); the keys' notes go as MIDI.
 * Values: 14 bits as two 7-bit bytes, low first, offset by 8192 (-8192..8191); text ASCII, 0-terminated. */
#define L_PROTO 4u                                     /* (4: Ellic Studio's sound pages, names, PAGE, PREFS, TOAST) */
enum { L_HELLO = 0x01, L_BYE, L_STATE, L_PING, L_PAGE, L_PREFS, L_TOAST,
       L_INSTRUMENT = 0x40, L_DESC, L_SOUNDS, L_INSTRUMENTS, L_TRACKS, L_PARAM, L_GLOBAL, L_STEPS, L_PATTERNS,
       L_PLAY, L_SONG, L_SCOPE = 0x4C, L_PAGES, L_WAVE, L_MARKS, L_REQUEST = 0x50 };
enum { L_FREE, L_YOURS, L_TAKEN };                     /* STATE: whose the remote is, to the port asking */
enum { L_WHY_POWER, L_WHY_SILENT, L_WHY_TAKEN, L_WHY_LOST };   /* why the generation last changed */
#define L_SILENT_MS 7000u                              /* a copy of Ellic Studio that PINGs and is then silent this long: let go */
#define L_SEND_MAX 24u                                 /* messages a frame (Bluetooth MIDI's pace) */

static struct {
    uint8_t host;                                      /* where Ellic Studio is: 0 nowhere, else 1 + U_PORT_* */
    uint8_t pings;                                     /* it PINGs: its silence lets the remote go */
    uint16_t gen;                                      /* the mirror's generation */
    uint8_t why;
    uint32_t heard_ms;                                 /* Ellic Studio's last message */
    uint32_t link_ms[2];                               /* each port up since (| 1; 0 down): HELLO again while free */
    uint8_t hellos[2];
    uint8_t sent;                                      /* messages this frame */
    struct { uint8_t what; uint16_t a, b, c; } req[16];
    uint8_t req_w, req_r;
    /* the mirror as Ellic Studio last told it: what differs is the controls' */
    int16_t p[NTRK][P_COUNT], g[G_COUNT];
    step_t step[NTRK][NSTEP];
    uint8_t eng[NTRK], preset[NTRK], sel, ntrk;
    uint32_t frames_in, frames_out;                    /* (the simulator's checks) */
    uint32_t lost;                                     /* frames cut short on the way (bytes lost) */
    uint8_t last_req, last_cmd;
} lk __attribute__((section(".pool")));

static int32_t rd14(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 7) - 8192; }
static uint32_t wr14(uint8_t *p, int32_t v)
{
    uint32_t u = (uint32_t)(clamp(v, -8192, 8191) + 8192);
    p[0] = (uint8_t)(u & 0x7Fu);
    p[1] = (uint8_t)(u >> 7);
    return 2;
}
static uint32_t wr_text(uint8_t *p, const char *s, uint32_t max)
{
    uint32_t n = 0;
    while (s && s[n] && n < max) {
        p[n] = (uint8_t)(s[n] >= 0x20 && s[n] < 0x7F ? s[n] : '?');
        n++;
    }
    p[n] = 0;
    return n + 1u;
}

/* a frame out: F0 7D 4D 52 cmd body F7, to the port given (-1: wherever Ellic Studio is) */
static void link_send(uint32_t cmd, const uint8_t *body, uint32_t n, int port)
{
    uint8_t f[300];
    uint32_t i;
    if (port < 0)
        port = lk.host ? (int)lk.host - 1 : -1;
    if (port < 0 || n + 6u > sizeof f)
        return;
    f[0] = 0xF0; f[1] = 0x7D; f[2] = 0x4D; f[3] = 0x52; f[4] = (uint8_t)cmd;
    for (i = 0; i < n; i++)
        f[5 + i] = (uint8_t)(body[i] & 0x7Fu);
    f[5 + n] = 0xF7;
    app_sysex_out(f, n + 6u, port);
    lk.sent++;
    lk.frames_out++;
    lk.last_cmd = (uint8_t)cmd;
}

static uint32_t link_state(uint32_t port) { return !lk.host ? L_FREE : lk.host == 1u + port ? L_YOURS : L_TAKEN; }
static int link_held(void) { return lk.host != 0; }
static void link_hello(uint32_t port)                  /* version, generation, state, the tracks it holds */
{
    uint8_t b[5] = {L_PROTO, (uint8_t)(lk.gen & 0x7Fu), (uint8_t)(lk.gen >> 7 & 0x7Fu), 0, NTRK};
    b[3] = (uint8_t)link_state(port);
    link_send(L_HELLO, b, 5, (int)port);
}
static void link_state_to(uint32_t port)
{
    uint8_t b[5] = {(uint8_t)(lk.gen & 0x7Fu), (uint8_t)(lk.gen >> 7 & 0x7Fu), (uint8_t)link_state(port), lk.why, NTRK};
    link_send(L_STATE, b, 5, (int)port);
}
/* a bit per track, 35 in 5 bytes of 7 bits, low first */
static uint32_t rd_bits(const uint8_t *p)
{
    uint32_t v = 0, i;
    for (i = 0; i < 5u; i++)
        v |= (uint32_t)(p[i] & 0x7Fu) << (7u * i);
    return v;
}
static void link_new_gen(uint32_t why)
{
    lk.gen = (uint16_t)((lk.gen + 1u) & 0x3FFFu);
    lk.why = (uint8_t)why;
}

/* the mirror's view now is Ellic Studio's: nothing for the controls to tell */
static void link_mark(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        memcpy(lk.p[k], trk[k].p, sizeof lk.p[k]);
        memcpy(lk.step[k], trk[k].step, sizeof lk.step[k]);
        lk.eng[k] = trk[k].eng_req;
        lk.preset[k] = trk[k].preset;
    }
    memcpy(lk.g, song.g, sizeof lk.g);
    lk.sel = song.sel;
    lk.ntrk = song.ntrk;
}

/* a frame cut short on a port (bytes lost on the way): from the copy of Ellic Studio that has the remote, a new generation, so the
 * next STATE (its PING's answer) has it tell everything again */
static void link_lost(int port)
{
    lk.lost++;
    if (lk.host == 1u + (uint32_t)port)
        link_new_gen(L_WHY_LOST);
}

static void link_let_go(void)
{
    lk.host = 0;
    lk.pings = 0;
    lk.hellos[0] = lk.hellos[1] = 0;
    lk.link_ms[0] = lk.link_ms[0] ? plat_ms() | 1u : 0u;
    lk.link_ms[1] = lk.link_ms[1] ? plat_ms() | 1u : 0u;
    ui_say("ELLIC ", "LET GO");
}

static void link_request(uint32_t what, uint32_t a, uint32_t b, uint32_t c)
{
    uint8_t w = lk.req_w;
    lk.last_req = (uint8_t)what;
    if (!lk.host) {                                    /* (nobody to ask: said, and forgotten) */
        ui_say("ELLIC ", "NOT HERE");
        return;
    }
    if ((uint8_t)(w - lk.req_r) >= 16u)
        return;
    lk.req[w % 16u].what = (uint8_t)what;
    lk.req[w % 16u].a = (uint16_t)a;
    lk.req[w % 16u].b = (uint16_t)b;
    lk.req[w % 16u].c = (uint16_t)c;
    lk.req_w = (uint8_t)(w + 1u);
}


/* ------------------------------------------------------------ steps --- */
/* a step: its 11 bytes in 13 7-bit ones (the high bits first) */
static void step_put(uint8_t *b, const step_t *s)
{
    const uint8_t raw[11] = {s->note[0], s->note[1], s->note[2], s->note[3], s->n, s->time, s->flags, s->vel,
                             s->hit, s->acc, s->probability};
    uint32_t i;
    b[0] = b[1] = 0;
    for (i = 0; i < 11u; i++) {
        b[i < 7u ? 0 : 1] |= (uint8_t)((raw[i] >> 7) << (i < 7u ? i : i - 7u));
        b[2 + i] = raw[i] & 0x7Fu;
    }
}
static void step_get(step_t *s, const uint8_t *b)
{
    uint8_t raw[11];
    uint32_t i;
    for (i = 0; i < 11u; i++)
        raw[i] = (uint8_t)(b[2 + i] | ((b[i < 7u ? 0 : 1] >> (i < 7u ? i : i - 7u)) & 1u) << 7);
    memcpy(s->note, raw, 4);
    s->n = raw[4]; s->time = raw[5]; s->flags = raw[6]; s->vel = raw[7];
    s->hit = raw[8]; s->acc = raw[9]; s->probability = raw[10];
}

/* -------------------------------------------------------- from Ellic Studio --- */
static void link_claim(int port)
{
    if (lk.host && lk.host != 1u + (uint32_t)port)
        link_new_gen(L_WHY_TAKEN);                     /* (another copy of Ellic Studio had it: it hears it has gone) */
    lk.host = (uint8_t)(1u + (uint32_t)port);
    lk.heard_ms = plat_ms();
    told_reset(0xFFu);                                 /* (the sound pages: Ellic Studio tells the selected track's) */
    follow_claimed();
    link_state_to((uint32_t)port);
    ui_say("ELLIC ", "CONNECTED");
}

static int link_sysex(const uint8_t *p, uint32_t n, int port)
{
    const uint8_t *b = p + 5, *end;
    uint32_t cmd, k, i;
    if (n < 6u || p[0] != 0xF0 || p[1] != 0x7D || p[2] != 0x4D || p[3] != 0x52 || p[n - 1u] != 0xF7)
        return 0;
    cmd = p[4];
    end = p + n - 1u;                                  /* (the body: b .. end) */
    lk.frames_in++;
    if (cmd == L_HELLO) {                              /* Ellic Studio at this version claims; an older one hears ours */
        if (end > b && b[0] >= L_PROTO)
            link_claim(port);
        else
            link_hello((uint32_t)port);
        return 1;
    }
    if (cmd == L_PING) {
        if (lk.host == 1u + (uint32_t)port) {
            lk.pings = 1;
            lk.heard_ms = plat_ms();
        }
        link_state_to((uint32_t)port);
        return 1;
    }
    if (lk.host != 1u + (uint32_t)port)
        return 1;                                      /* (only the copy of Ellic Studio that has the remote tells it) */
    lk.heard_ms = plat_ms();
    switch (cmd) {
    case L_BYE:
        link_let_go();
        break;
    case L_INSTRUMENT:                                 /* n, sounds v14, name */
        if (end - b >= 4) {
            const char *nm = (const char *)b + 3;
            if ((const uint8_t *)nm + str_len(nm) < end)
                remote_instrument(b[0], nm, str_len(nm), (uint32_t)rd14(b + 1));
        }
        break;
    case L_DESC:                                       /* k, fmt, min v14, max v14, def v14, nn, label, unit, names:
                                                        * the selected track's told value k (remote_told.c) */
        if (end - b >= 9) {
            const char *label = (const char *)b + 9, *unit = label + str_len(label) + 1;
            const char *names = unit + str_len(unit) + 1, *q = names;
            for (i = 0; i < b[8] && (const uint8_t *)q < end; i++)
                q += str_len(q) + 1;
            if ((const uint8_t *)unit < end && (const uint8_t *)q <= end)
                told_desc(b[0], b[1], rd14(b + 2), rd14(b + 4), rd14(b + 6), label, unit, names, b[8]);
        }
        break;
    case L_PAGES:                                      /* the selected track's sound pages (remote_told.c) */
        told_pages(b, end);
        break;
    case L_WAVE:
        told_wave(b, end);
        break;
    case L_MARKS:
        told_marks(b, end);
        break;
    case L_PAGE:                                       /* screen, track, family, page: Ellic Studio's screen, the remote's */
        if (end - b >= 4)
            page_follow(b);
        break;
    case L_PREFS:                                      /* (id, value v14)...: what Ellic Studio keeps for the remote */
        for (; end - b >= 3; b += 3)
            pref_set(b[0], rd14(b + 1));
        break;
    case L_TOAST:                                      /* a line, a second line: Ellic Studio's word, shown a moment */
        if (b < end) {
            const char *a = (const char *)b, *c = a + str_len(a) + 1;
            char t[32];
            str_cpy(t, a, sizeof t);
            if ((const uint8_t *)c < end && c[0] && str_len(t) + 2u < sizeof t) {
                str_cpy(t + str_len(t), " ", 2);
                str_cpy(t + str_len(t), c, sizeof t - str_len(t));
            }
            ui_message(t);
        }
        break;
    case L_SOUNDS:                                     /* n, first v14, count, names */
        if (end - b >= 4) {
            const char *q = (const char *)b + 4;
            for (i = 0; i < b[3] && (const uint8_t *)q < end; i++)
                q += str_len(q) + 1;
            if ((const uint8_t *)q <= end)
                remote_sounds_named(b[0], (uint32_t)rd14(b + 1), b[3], (const char *)b + 4);
        }
        break;
    case L_INSTRUMENTS:                                /* count */
        if (end - b >= 1) {
            remote_instruments(b[0]);
            ui.force = 1;
        }
        break;
    case L_TRACKS:                                     /* count, sel, first, then from first: (instrument (127 none),
                                                        * sound v14, name) */
        if (end - b >= 3) {
            song.ntrk = (uint8_t)clamp(b[0], 1, NTRK);
            song.sel = (uint8_t)(b[1] % trk_n());
            for (k = b[2], b += 3; k < song.ntrk && end - b >= 4; k++) {
                const char *nm = (const char *)b + 3;
                /* no instrument (a MIDI, audio or plug-in track): one that is never offered, so the remote tells it
                 * back as none (127) and never as the first instrument's sound */
                trk[k].engine = trk[k].eng_req = b[0] < REMOTE_ENGINES ? REMOTE_IDX[b[0]] : ENGI_NONE_HERE;
                trk[k].preset = (uint8_t)rd14(b + 1);
                str_cpy(trk_name[k], nm, sizeof trk_name[k]);
                b = (const uint8_t *)nm + str_len(nm) + 1;
            }
            lk.sel = song.sel;
            lk.ntrk = song.ntrk;
            for (k = 0; k < NTRK; k++) {
                lk.eng[k] = trk[k].eng_req;
                lk.preset[k] = trk[k].preset;
            }
        }
        break;
    case L_PARAM:                                      /* (track, id v14, value v14)...: P_*, told (4096 + k) */
        for (; end - b >= 5; b += 5)
            if (b[0] < NTRK && (uint32_t)rd14(b + 1) < P_COUNT) {
                uint32_t id = (uint32_t)rd14(b + 1);
                trk[b[0]].p[id] = lk.p[b[0]][id] = (int16_t)rd14(b + 3);
                layer_heard(b[0], id, rd14(b + 3));
            } else if (b[0] < NTRK && rd14(b + 1) >= (int32_t)TOLD_ID) {
                told_param(b[0], (uint32_t)rd14(b + 1) - TOLD_ID, rd14(b + 3));
            }
        break;
    case L_GLOBAL:                                     /* (id, value v14)... */
        for (; end - b >= 3; b += 3)
            if (b[0] < G_COUNT) {
                song.g[b[0]] = lk.g[b[0]] = (int16_t)rd14(b + 1);
                layer_heard(0xFFu, b[0], rd14(b + 1));
            }
        break;
    case L_STEPS:                                      /* track, first, count, count x 13 bytes */
        if (end - b >= 3 && b[0] < NTRK) {
            k = b[0];
            for (i = 0; i < b[2] && b[1] + i < NSTEP && end - (b + 3 + 13u * i) >= 13; i++) {
                step_get(&trk[k].step[b[1] + i], b + 3 + 13u * i);
                lk.step[k][b[1] + i] = trk[k].step[b[1] + i];
            }
        }
        break;
    case L_PATTERNS:                                   /* track, playing, next (127 none), holds (3 x 7 bits),
                                                        * then up to 16 x (length, division) */
        if (end - b >= 6 && b[0] < NTRK) {
            k = b[0];
            trk[k].pattern = (uint8_t)(b[1] % NPAT);
            trk[k].pattern_next = b[2] < NPAT ? b[2] : 0xFFu;
            pattern_holds[k] = (uint16_t)(b[3] | b[4] << 7 | (b[5] & 3u) << 14);
            for (i = 0, b += 6; i < NPAT && end - b >= 2; i++, b += 2) {
                pattern_timings[k][i][0] = b[0];
                pattern_timings[k][i][1] = b[1];
            }
            pattern_epoch++;
        }
        break;
    case L_PLAY:                                       /* playing, armed (5 bytes of bits), waiting (5), counting,
                                                        * song running, song row, then per track the step playing */
        if (end - b >= 14) {
            song.playing = b[0] & 1u;
            song.rec = rd_bits(b + 1);
            song.rec_wait = rd_bits(b + 6);
            cin_left = b[11];
            chain.running = b[12] & 1u;
            chain.row = b[13];
            for (k = 0, b += 14; k < NTRK && b < end; k++, b++)
                trk[k].seq_idx = b[0];
        }
        break;
    case L_SONG:                                       /* count, count x (pattern, repeat) */
        if (end - b >= 1) {
            chain_config.count = (uint8_t)(b[0] < CHAIN_ROWS ? b[0] : CHAIN_ROWS);
            for (i = 0, b++; i < chain_config.count && end - b >= 2; i++, b += 2) {
                chain_config.row[i].slot = b[0];
                chain_config.row[i].repeat = b[1];
            }
        }
        break;
    case L_SCOPE:                                      /* samples, 7 bits each (64 the middle) */
        for (; b < end; b++)
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)(((int32_t)b[0] - 64) * 400);
        break;
    default:
        break;
    }
    return 1;
}

/* ------------------------------------------------------- to Ellic Studio --- */
static void link_poll(void)
{
    uint8_t m[300];
    uint32_t k, i, n, now = plat_ms(), p;
    lk.sent = 0;
    {                                                  /* (USB: taken as there, greeted again at each new host) */
        static uint32_t usb_link;
        uint32_t l = plat_midi_out_link();
        if (l != usb_link) {
            usb_link = l;
            lk.link_ms[U_PORT_USB] = 0;
        }
    }
    for (p = 0; p < 2u; p++) {                         /* the links: a new one is greeted, again 1, 2, 4, 8, 16 s on */
        int up = p == U_PORT_USB ? usb.up : plat_ble_connected();
        if (up && !lk.link_ms[p]) {
            lk.link_ms[p] = now | 1u;
            lk.hellos[p] = 0;
            link_hello(p);
        } else if (!up && lk.link_ms[p]) {
            lk.link_ms[p] = 0;
            if (lk.host == 1u + p) {
                link_new_gen(L_WHY_SILENT);
                link_let_go();
            }
        } else if (up && !lk.host && lk.hellos[p] < 5u && now - lk.link_ms[p] >= 1000u << lk.hellos[p]) {
            lk.hellos[p]++;
            link_hello(p);
        }
    }
    if (lk.host && lk.pings && now - lk.heard_ms > L_SILENT_MS) {
        link_new_gen(L_WHY_SILENT);
        link_let_go();
    }
    if (!lk.host)
        return;
    {                                                  /* MASTER: Ellic Studio's master level, 14 bits as CC 7 and 39 */
        static int32_t sent = -1;
        int32_t v = (int32_t)(plat_master() * 16383u / 4096u);
        if (sent < 0 || v - sent > 31 || sent - v > 31 || ((v == 0 || v == 16383) && v != sent)) {
            sent = v;
            midi_out_event(0x0Bu | 0xBFu << 8 | 7u << 16 | (uint32_t)(v >> 7) << 24);
            midi_out_event(0x0Bu | 0xBFu << 8 | 39u << 16 | (uint32_t)(v & 0x7F) << 24);
        }
    }
    while (lk.req_r != lk.req_w && lk.sent < L_SEND_MAX) {   /* what the controls asked */
        i = lk.req_r % 16u;
        m[0] = lk.req[i].what;
        n = 1;
        n += wr14(m + n, lk.req[i].a);
        n += wr14(m + n, lk.req[i].b);
        n += wr14(m + n, lk.req[i].c);
        m[n++] = 0;                                    /* (an empty text) */
        link_send(L_REQUEST, m, n, -1);
        lk.req_r++;
    }
    for (k = 0; k < NTRK && lk.sent < L_SEND_MAX; k++)     /* a sound chosen (PRESETS): that track alone */
        if (k < trk_n() && (trk[k].eng_req != lk.eng[k] || trk[k].preset != lk.preset[k])) {
            memcpy(lk.p[k], trk[k].p, sizeof lk.p[k]);   /* (its values are Ellic Studio's to tell) */
            n = 0;
            m[n++] = song.ntrk;
            m[n++] = song.sel;
            m[n++] = (uint8_t)k;
            m[n++] = (uint8_t)(eng_ok(trk[k].eng_req) ? eng_rank(trk[k].eng_req) : 127u);
            n += wr14(m + n, trk[k].preset);
            n += wr_text(m + n, trk_name[k], TRK_NAME_LEN);   /* (as told: the remote names nothing) */
            lk.eng[k] = trk[k].eng_req;
            lk.preset[k] = trk[k].preset;
            lk.sel = song.sel;
            link_send(L_TRACKS, m, n, -1);
        }
    if (song.sel != lk.sel && lk.sent < L_SEND_MAX) {   /* the track selected: no tracks after it */
        m[0] = song.ntrk;
        m[1] = song.sel;
        m[2] = song.ntrk;
        lk.sel = song.sel;
        link_send(L_TRACKS, m, 3, -1);
    }
    for (k = 0; k < NTRK && lk.sent < L_SEND_MAX; k++) {   /* values the controls turned */
        n = 0;
        for (i = 0; i < P_COUNT && n + 5u <= 250u; i++)
            if (trk[k].p[i] != lk.p[k][i]) {
                m[n++] = (uint8_t)k;
                n += wr14(m + n, (int32_t)i);
                n += wr14(m + n, trk[k].p[i]);
                lk.p[k][i] = trk[k].p[i];
            }
        if (n)
            link_send(L_PARAM, m, n, -1);
    }
    if (told.track < NTRK && lk.sent < L_SEND_MAX) {   /* told values the knobs turned (the told track's) */
        n = 0;
        for (i = 0; i < TOLD_K && n + 5u <= 250u; i++)
            if (told.val[i] != told.lk[i]) {
                m[n++] = told.track;
                n += wr14(m + n, (int32_t)(TOLD_ID + i));
                n += wr14(m + n, told.val[i]);
                told.lk[i] = told.val[i];
            }
        if (n)
            link_send(L_PARAM, m, n, -1);
    }
    page_poll();                                       /* (the page shown, the preferences: Ellic Studio's to know) */
    prefs_poll();
    n = 0;
    for (i = 0; i < G_COUNT && n + 3u <= 250u; i++)
        if (song.g[i] != lk.g[i]) {
            m[n++] = (uint8_t)i;
            n += wr14(m + n, song.g[i]);
            lk.g[i] = song.g[i];
        }
    if (n && lk.sent < L_SEND_MAX)
        link_send(L_GLOBAL, m, n, -1);
    for (k = 0; k < NTRK && lk.sent < L_SEND_MAX; k++)    /* steps the controls wrote: a run at a time */
        for (i = 0; i < NSTEP && lk.sent < L_SEND_MAX; i++)
            if (memcmp(&trk[k].step[i], &lk.step[k][i], sizeof(step_t))) {
                uint32_t c = 0;
                m[0] = (uint8_t)k;
                m[1] = (uint8_t)i;
                while (i + c < NSTEP && c < 16u && memcmp(&trk[k].step[i + c], &lk.step[k][i + c], sizeof(step_t))) {
                    step_put(m + 3 + 13u * c, &trk[k].step[i + c]);
                    lk.step[k][i + c] = trk[k].step[i + c];
                    c++;
                }
                m[2] = (uint8_t)c;
                link_send(L_STEPS, m, 3u + 13u * c, -1);
                i += c;
            }
}

static void link_start(void)
{
    lk.gen = (uint16_t)((plat_master() * 2654435761u ^ plat_ms() * 40503u) & 0x3FFFu);   /* (a new run: unlike the last) */
    lk.why = L_WHY_POWER;
    usb.up = usb.config = 1;
    link_mark();
}
