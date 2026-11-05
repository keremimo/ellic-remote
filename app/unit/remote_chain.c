/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The selected track's chain and the lists around it, as Ellic Studio tells them (docs/PROTOCOL.md version 5, "The
 * chain"): CHAIN (the devices in order, each its kind, its switch, its name and its first reading; the one chosen),
 * CATALOGUE (what a chain can take, in Ellic Studio's groups: the Add and Replace lists), SAMPLES (Ellic Studio's samples
 * in their folders: LOAD's list) and METER (a take's source level). What the CHAIN page's controls do goes back as
 * requests (DEVICE ON, MOVE, REMOVE, ADD, REPLACE, CHOOSE; LOAD SAMPLE): Ellic Studio does it and tells CHAIN again. */
static void chain_changed(void);                       /* (ui_chain.c: the page redrawn) */
static uint32_t told_find(const char *label);           /* (below: a told value by its label) */
static const char *const CHAIN_TAG[CK_N] = {"NOTE", "MIDI", "INST", "FX", "LOOP", "PLUG"};
static const char *const CHAIN_KIND[CK_N] = {"Note effect", "MIDI effect", "Instrument", "Effect", "LOOPER", "Plug-in"};
static const char *const CAT_GROUP[CG_N] = {"Note effects", "Instruments", "Effects", "Plug-ins"};

/* a name kept in a pool ("" when there is no room left) */
static const char *pool_text(char *pool, uint16_t *used, uint32_t cap, const char *s, uint32_t n)
{
    char *d;
    if (n > 31u)
        n = 31u;
    if (*used + n + 1u > cap)
        return "";
    d = pool + *used;
    memcpy(d, s, n);
    d[n] = 0;
    *used = (uint16_t)(*used + n + 1u);
    return d;
}

static void loop_reset(void)
{
    loopm.track = 0xFFu;
    loopm.device = 127;
    loopm.focus = 0;
    memset(loopm.loop, 0, sizeof loopm.loop);
    loopm.gen++;
}
static void chain_reset(void)
{
    loop_reset();
    chn.track = 0xFFu;
    chn.chosen = 127;
    chn.n = 0;
    chn.row = 0;
    chn.gen++;
}
static void lists_reset(void)                          /* (a new claim: Ellic Studio tells the lists again) */
{
    memset(cat.n, 0, sizeof cat.n);
    cat.used = 0;
    smp.nf = 0;
    memset(smp.n, 0, sizeof smp.n);
    smp.used = 0;
    smp.gen++;
    meter.ms = 0;
}

/* CHAIN: track, the device chosen, count, then count x (kind, on, name, reading) */
static void chain_told(const uint8_t *b, const uint8_t *end)
{
    uint32_t i, n, was = chn.track;
    if (end - b < 3 || b[0] >= NTRK)
        return;
    chn.track = b[0];
    chn.chosen = b[1];
    n = b[2] < CHAIN_MAX ? b[2] : CHAIN_MAX;
    for (i = 0, b += 3; i < n && end - b >= 4; i++) {
        const char *nm = (const char *)b + 2, *rd = nm + str_len(nm) + 1;
        if ((const uint8_t *)rd >= end)
            break;
        chn.dev[i].kind = b[0] < CK_N ? b[0] : CK_FX;
        chn.dev[i].on = b[1] & 1u;
        str_cpy(chn.dev[i].name, nm, CHAIN_NAME);
        str_cpy(chn.dev[i].read, rd, CHAIN_READ);
        b = (const uint8_t *)rd + str_len(rd) + 1;
    }
    chn.n = (uint8_t)i;
    if (was != chn.track || chn.row > chn.n)           /* another track's, or the row gone: the chosen device's row */
        chn.row = chn.chosen < chn.n ? chn.chosen : (chn.row > chn.n ? chn.n : chn.row);
    chn.gen++;
    chain_changed();
}

