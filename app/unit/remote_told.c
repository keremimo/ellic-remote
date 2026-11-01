/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The selected track's sound pages as Melodee tells them (docs/PROTOCOL.md, "The selected track's pages"): PAGES (its
 * EDIT, ENV, LFO and FX pages and Stage's knobs, each four told values), DESC (each value's range and how it reads, or
 * an action), PARAM 4096 + k (the values), WAVE and MARKS (a sample's or a clip's sound and its marks). They fill the
 * page table's told slots (params.c SC_TOLD), which the families step through as Melodee FM-1's own pages; a knob turns
 * a value here and link.c tells Melodee what changed; an action's column asks it (REQUEST ACTION). While another track
 * is selected than the one told, the slots show nothing: Melodee tells the new one's at once. */
/* text kept with the set ("" when there is no room left) */
static const char *told_text(const char *s, uint32_t n)
{
    char *d;
    if (n > 31u)
        n = 31u;
    if (told.used + n + 1u > TOLD_TEXT)
        return "";
    d = told.text + told.used;
    memcpy(d, s, n);
    d[n] = 0;
    told.used = (uint16_t)(told.used + n + 1u);
    return d;
}

/* a new set for track tr: no pages, no values, until Melodee tells them */
static void told_reset(uint32_t tr)
{
    uint32_t k;
    told.track = (uint8_t)tr;
    told.n = 0;
    told.used = 0;
    told.nnames = 0;
    told.keys = 0;
    for (k = 0; k < TOLD_K; k++) {
        told.desc[k].label = 0;
        told.val[k] = told.lk[k] = 0;
    }
    memset(told.action, 0, sizeof told.action);
    told.gen++;
}

/* PAGES: track, keys, pads, first, count, total, then count x (family, view, k0..k3, title); first 0 starts a set */
static void told_pages(const uint8_t *b, const uint8_t *end)
{
    uint32_t first, count, total, i, j;
    if (end - b < 6 || b[0] >= NTRK)
        return;
    first = b[3];
    count = b[4];
    total = b[5];
    if (first == 0u || told.track != b[0])
        told_reset(b[0]);
    told.keys = b[1] & 1u;
    told.pads = b[2] & 127u;
    for (i = 0, b += 6; i < count && first + i < TOLD_PAGES && end - b >= 7; i++) {
        const char *t = (const char *)b + 6;
        uint32_t len = str_len(t);
        if ((const uint8_t *)t + len >= end)
            break;
        told.page[first + i].fam = b[0] < TF_N ? b[0] : TF_EDIT;
        told.page[first + i].view = b[1];
        for (j = 0; j < 4u; j++)
            told.page[first + i].k[j] = b[2 + j] < TOLD_K ? b[2 + j] : 0xFFu;
        told.page[first + i].title = told_text(t, len < 12u ? len : 12u);
        b = (const uint8_t *)t + len + 1;
    }
    told.n = (uint8_t)(total < TOLD_PAGES ? total : TOLD_PAGES);
    told.gen++;
}

/* DESC: value k's label, format (14: an action), range, default, unit, its value names (nn of them) */
static void told_desc(uint32_t k, uint32_t fmt, int32_t min, int32_t max, int32_t def, const char *label,
                      const char *unit, const char *names, uint32_t nn)
{
    param_desc_t *d;
    uint32_t i;
    if (k >= TOLD_K)
        return;
    d = &told.desc[k];
    d->label = told_text(label, str_len(label));
    d->fmt = (uint8_t)(fmt == 14u ? F_INT : fmt);
    d->min = (int16_t)min;
    d->max = (int16_t)(fmt == 14u ? min : max);
    d->def = (int16_t)def;
    d->unit = unit[0] ? told_text(unit, str_len(unit)) : 0;
    d->names = 0;
    if (fmt == 14u)
        told.action[k >> 3] |= (uint8_t)(1u << (k & 7u));
    else
        told.action[k >> 3] &= (uint8_t)~(1u << (k & 7u));
    if (nn && told.nnames + nn + 1u <= TOLD_NAMES) {
        d->names = &told.names[told.nnames];
        for (i = 0; i < nn; i++) {
            told.names[told.nnames++] = told_text(names, str_len(names));
            names += str_len(names) + 1u;
        }
        told.names[told.nnames++] = 0;
    }
    told.gen++;
}

