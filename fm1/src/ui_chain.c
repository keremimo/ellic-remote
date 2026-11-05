/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The chain and the pads on the remote (docs/PROTOCOL.md version 5, "The chain and the pads on the remote"):
 *   CHAIN (FX's first page): the track's devices a row each in the panel (its lamp, its kind, its name, its first
 *     reading), the row picked tinted, + Add last; KNOB 1 / SELECT the row, KNOB 2 ON, KNOB 3 MOVE, KNOB 4 the
 *     device's first knob (remote_chain.c chain_desc, chain_knob); OCT+ the row's pages (+ Add: the Add list),
 *     OCT- Stage; OCT+ held the row's sheet: Replace…, Remove (asked first), Add after…. A device's own page (FX,
 *     told) carries the chain as chips in its footer, its own filled; OCT- there goes back to CHAIN.
 *   The Add / Replace list (a popup): CATALOGUE's names in Ellic Studio's groups, KNOB 1 the group, KNOB 2 or PRESETS the
 *     row, OCT+ DEVICE ADD / DEVICE REPLACE, OCT- closes.
 *   PADS (a told page, view 3): a sampler's bank as a 4 x 4 grid under the cards, the pad chosen ringed.
 *   LIST (view 4): LOAD's samples, folder by folder, the row scrolled onto (the told value ROW) in a card.
 *   The meter: a take's source level under a wave while METER is fresh.
 * Included by ui_draw.c after the popups; the graphs are called from ui_graph.c draw_graph. */
static void chain_changed(void) { ui.force = 1; }
static const char *const LOOP_KEY_WORD[4] = {"REC", "PLAY", "DUB", "ARMED"};

/* ------------------------------------------------------ the rows --- */
#define CH_ROW 31                                       /* the panel's 124 rows: four of them */
#define CH_VIS 4u
static void graph_chain(uint16_t c)
{
    uint32_t n = chn.n + 1u, i;
    const aafont_t *tf = TY_TX, *lf = TY_LB;
    if (chn.row < chain_first)
        chain_first = chn.row;
    if (chn.row >= chain_first + CH_VIS)
        chain_first = (uint8_t)(chn.row - CH_VIS + 1u);
    if (chain_first + CH_VIS > n)
        chain_first = (uint8_t)(n > CH_VIS ? n - CH_VIS : 0u);
    for (i = chain_first; i < n && i < chain_first + CH_VIS; i++) {
        int32_t y = (int32_t)(i - chain_first) * CH_ROW, cy = y + CH_ROW / 2, x, xr = 228;
        int sel = i == chn.row;
        uint16_t bg = sel ? ux_mix(T_SURF, c, 18) : T_SURF;
        if (sel) {                                      /* the row picked: outlined and tinted in the track's colour */
            cv_rrect(5, y + 2, 230, CH_ROW - 4, 7, c, T_SURF);
            cv_rrect(6, y + 3, 228, CH_ROW - 6, 6, bg, c);
        }
        if (i == chn.n) {                               /* + Add */
            if (!sel) {
                cv_rrect(5, y + 2, 230, CH_ROW - 4, 7, T_LINE, T_SURF);
                cv_rrect(6, y + 3, 228, CH_ROW - 6, 6, T_SURF, T_LINE);
            }
            cv_text_on(16, ty_y(cy, tf), tf, "+ Add", sel ? T_TEXT : T_MID, bg);
            continue;
        }
        {
            const chain_dev_t *d = &chn.dev[i];
            uint16_t nc = !d->on ? T_DIM : sel ? T_TEXT : T_SEC;
            cv_disc(17, cy, 7, d->on ? c : T_DIM, !d->on);   /* its lamp: filled while on */
            cv_text_on(27, ty_y(cy, lf), lf, CHAIN_TAG[d->kind % CK_N], sel ? T_MID : T_DIM, bg);
            if (d->read[0])
                xr = cv_text_r(xr, ty_y(cy, lf), lf, d->read, !d->on ? T_DIM : sel ? c : T_MID, bg) - 8;
            x = 72;
            cv_free_text(x, ty_y(cy, tf), tf, d->name, nc, bg, xr - x);
        }
    }
}

/* a page's title is the device's: its name, or its name and a number ("LOOPER 2": its second page) */
static int title_of_dev(const char *title, const char *name)
{
    uint32_t n = str_len(name);
    return n && (str_eq(title, name) || (!memcmp(title, name, n) && title[n] == ' '));
}
/* the device whose page this is (its title the device's name), CHAIN_MAX none */
static uint32_t chain_dev_of_page(uint32_t p)
{
    uint32_t i;
    if (p >= NPAGES || PAGES[p].scope != SC_TOLD)
        return CHAIN_MAX;
    for (i = 0; i < chn.n; i++)
        if (title_of_dev(PAGES[p].title, chn.dev[i].name))
            return i;
    return CHAIN_MAX;
}
static uint32_t chain_page(void)                        /* the CHAIN page, NPAGES none */
{
    uint32_t i;
    for (i = 0; i < NPAGES && PAGES[i].graph != GR_CHAIN; i++)
        ;
    return i;
}
static void chain_show(void)
{
    uint32_t p = chain_page();
    if (p >= NPAGES)
        return;
    ui.home = 0;
    ui.page = (uint8_t)p;
    ui.fam_last[FAM_FX] = ui.page;
    page_entered();
    ui.force = 1;
}
static void page_go_to(uint32_t p)                      /* page p shown, as its button would */
{
    ui.home = 0;
    ui.page = (uint8_t)p;
    if (PAGES[p].fam < FAM_COUNT)
        ui.fam_last[PAGES[p].fam] = ui.page;
    page_entered();
    ui.force = 1;
}

/* the footer: CHAIN's keycaps (OCT+ OPEN or ADD, OCT- BACK); a device's page: the chain as chips, its own filled.
 * 1: drawn here (ui_draw.c draw_foot) */
static int chain_foot(void)
{
    const page_t *pg = cur_page();
    const aafont_t *lf = TY_LB;
    uint32_t sig, i, cur, from;
    int32_t x = 6, cy = H_FOOT / 2;
    if (ui.home)
        return 0;
    if (pg->graph == GR_CHAIN) {
        khint_t kh[2] = {{KC_OCTUP, chn.row < chn.n ? "OPEN" : "ADD"}, {KC_OCTDN, "BACK"}};
        sig = chn.gen * 7u + chn.row * 131u + ux.gen * 977u + ux.pal * 31u + 0x4348u;
        if (!ui.force && sig == ui.foot_sig)
            return 1;
        ui.foot_sig = sig;
        cv_begin(240, H_FOOT, T_BG);
        cv_key_row(8, 232, (H_FOOT - KC_PH) / 2, kh, 2, chain_on() ? 3u : 2u, T_BG);
        cv_blit(0, Y_FOOT);
        return 1;
    }
    if (pg->graph == GR_LOOP) {                         /* LOOP: REC the bar's LOOP key, OCT- / OCT+ the loop in focus */
        khint_t kh[3] = {{KC_REC, LOOP_KEY_WORD[loopm.loop[loopm.focus & 1u].key & 3u]}, {KC_OCTDN, "1"}, {KC_OCTUP, "2"}};
        sig = loopm.gen * 7u + ux.gen * 977u + ux.pal * 31u + 0x4C50u;
        if (!ui.force && sig == ui.foot_sig)
            return 1;
        ui.foot_sig = sig;
        pv.sound = 0;
        cv_begin(240, H_FOOT, T_BG);
        cv_key_row(8, 232, (H_FOOT - KC_PH) / 2, kh, 3, loop_on() ? 7u : 0u, T_BG);
        cv_blit(0, Y_FOOT);
        return 1;
    }
    if (pg->graph == GR_LIST) {                         /* LOAD: OCT+ loads the row onto the pad */
        khint_t kh[2] = {{KC_OCTUP, "LOAD"}, {KC_OCTDN, "BACK"}};
        sig = smp.gen * 7u + ux.gen * 977u + ux.pal * 31u + 0x4C44u;
        if (!ui.force && sig == ui.foot_sig)
            return 1;
        ui.foot_sig = sig;
        pv.sound = 0;
        cv_begin(240, H_FOOT, T_BG);
        cv_key_row(8, 232, (H_FOOT - KC_PH) / 2, kh, 2, smp.nf ? 3u : 2u, T_BG);
        cv_blit(0, Y_FOOT);
        return 1;
    }
    if (pg->scope != SC_TOLD || pg->fam != FAM_FX || !chain_on() || !chn.n)
        return 0;
    cur = chain_dev_of_page(ui.page);
    sig = chn.gen * 7u + cur * 131u + ui.page * 1291u + ux.gen * 977u + ux.pal * 31u + 0x4346u;
    if (!ui.force && sig == ui.foot_sig)
        return 1;
    ui.foot_sig = sig;
    pv.sound = 0;                                       /* (the sound bar's place: it draws anew when it is back) */
    cv_begin(240, H_FOOT, T_BG);
    for (from = 0, x = 6; from < cur && cur < CHAIN_MAX; from++) {   /* from the head while the page's fits, else on */
        int32_t w = 0;
        for (i = from; i <= cur; i++)
            w += text_w(lf, chn.dev[i].name) + 14;
        if (w <= 230)
            break;
    }
    for (i = from; i < chn.n; i++) {
        int32_t w = text_w(lf, chn.dev[i].name) + 10;
        int on = i == cur;
        if (x + w > 236)
            break;
        if (on) {
            cv_rrect(x, cy - 9, w, 18, 5, T_THEME, T_BG);
        } else {
            cv_rrect(x, cy - 9, w, 18, 5, T_LINE, T_BG);
            cv_rrect(x + 1, cy - 8, w - 2, 16, 4, T_BG, T_LINE);
        }
        cv_text_on(x + 5, ty_y(cy, lf), lf, chn.dev[i].name, on ? T_INK : chn.dev[i].on ? T_MID : T_DIM, on ? T_THEME : T_BG);
        x += w + 4;
    }
    cv_blit(0, Y_FOOT);
    return 1;
}

/* OCT+ on CHAIN: the row's pages (its first told page; an instrument's EDIT; ARP's page), + Add: the Add list.
 * 0: not on CHAIN */
static void cat_open(int replace, uint32_t at);
static int chain_enter(void)
{
    uint32_t p;
    if (ui.home || cur_page()->graph != GR_CHAIN)
        return 0;
    if (!chain_on())
        return 1;
    if (chn.row >= chn.n) {
        cat_open(0, 127);
        return 1;
    }
    p = chain_page_of(chn.row);
    if (p < NPAGES)
        page_go_to(p);
    else if (chn.dev[chn.row].kind == CK_INST)
        open_family(FAM_EDIT);
    else if (str_eq(chn.dev[chn.row].name, "ARP"))
        open_family(FAM_ARP);
    else
        ui_message("NO PAGE HERE");
    return 1;
}

/* ------------------------------------------------------ the sheet --- */
static void dev_replace(void) { cat_open(1, chn.row); }
static void dev_add_after(void) { cat_open(0, chn.row); }
static const sheet_row_t SHEET_DEVICE[] = {{"Replace\x85", 0, 0, dev_replace, 0}, {"Remove", 0, 0, 0, CF_DEL_DEVICE},
                                           {"Add after\x85", 0, 0, dev_add_after, 0}};
static void chain_sheet_open(void)
{
    char sub[20];
    if (!chain_on() || chn.row >= chn.n)
        return;
    str_cpy(sub, CHAIN_KIND[chn.dev[chn.row].kind % CK_N], sizeof sub);
    sheet_open(chn.dev[chn.row].name, sub, SHEET_DEVICE, NELEM(SHEET_DEVICE));
}
/* the question before a device goes (ui_draw.c confirm_text): "Remove device?", the device named under it */
static void chain_confirm_text(char *a, char *b)
{
    uint32_t i = ui.confirm_trk;
    str_cpy(a, "REMOVE DEVICE?", 24);
    if (i < chn.n) {
        str_cpy(b, chn.dev[i].name, 32);
        str_cpy(b + str_len(b), " \xB7 ", 4);
        str_cpy(b + str_len(b), CHAIN_KIND[chn.dev[i].kind % CK_N], 12);
        str_cpy(b + str_len(b), " ", 2);
        fmt_int(b + str_len(b), (int32_t)i + 1);
        str_cpy(b + str_len(b), " of ", 5);
        fmt_int(b + str_len(b), (int32_t)chn.n);
    }
}

/* ------------------------------------------- the Add / Replace list --- */
static struct { uint8_t replace, at, g, idx[CG_N]; } catp;   /* at: the device replaced, or the one added after */
static void cat_open(int replace, uint32_t at)
{
    uint32_t kind = at < chn.n ? chn.dev[at].kind : CK_FX;
    catp.replace = (uint8_t)replace;
    catp.at = (uint8_t)at;
    catp.g = (uint8_t)(replace ? (kind == CK_NOTE ? CG_NOTE : kind == CK_INST ? CG_INST : kind == CK_PLUGIN ? CG_PLUGIN : CG_FX)
                               : CG_FX);
    pop.on = POP_CAT;
    pop.sel = 0;
    ui.force = 1;
}
/* KNOB 1 the group, KNOB 2 / PRESETS the row; OCT+ asks for it, OCT- closes (oct: bit 0 OCT-, bit 1 OCT+) */
static void cat_input(int32_t k1, int32_t k2, uint32_t oct)
{
    uint32_t g = catp.g % CG_N;
    if (k1) {
        catp.g = (uint8_t)clamp((int32_t)g + (k1 > 0 ? 1 : -1), 0, CG_N - 1);
        g = catp.g;
        ui.force = 1;
    }
    if (k2 && cat.n[g]) {
        catp.idx[g] = (uint8_t)clamp((int32_t)catp.idx[g] + (k2 > 0 ? 1 : -1), 0, (int32_t)cat.n[g] - 1);
        ui.force = 1;
    }
    if (oct & 1u) {
        pop_close();
    } else if (oct & 2u) {
        pop_close();
        if (!cat.n[g]) {
            ui_message("NOTHING TO ADD");
            return;
        }
        if (catp.idx[g] >= cat.n[g])
            catp.idx[g] = 0;
        if (catp.replace)
            chain_req_replace(catp.at, g, catp.idx[g]);
        else
            chain_req_add(catp.at, g, catp.idx[g]);
    }
}
static void cat_row(uint32_t i, int32_t y)
{
    uint32_t g = catp.g % CG_N;
    if (!cat.n[g])
        pop_row(y, "Nothing told", "", 0, 0, 0);
    else
        pop_row(y, cat_name(g, i), "", i == catp.idx[g], 0, 0);
}
static void cat_draw(void)
{
    uint32_t g = catp.g % CG_N, n = cat.n[g] ? cat.n[g] : 1u, vis = n < POP_VIS ? n : POP_VIS, first, sig;
    char title[24];
    if (catp.idx[g] >= cat.n[g])
        catp.idx[g] = 0;
    sig = g * 7u + catp.idx[g] * 131u + n * 1031u + catp.replace * 3u + catp.at * 17u + ux.gen * 977u + ux.pal * 31u + 0x4341u;
    if (!ui.force && sig == pop.sig)
        return;
    pop.sig = sig;
    if (catp.replace) {
        str_cpy(title, "Replace ", sizeof title);
        str_cpy(title + 8, catp.at < chn.n ? chn.dev[catp.at].name : "", sizeof title - 8);
    } else if (catp.at < chn.n) {
        str_cpy(title, "Add after ", sizeof title);
        str_cpy(title + 10, chn.dev[catp.at].name, sizeof title - 10);
    } else {
        str_cpy(title, "Add at the end", sizeof title);
    }
    first = (uint32_t)clamp((int32_t)catp.idx[g] - (int32_t)vis / 2, 0, (int32_t)(n - vis));
    pop_card(POP_ROW + (int32_t)vis * POP_ROW + 4, title, CAT_GROUP[g], first, vis, cat_row);
}

/* ------------------------------------------------------- the pads --- */
/* a sampler's bank as a grid: the told value PAD (the page's first) picks the pad, BANK the bank (none: the pad's) */
static void graph_pads(uint16_t c)
{
    uint32_t kp = cur_page()->id[0], kb = told_find("BANK"), i;
    int32_t pad = kp < TOLD_K && told.desc[kp].label ? told.val[kp] - told.desc[kp].min : 0;
    int32_t bank = kb < TOLD_K ? told.val[kb] - told.desc[kb].min : pad / 16;
    int32_t sel = pad / 16 == bank ? pad % 16 : -1;
    const aafont_t *lf = TY_LB;
    char id[4];
    for (i = 0; i < 16u; i++) {
        int32_t x = 8 + (int32_t)(i % 4u) * 57, y = 6 + (int32_t)(i / 4u) * 29;
        int on = (int32_t)i == sel;
        id[0] = (char)('A' + bank % 4);
        fmt_int(id + 1, (int32_t)i + 1);
        cv_rrect(x, y, 53, 25, 5, on ? c : T_RAISE, T_SURF);
        if (on)
            cv_rrect(x + 2, y + 2, 49, 21, 4, ux_mix(T_SURF, c, 30), c);
        cv_text_on(x + 6, ty_y(y + 12, lf), lf, id, on ? T_TEXT : T_MID, on ? ux_mix(T_SURF, c, 30) : T_RAISE);
    }
}

/* ------------------------------------------------------- the list --- */
/* LOAD's samples: the told value ROW (folder x 128 + index) in a card, the rows around it, the folder's name over them */
#define LS_ROW 25
static void graph_list(uint16_t c)
{
    uint32_t kr = told_find("ROW");
    int32_t v = kr < TOLD_K ? told.val[kr] : 0, r;
    uint32_t f = (uint32_t)v / 128u, i = (uint32_t)v % 128u, n;
    const aafont_t *tf = TY_TX, *lf = TY_LB;
    char b[12];
    if (!smp.nf) {
        cv_text_c(120, ty_y(H_GRAPH / 2, lf), lf, "NO SAMPLES TOLD", T_MID, T_SURF);
        return;
    }
    if (f >= smp.nf)
        f = smp.nf - 1u;
    n = smp.n[f];
    cv_free_text(12, ty_y(12, lf), lf, smp_folder_name(f), c, T_SURF, 160);
    if (n) {
        fmt_int(b, (int32_t)(i < n ? i : n - 1u) + 1);
        str_cpy(b + str_len(b), "/", 2);
        fmt_int(b + str_len(b), (int32_t)n);
        cv_text_r(228, ty_y(12, lf), lf, b, T_MID, T_SURF);
    } else {
        cv_text_c(120, ty_y(H_GRAPH / 2, lf), lf, "EMPTY", T_MID, T_SURF);
        return;
    }
    for (r = -1; r <= 2; r++) {                         /* the row before, the card, two after */
        int32_t k = (int32_t)i + r, y = 22 + (r + 1) * LS_ROW;
        if (k < 0 || k >= (int32_t)n)
            continue;
        if (!r) {
            uint16_t bg = ux_mix(T_SURF, c, 18);
            cv_rrect(6, y, 228, LS_ROW - 2, 6, bg, T_SURF);
            cv_free_text(14, ty_y(y + LS_ROW / 2 - 1, tf), tf, smp_name(f, (uint32_t)k), T_TEXT, bg, 212);
        } else {
            cv_free_text(14, ty_y(y + LS_ROW / 2 - 1, tf), tf, smp_name(f, (uint32_t)k), r == -1 || r == 1 ? T_SEC : T_MID, T_SURF, 212);
        }
    }
}

/* PRESETS on LOAD: the row scrolled onto (the told value ROW: folder x 128 + index), on through the folders */
static int load_row_step(int32_t d)
{
    uint32_t k = told_find("ROW"), f, i;
    int32_t v;
    if (k >= TOLD_K || !smp.nf)
        return 0;
    v = told.val[k];
    f = (uint32_t)v / 128u;
    i = (uint32_t)v % 128u;
    if (f >= smp.nf) { f = smp.nf - 1u; i = 0; }
    if (d > 0) {
        if (i + 1u < smp.n[f]) i++;
        else if (f + 1u < smp.nf) { f++; i = 0; }
    } else if (i > 0u) {
        i--;
    } else if (f > 0u) {
        f--;
        i = smp.n[f] ? smp.n[f] - 1u : 0u;
    }
    told.val[k] = (int16_t)(f * 128u + i);
    told.gen++;
    ui.force = 1;
    return 1;
}
/* OCT+ on LOAD: the row onto the pad chosen (the told value PAD). 0: not on LOAD */
static int load_enter(void)
{
    uint32_t kr = told_find("ROW"), kp = told_find("PAD");
    int32_t v, pad;
    if (ui.home || cur_page()->graph != GR_LIST)
        return 0;
    if (kr >= TOLD_K || !smp.nf) {
        ui_message("NO SAMPLES TOLD");
        return 1;
    }
    v = told.val[kr];
    pad = kp < TOLD_K ? told.val[kp] - told.desc[kp].min : 0;
    if ((uint32_t)v / 128u >= smp.nf || (uint32_t)v % 128u >= smp.n[(uint32_t)v / 128u]) {
        ui_message("NOTHING HERE");
        return 1;
    }
    load_sample_req((uint32_t)clamp(pad, 0, 127), (uint32_t)v / 128u, (uint32_t)v % 128u);
    ui_message("LOADING");
    return 1;
}

/* ----------------------------------------------------- the LOOPER --- */
/* the page shown is the LOOPER's: its LOOP page (view 5) or another of its device's */
static int loop_page(void)
{
    const page_t *pg = cur_page();
    if (ui.home || !loop_on())
        return 0;
    if (pg->graph == GR_LOOP)
        return 1;
    return pg->scope == SC_TOLD && pg->fam == FAM_FX && chain_dev_of_page(ui.page) == loopm.device;
}
/* REC's light on the LOOPER's pages: the loop in focus, lit while it records or overdubs, blinking while it waits
 * for its line or is armed */
static void loop_led(uint32_t *lit, uint32_t *wait)
{
    const loop_t *l = &loopm.loop[loopm.focus & 1u];
    *lit = l->state == LS_RECORDING || l->state == LS_OVERDUBBING;
    *wait = l->waiting != LW_NONE || l->armed;
}
/* a loop's word: EMPTY, ARMED, WAITING, REC 2/4, 4 BARS +1, DUB +1, STOPPED */
static void loop_word(const loop_t *l, char *b)
{
    if (l->armed) { str_cpy(b, "ARMED", 12); return; }
    if (l->state == LS_EMPTY) { str_cpy(b, l->waiting ? "WAITING" : "EMPTY", 12); return; }
    if (l->state == LS_RECORDING) {
        str_cpy(b, "REC ", 12);
        fmt_int(b + 4, (int32_t)l->bars4 / 4);
        if (l->set) { str_cpy(b + str_len(b), "/", 2); fmt_int(b + str_len(b), (int32_t)l->set); }
        return;
    }
    if (l->state == LS_STOPPED) { str_cpy(b, "STOPPED", 12); return; }
    if (l->state == LS_OVERDUBBING) str_cpy(b, "DUB", 12);
    else { fmt_int(b, (int32_t)l->bars4 / 4); str_cpy(b + str_len(b), l->bars4 / 4 == 1u ? " BAR" : " BARS", 6); }
    if (l->layers > 1u) { str_cpy(b + str_len(b), " +", 3); fmt_int(b + str_len(b), (int32_t)l->layers - 1); }
}
/* dots along a circle from angle a0 to a1 (0..128 a turn from the top, clockwise), every fourth left out dashed */
static void ring_arc(int32_t cx, int32_t cy, int32_t r, uint32_t a0, uint32_t a1, uint16_t c, int32_t d, int dashed)
{
    uint32_t a;
    for (a = a0; a < a1; a++) {
        uint32_t ph = a << 25;
        if (dashed && (a & 4u))
            continue;
        cv_disc(cx + ((r * sine_i(ph)) >> 15), cy - ((r * sine_i(ph + (1u << 30))) >> 15), d, c, 0);
    }
}
/* the pedal: the two loops as rings (the loop going round in the track's colour, red while it records or overdubs,
 * dashed while it waits or is armed, dim while stopped), their bars in the middle, the loop in focus outlined, under
 * each what its key does (REC: the focus; OCT- / OCT+: the other), what it hears as a bar */
static void graph_loop(uint16_t c)
{
    const aafont_t *tf = TY_TX, *lf = TY_LB;
    uint32_t i;
    char b[16];
    for (i = 0; i < 2u; i++) {
        const loop_t *l = &loopm.loop[i];
        int32_t x0 = (int32_t)i * 118 + 2, cx = x0 + 58, cy = 70, r = 23;
        int focus = (loopm.focus & 1u) == i, busy = l->state == LS_RECORDING || l->state == LS_OVERDUBBING;
        int waits = l->state == LS_EMPTY && (l->waiting || l->armed);
        uint16_t lc = busy || waits ? T_REC : l->state == LS_PLAYING ? c : T_DIM, bg = T_SURF;
        if (focus) {
            cv_rrect(x0, 1, 116, H_GRAPH - 2, 8, c, T_SURF);
            cv_rrect(x0 + 1, 2, 114, H_GRAPH - 4, 7, T_SURF, c);
        }
        str_cpy(b, "LOOP ", 8);
        b[5] = (char)('1' + i);
        b[6] = 0;
        cv_text_on(x0 + 8, ty_y(11, lf), lf, b, c, bg);
        loop_word(l, b);
        cv_free_text(x0 + 8, ty_y(27, lf), lf, b, busy || waits ? T_REC : l->state == LS_PLAYING ? c : T_MID, bg, 100);
        ring_arc(cx, cy, r, 0, 128, T_RAISE, 5, 0);
        if (waits)
            ring_arc(cx, cy, r, 0, 128, lc, 5, 1);
        else if (l->state != LS_EMPTY)
            ring_arc(cx, cy, r, 0, l->state == LS_STOPPED ? 128u : (uint32_t)l->pos * 128u / 127u + 1u, lc, 5, 0);
        if (l->state == LS_EMPTY) str_cpy(b, "--", 3);
        else { fmt_int(b, (int32_t)l->bars4 / 4); if (l->state == LS_RECORDING && l->set) { str_cpy(b + str_len(b), "/", 2); fmt_int(b + str_len(b), (int32_t)l->set); } }
        cv_text_c(cx, ty_y(cy, tf), tf, b, l->state == LS_EMPTY ? T_DIM : T_TEXT, bg);
        {   /* under it: what the key does (the focus: REC; the other: OCT- / OCT+ picks it) */
            uint32_t id = focus ? KC_REC : i ? KC_OCTUP : KC_OCTDN;
            const char *w = focus ? LOOP_KEY_WORD[l->key & 3u] : "PICK";
            int32_t x = cx - kh_w(id, w) / 2;
            cv_text_on(cv_keycap(x, 104 - KC_PH / 2, id, focus ? T_KEY : T_RAISE, focus ? T_INK : T_MID, bg) + KH_GAP,
                       ty_y(104, lf), lf, w, focus ? T_TEXT : T_MID, bg);
        }
        cv_rrect(cx - 30, 117, 60, 3, 1, T_RAISE, bg);
        if (l->hear)
            cv_rrect(cx - 30, 117, (int32_t)l->hear * 60 / 127 < 3 ? 3 : (int32_t)l->hear * 60 / 127, 3, 1, c, T_RAISE);
    }
}

/* ------------------------------------------------------ the meter --- */
/* a take's source level under a wave while Ellic Studio tells it (METER) */
static void graph_meter(uint16_t c)
{
    int32_t w;
    if (!meter_fresh())
        return;
    w = (int32_t)meter.level * 216 / 127;
    cv_rrect(8, H_GRAPH - 7, 224, 4, 2, T_RAISE, T_SURF);
    if (w > 0)
        cv_rrect(8, H_GRAPH - 7, w < 4 ? 4 : w, 4, 2, c, T_RAISE);
}