/* CATALOGUE: group, first v14, count, names (first 0: the group anew; group 0 first 0: all of them) */
static void cat_told(const uint8_t *b, const uint8_t *end)
{
    uint32_t g, first, count, i;
    const char *q;
    if (end - b < 4 || b[0] >= CG_N)
        return;
    g = b[0];
    first = (uint32_t)told_v14(b + 1);
    count = b[3];
    if (first == 0u && g == 0u) {
        memset(cat.n, 0, sizeof cat.n);
        cat.used = 0;
    }
    if (first == 0u)
        cat.n[g] = 0;
    for (i = 0, q = (const char *)b + 4; i < count && (const uint8_t *)q < end; i++) {
        uint32_t len = str_len(q);
        if ((const uint8_t *)q + len >= end)
            break;
        if (first + i < CAT_MAX) {
            cat.name[g][first + i] = (uint16_t)(pool_text(cat.text, &cat.used, CAT_TEXT, q, len) - cat.text);
            if (first + i >= cat.n[g])
                cat.n[g] = (uint8_t)(first + i + 1u);
        }
        q += len + 1u;
    }
}
static const char *cat_name(uint32_t g, uint32_t i)
{
    return g < CG_N && i < cat.n[g] ? cat.text + cat.name[g][i] : "";
}

/* SAMPLES: folder, its name (on the message with first 0), first v14, count, names */
static void smp_told(const uint8_t *b, const uint8_t *end)
{
    uint32_t f, first, count, i;
    const char *nm, *q;
    if (end - b < 2 || b[0] >= SMP_FOLDERS)
        return;
    f = b[0];
    nm = (const char *)b + 1;
    if ((const uint8_t *)nm + str_len(nm) + 3 > end)
        return;
    q = nm + str_len(nm) + 1;
    first = (uint32_t)told_v14((const uint8_t *)q);
    count = (uint32_t)((const uint8_t *)q)[2];
    q += 3;
    if (f == 0u && first == 0u) {
        smp.nf = 0;
        memset(smp.n, 0, sizeof smp.n);
        smp.used = 0;
    }
    if (first == 0u) {
        smp.n[f] = 0;
        smp.folder[f] = (uint16_t)(pool_text(smp.text, &smp.used, SMP_TEXT, nm, str_len(nm)) - smp.text);
    }
    if (f >= smp.nf)
        smp.nf = (uint8_t)(f + 1u);
    for (i = 0; i < count && (const uint8_t *)q < end; i++) {
        uint32_t len = str_len(q);
        if ((const uint8_t *)q + len >= end)
            break;
        if (first + i < SMP_MAX) {
            smp.name[f][first + i] = (uint16_t)(pool_text(smp.text, &smp.used, SMP_TEXT, q, len) - smp.text);
            if (first + i >= smp.n[f])
                smp.n[f] = (uint8_t)(first + i + 1u);
        }
        q += len + 1u;
    }
    smp.gen++;
    chain_changed();
}
static const char *smp_folder_name(uint32_t f) { return f < smp.nf ? smp.text + smp.folder[f] : ""; }
static const char *smp_name(uint32_t f, uint32_t i) { return f < smp.nf && i < smp.n[f] ? smp.text + smp.name[f][i] : ""; }

/* LOOP: track, device, the loop in focus, then 2 x (state, waiting, armed, key, bars x 4, bars set, layers, undone,
 * position, hears) */
static void loop_told(const uint8_t *b, const uint8_t *end)
{
    uint32_t i;
    if (end - b < 23)
        return;
    loopm.track = b[0];
    loopm.device = b[1];
    loopm.focus = b[2] & 1u;
    for (i = 0, b += 3; i < 2u; i++, b += 10) {
        loop_t *l = &loopm.loop[i];
        l->state = b[0] <= LS_STOPPED ? b[0] : LS_EMPTY;
        l->waiting = b[1] <= LW_STOP ? b[1] : LW_NONE;
        l->armed = b[2] & 1u;
        l->key = b[3] & 3u;
        l->bars4 = b[4];
        l->set = b[5];
        l->layers = b[6];
        l->undone = b[7] & 1u;
        l->pos = b[8] & 127u;
        l->hear = b[9] & 127u;
    }
    loopm.gen++;
    chain_changed();
}
/* the bar's LOOP key (REC on the LOOPER's pages), a loop's own footswitch (its key on PATTERNS); held: the loop stops */
static void loop_key_req(uint32_t held) { link_request(RQ_LOOP_KEY, held, 0, 0); }
static void loop_footswitch_req(uint32_t i, uint32_t held) { link_request(RQ_LOOP_FOOTSWITCH, song.sel, loopm.device * 2u + i, held); }
/* OCT- / OCT+ on the LOOP page: the loop in focus (the told value LOOP, Ellic Studio's loop last touched) */
static void loop_pick(uint32_t i)
{
    uint32_t k = told_find("LOOP");
    if (k < TOLD_K) {
        told.val[k] = (int16_t)(told.desc[k].min + (int32_t)i);
        told.gen++;
    }
    loopm.focus = (uint8_t)(i & 1u);
    loopm.gen++;
    chain_changed();
}