/* PARAM 4096 + k: the told track's value k */
static void told_param(uint32_t tr, uint32_t k, int32_t v)
{
    if (tr != told.track || k >= TOLD_K)
        return;
    told.val[k] = told.lk[k] = (int16_t)v;
    told.gen++;
}

/* WAVE: track, gen, first column, count, then count x (low, high) */
static void told_wave(const uint8_t *b, const uint8_t *end)
{
    uint32_t first, count, i;
    if (end - b < 5 || b[0] != told.track)
        return;
    told.wave_gen = (uint16_t)told_v14(b + 1);
    first = b[3];
    count = b[4];
    for (i = 0, b += 5; i < count && first + i < TOLD_WAVE_W && end - b >= 2; i++, b += 2) {
        told.wave[first + i][0] = b[0];
        told.wave[first + i][1] = b[1];
    }
    told.gen++;
}

/* MARKS: track, gen, count, then count x (kind, position, value): all of them, again */
static void told_marks(const uint8_t *b, const uint8_t *end)
{
    uint32_t i, n;
    if (end - b < 4 || b[0] != told.track)
        return;
    told.marks_gen = (uint16_t)told_v14(b + 1);
    n = b[3];
    for (i = 0, b += 4; i < n && i < TOLD_MARKS && end - b >= 5; i++, b += 5) {
        told.mark[i].kind = b[0];
        told.mark[i].pos = (int16_t)told_v14(b + 1);
        told.mark[i].value = (int16_t)told_v14(b + 3);
    }
    told.nmarks = (uint8_t)i;
    told.gen++;
}

/* the page table's told slots as the set is (the selected track's; none while another's): 1 if they changed */
static uint32_t told_first;              /* the first told slot in PAGES */
static uint32_t told_synced = 0xFFFFFFFFu;
static int told_sync(void)
{
    static const uint8_t FAM[TF_N] = {FAM_HOME, FAM_EDIT, FAM_ENV, FAM_LFO, FAM_FX};
    static const uint8_t GRAPH[3] = {GR_NONE, GR_WAVE, GR_ADSR};
    uint32_t i, j, sig = told.gen * 2654435761u + song.sel * 40503u;
    if (!told_first)
        while (told_first < NPAGES && PAGES[told_first].scope != SC_TOLD)
            told_first++;
    if (sig == told_synced)
        return 0;
    told_synced = sig;
    for (i = 0; i < TOLD_PAGES; i++) {
        page_t *pg = &PAGES[told_first + i];
        if (told_on() && i < told.n && told.page[i].fam != TF_HOME) {
            pg->title = told.page[i].title;
            pg->fam = FAM[told.page[i].fam % TF_N];
            pg->graph = told.page[i].view < 3u ? GRAPH[told.page[i].view] : GR_NONE;
            for (j = 0; j < 4u; j++)
                pg->id[j] = told.page[i].k[j];
        } else {
            pg->title = "";
            pg->fam = FAM_COUNT;                 /* (no family's: never shown) */
            pg->graph = GR_NONE;
            for (j = 0; j < 4u; j++)
                pg->id[j] = 0xFFu;
        }
    }
    return 1;
}
static int told_visible(uint32_t i) { return told_on() && i - told_first < told.n && PAGES[i].fam < FAM_COUNT; }

/* Stage's four knobs: the set's HOME page (none told: none) */
static int told_home(uint8_t k[4])
{
    uint32_t i, j;
    for (i = 0; told_on() && i < told.n; i++)
        if (told.page[i].fam == TF_HOME) {
            for (j = 0; j < 4u; j++)
                k[j] = told.page[i].k[j];
            return 1;
        }
    for (j = 0; j < 4u; j++)
        k[j] = 0xFFu;
    return 0;
}

/* a told page's value on a knob's column as 0..127 of its range (the envelope's curve) */
static int32_t told_value127(uint32_t k)
{
    const param_desc_t *d = k < TOLD_K ? &told.desc[k] : 0;
    if (!d || !d->label || d->max <= d->min)
        return 0;
    return (told.val[k] - d->min) * 127 / (d->max - d->min);
}

/* the pads on the keys (PAGES keys 1, the told track): white key i plays the pads' first note + i, the black keys
 * nothing; -1 the layout's (engine_t.keys) */
static int32_t remote_pad_keys(const struct track *t, uint32_t k)
{
    if (!told.keys || (uint32_t)(t - trk) != told.track)
        return -1;
    if (key_black(k))
        return (int32_t)KB_SILENT;
    return (int32_t)told.pads + (int32_t)key_place(k);
}