/* METER: track, the source's level */
static void meter_told(const uint8_t *b, const uint8_t *end)
{
    if (end - b < 2)
        return;
    meter.track = b[0];
    meter.level = b[1] & 127u;
    meter.ms = plat_ms() | 1u;
}
static int meter_fresh(void) { return meter.ms && meter.track == song.sel && plat_ms() - meter.ms < METER_FRESH_MS; }

/* a told value by its label (BANK, SOURCE, ROW, PAD: PROTOCOL.md "the remote's to know by name"), TOLD_K none */
static uint32_t told_find(const char *label)
{
    uint32_t k;
    for (k = 0; told_on() && k < TOLD_K; k++)
        if (told.desc[k].label && str_eq(told.desc[k].label, label))
            return k;
    return TOLD_K;
}
/* a told value stepped by its knob's grammar (the knobs' turns go to Ellic Studio as PARAM: link.c); 0: no such value */
static int told_step(const char *label, int32_t d)
{
    uint32_t k = told_find(label);
    if (k >= TOLD_K)
        return 0;
    told.val[k] = (int16_t)clamp(told.val[k] + d, told.desc[k].min, told.desc[k].max);
    told.gen++;
    chain_changed();
    return 1;
}
/* OCT- / OCT+ in pads mode: the sampler's bank, in place of the octave (Ellic Studio then tells the keys' pads anew) */
static int told_bank_step(int32_t d) { return told.keys && told_step("BANK", d); }

/* ------------------------------------------------------- requests --- */
static void chain_req_choose(uint32_t i) { if (i < chn.n && i != chn.chosen) link_request(RQ_DEVICE_CHOOSE, song.sel, i, 0); }
static void chain_req_on(uint32_t i, uint32_t on) { link_request(RQ_DEVICE_ON, song.sel, i, on); }
static void chain_req_move(uint32_t i, int32_t d) { link_request(RQ_DEVICE_MOVE, song.sel, i, (uint32_t)(int32_t)(d > 0 ? 1 : -1)); }
static void chain_req_remove(uint32_t i) { link_request(RQ_DEVICE_REMOVE, song.sel, i, 0); }
static void chain_req_add(uint32_t after, uint32_t g, uint32_t i) { link_request(RQ_DEVICE_ADD, song.sel, after, g << 7 | i); }
static void chain_req_replace(uint32_t dev, uint32_t g, uint32_t i) { link_request(RQ_DEVICE_REPLACE, song.sel, dev, g << 7 | i); }
static void load_sample_req(uint32_t pad, uint32_t f, uint32_t i) { link_request(RQ_LOAD_SAMPLE, song.sel, pad, f << 7 | i); }

/* --------------------------------------------- the CHAIN page's knobs --- */
/* KNOB 1 DEVICE (the row: a device, or + Add), KNOB 2 ON, KNOB 3 MOVE, KNOB 4 the chosen device's first knob as its
 * first page lists it (a told value: edit_param turns it, link.c tells it) */
static const char *chain_names[CHAIN_MAX + 2];
static const char *const CHAIN_MOVE_NAMES[] = {"--", 0};
static param_desc_t chain_k[3];
static int16_t chain_v[3];
/* device i's first told page (its title the device's name; FX's first): NPAGES none */
static uint32_t chain_page_of(uint32_t i)
{
    uint32_t p, any = NPAGES;
    if (i >= chn.n || !told_on())
        return NPAGES;
    for (p = 0; p < told.n && p < TOLD_PAGES; p++)
        if (str_eq(told.page[p].title, chn.dev[i].name)) {
            if (told.page[p].fam == TF_FX)
                return told_first + p;
            if (any == NPAGES)
                any = told_first + p;
        }
    return any;
}
static const param_desc_t *chain_desc(uint32_t slot, int16_t **valp)
{
    uint32_t i;
    *valp = 0;
    if (!chain_on())
        return 0;
    switch (slot) {
    case 0:
        for (i = 0; i < chn.n; i++)
            chain_names[i] = chn.dev[i].name;
        chain_names[chn.n] = "+ ADD";
        chain_names[chn.n + 1u] = 0;
        chain_k[0] = (param_desc_t){"DEVICE", F_ENUM, 0, (int16_t)chn.n, 0, chain_names, 0};
        chain_v[0] = (int16_t)(chn.row < chn.n ? chn.row : chn.n);
        *valp = &chain_v[0];
        return &chain_k[0];
    case 1:
        if (chn.row >= chn.n)
            return 0;
        chain_k[1] = (param_desc_t){"ON", F_ONOFF, 0, 1, 1, 0, 0};
        chain_v[1] = chn.dev[chn.row].on;
        *valp = &chain_v[1];
        return &chain_k[1];
    case 2:
        if (chn.row >= chn.n)
            return 0;
        chain_k[2] = (param_desc_t){"MOVE", F_ENUM, 0, 0, 0, CHAIN_MOVE_NAMES, 0};
        chain_v[2] = 0;
        *valp = &chain_v[2];
        return &chain_k[2];
    default: {
        uint32_t p = chain_page_of(chn.row), k;
        if (p >= NPAGES || PAGES[p].fam != FAM_FX)
            return 0;
        k = PAGES[p].id[0];
        if (k >= TOLD_K || !told.desc[k].label || told_is_action(k))
            return 0;
        *valp = &told.val[k];
        return &told.desc[k];
    }
    }
}
/* a device moved along the chain here, as Ellic Studio will move it (it tells CHAIN again): a note effect among its own,
 * the rest among themselves. 0: it cannot move that way */
static int chain_move_local(uint32_t i, int32_t d)
{
    uint32_t j = d > 0 ? i + 1u : i - 1u;
    chain_dev_t t;
    if (i >= chn.n || (d < 0 && i == 0u) || j >= chn.n)
        return 0;
    if ((chn.dev[i].kind <= CK_MIDI) != (chn.dev[j].kind <= CK_MIDI))
        return 0;
    t = chn.dev[i];
    chn.dev[i] = chn.dev[j];
    chn.dev[j] = t;
    if (chn.chosen == i) chn.chosen = (uint8_t)j;
    else if (chn.chosen == j) chn.chosen = (uint8_t)i;
    chn.row = (uint8_t)j;
    chn.gen++;
    return 1;
}
/* the CHAIN page's KNOB 1..3 (and SELECT as KNOB 1) turned s: 1 taken */
static int chain_knob(uint32_t k, int32_t s)
{
    uint32_t row;
    if (!chain_on() || k > 2u)
        return 0;
    switch (k) {
    case 0:
        row = (uint32_t)clamp((int32_t)chn.row + (s > 0 ? 1 : -1), 0, (int32_t)chn.n);
        if (row != chn.row) {
            chn.row = (uint8_t)row;
            chain_req_choose(row);
            chn.gen++;
            chain_changed();
        }
        return 1;
    case 1:
        if (chn.row < chn.n && chn.dev[chn.row].on != (s > 0)) {
            chn.dev[chn.row].on = s > 0;
            chain_req_on(chn.row, (uint32_t)(s > 0));
            chn.gen++;
            chain_changed();
        }
        return 1;
    default:
        if (chn.row < chn.n && chain_move_local(chn.row, s))
            chain_req_move(s > 0 ? chn.row - 1u : chn.row + 1u, s);
        return 1;
    }
}
