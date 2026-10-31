/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The protocol's reading and writing (mel_proto.h, docs/PROTOCOL.md). No hardware here: the app includes it in the
 * unity build, tests/proto_test.c compiles it alone. */
#include "mel_proto.h"

static void mel_blank(mel_screen_t *s)
{
    uint8_t *p = (uint8_t *)s, ch = s->keys_ch, flags = s->keys_flags;
    uint32_t i;
    for (i = 0; i < sizeof *s; i++)
        p[i] = 0;
    s->keys_ch = ch;
    s->keys_flags = flags;
    s->octave = MEL_OCT_SAME;
    s->body = MEL_BODY_TEXT;
    s->dirty = MEL_DIRTY_ALL;
    s->v2.octave = 64;
    s->v2.shown = 1;
    s->v2.trans = 64;
    for (i = 0; i < MEL_PLACES; i++) {
        s->v2.trk[i].len = 16;
        s->v2.trk[i].ph = MEL_PH_NONE;
    }
    s->v2.grid_pages = 1;
    s->v2.dirty = MEL_V2_ALL;
}

void mel_reset(mel_screen_t *s) { mel_blank(s); }

int mel_ours(const uint8_t *p, uint32_t n)
{
    return n >= 4u && p[0] == MEL_ID0 && p[1] == MEL_ID1 && p[2] == MEL_ID2;
}

/* ------------------------------------------------------------- reading --- */
typedef struct {
    const uint8_t *p;
    uint32_t n, i;
    int bad;                             /* read past the end, or a byte with its top bit set */
} mel_rd_t;

static uint32_t rd_u7(mel_rd_t *r)
{
    uint8_t b;
    if (r->i >= r->n) {
        r->bad = 1;
        return 0;
    }
    b = r->p[r->i++];
    if (b & 0x80u)
        r->bad = 1;
    return b & 0x7Fu;
}

static uint32_t rd_bits(mel_rd_t *r, uint32_t bytes)   /* seven bits a byte, the lowest first */
{
    uint32_t v = 0, k;
    for (k = 0; k < bytes; k++)
        v |= rd_u7(r) << (7u * k);
    return v;
}

static uint16_t rd_colour(mel_rd_t *r)
{
    uint32_t cr = rd_u7(r), cg = rd_u7(r), cb = rd_u7(r);
    return mel_rgb7(cr, cg, cb);
}

/* version 2's few letters beyond ASCII, as codes 1..10 in a text (the screen's faces have them: Melodee FM-1's
 * tools/gen_aa_font.py EXTRAS): middle dot, degree, ellipsis, copyright, A O U a o u umlaut */
static const uint8_t MEL_EXTRA[11] = {'?', 0xB7, 0xB0, 0x85, 0xA9, 0xC4, 0xD6, 0xDC, 0xE4, 0xF6, 0xFC};

/* a text: its length, then its characters (printable ASCII, and codes 1..10 for MEL_EXTRA's; others shown as '?'), cut
 * to max */
static void rd_text(mel_rd_t *r, char *d, uint32_t max)
{
    uint32_t len = rd_u7(r), k, o = 0;
    for (k = 0; k < len && !r->bad; k++) {
        uint32_t c = rd_u7(r);
        if (o < max)
            d[o++] = (char)(c >= 0x20u && c < 0x7Fu ? c : c && c <= 10u ? MEL_EXTRA[c] : '?');
    }
    d[o] = 0;
}

static void text_copy(char *d, const char *s)
{
    while ((*d++ = *s++))
        ;
}

static int text_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a byte kept when it changed: 1 */
static int byte_keep(uint8_t *d, uint32_t v)
{
    if (*d == (uint8_t)v)
        return 0;
    *d = (uint8_t)v;
    return 1;
}

/* a text kept when it changed: 1 */
static int text_keep(char *d, const char *t)
{
    if (text_same(d, t))
        return 0;
    text_copy(d, t);
    return 1;
}

static int bytes_keep(void *d, const void *t, uint32_t n)
{
    uint8_t *a = d;
    const uint8_t *b = t;
    uint32_t i, same = 1;
    for (i = 0; i < n; i++)
        same &= a[i] == b[i];
    for (i = 0; !same && i < n; i++)
        a[i] = b[i];
    return !same;
}

/* version 2's messages (docs/PROTOCOL.md "Version 2"): each sets its part of s->v2 and marks it when it changed. The
 * command it was, or 0 (malformed: nothing changes) */
static int apply_v2(mel_screen_t *s, uint32_t cmd, mel_rd_t *r)
{
    mel_v2_t *v = &s->v2;
    uint32_t i, k, d = 0;
    switch (cmd) {
    case MEL_PAGE: {
        uint32_t pg = rd_u7(r), f = rd_u7(r);
        if (r->bad || pg >= MEL_NPAGE)
            return 0;
        if (pg != v->page || f != v->page_flags)
            d = MEL_V2_ALL;              /* (another page: all of it drawn again) */
        v->page = (uint8_t)pg;
        v->page_flags = (uint8_t)f;
        break;
    }
    case MEL_HEAD: {
        uint8_t h[7];
        char bpm[MEL_BPM_N + 1], chip[MEL_CHIP_N + 1], st[MEL_STATUS_N + 1];
        for (i = 0; i < 7u; i++)
            h[i] = (uint8_t)rd_u7(r);
        rd_text(r, bpm, MEL_BPM_N);
        rd_text(r, chip, MEL_CHIP_N);
        rd_text(r, st, MEL_STATUS_N);
        if (r->bad)
            return 0;
        if (bytes_keep(&v->transport, h, 7) | text_keep(v->bpm, bpm) | text_keep(v->chip, chip) | text_keep(v->status, st))
            d = MEL_V2_HEAD;
        break;
    }
    case MEL_TRACK: {
        mel_track_t t;
        uint32_t pl = rd_u7(r);
        if (pl >= MEL_PLACES)
            return 0;
        t = v->trk[pl];
        t.numeral = (uint8_t)rd_u7(r);
        t.flags = (uint8_t)rd_u7(r);
        t.colour = rd_colour(r);
        rd_text(r, t.pattern, MEL_PAT_N);
        rd_text(r, t.next, MEL_PAT_N);
        rd_text(r, t.name, MEL_SOUND_N);
        rd_text(r, t.role, MEL_ROLE_N);
        rd_text(r, t.extra, MEL_EXTRA_N);
        if (r->bad || t.numeral > 99u)
            return 0;
        if (bytes_keep(&v->trk[pl], &t, sizeof t))
            d = MEL_V2_TRACKS;
        break;
    }
    case MEL_STEPS: {
        uint8_t st[MEL_NSTEP / 4];
        uint32_t pl = rd_u7(r), len = rd_u7(r);
        for (i = 0; i < sizeof st; i++)
            st[i] = 0;
        for (i = 0; i < (len + 2u) / 3u && !r->bad; i++) {   /* three steps a byte, the first the lowest */
            uint32_t b = rd_u7(r);
            for (k = 0; k < 3u && 3u * i + k < MEL_NSTEP; k++) {
                uint32_t j = 3u * i + k, v2 = (b >> (2u * k)) & 3u;
                if (j < len)
                    st[j >> 2] |= (uint8_t)((v2 == 3u ? 0u : v2) << (2u * (j & 3u)));
            }
        }
        if (r->bad || pl >= MEL_PLACES || !len || len > MEL_NSTEP)
            return 0;
        if (bytes_keep(v->trk[pl].steps, st, sizeof st) | byte_keep(&v->trk[pl].len, len))
            d = MEL_V2_STEPS;
        break;
    }
    case MEL_PLAY: {
        uint8_t ph[MEL_PLACES];
        for (i = 0; i < MEL_PLACES; i++) {
            ph[i] = (uint8_t)rd_u7(r);
            if (ph[i] >= MEL_NSTEP)
                ph[i] = MEL_PH_NONE;
        }
        if (r->bad)
            return 0;
        for (i = 0; i < MEL_PLACES; i++)
            if (v->trk[i].ph != ph[i]) {
                v->trk[i].ph = ph[i];
                d = MEL_V2_PLAY;
            }
        break;
    }
    case MEL_SEL: {
        uint32_t sel = rd_u7(r), shown = rd_u7(r);
        if (r->bad || sel >= MEL_PLACES || !shown || shown > MEL_PLACES)
            return 0;
        if (sel != v->sel || shown != v->shown)
            d = MEL_V2_SEL | MEL_V2_TRACKS | MEL_V2_STEPS | MEL_V2_CARDS | MEL_V2_FOOT | MEL_V2_NOTES;
        v->sel = (uint8_t)sel;
        v->shown = (uint8_t)shown;
        break;
    }
    case MEL_NOTES: {
        uint8_t nt[16], of[16];
        uint32_t n = rd_u7(r);
        for (i = 0; i < n && !r->bad; i++) {
            uint32_t note = rd_u7(r), pl = rd_u7(r);
            if (i < 16u) {
                nt[i] = (uint8_t)note;
                of[i] = (uint8_t)(pl < MEL_PLACES ? pl : 0u);
            }
        }
        if (r->bad || n > 16u)
            return 0;
        if (byte_keep(&v->nnotes, n) | bytes_keep(v->note, nt, n) | bytes_keep(v->note_of, of, n))
            d = MEL_V2_NOTES;
        break;
    }
    case MEL_METER:
        for (i = 0; i < MEL_PLACES; i++) {
            uint32_t m = rd_u7(r);
            if (!r->bad && v->trk[i].meter != m) {
                v->trk[i].meter = (uint8_t)m;
                d = MEL_V2_METER;
            }
        }
        if (r->bad)
            return 0;
        break;
    case MEL_SCOPE: {
        uint8_t sc[120];
        uint32_t n = rd_u7(r);
        for (i = 0; i < n && !r->bad; i++) {
            uint32_t x = rd_u7(r);
            if (i < sizeof sc)
                sc[i] = (uint8_t)x;
        }
        if (r->bad || n > sizeof sc)
            return 0;
        v->nscope = (uint8_t)n;
        for (i = 0; i < n; i++)
            v->scope[i] = sc[i];
        d = MEL_V2_SCOPE;                /* (a new trace: drawn again, as the FM-1's scope moves) */
        break;
    }
    case MEL_CARD: {
        mel_card_t c;
        uint32_t kn = rd_u7(r);
        c.flags = (uint8_t)rd_u7(r);
        rd_text(r, c.label, MEL_LABEL_N);
        rd_text(r, c.value, MEL_VAL_N);
        rd_text(r, c.unit, MEL_UNIT_N);
        if (r->bad || kn >= 4u)
            return 0;
        if (bytes_keep(&v->card[kn], &c, sizeof c))
            d = MEL_V2_CARDS;
        break;
    }
    case MEL_LIST: {
        uint8_t l[4];
        for (i = 0; i < 4u; i++)
            l[i] = (uint8_t)rd_u7(r);
        if (r->bad || l[3] > MEL_LP_SCALE)
            return 0;
        if (bytes_keep(&v->list_n, l, 4))
            d = MEL_V2_LIST;
        break;
    }
    case MEL_LROW:
    case MEL_SROW: {
        mel_lrow_t w;
        uint32_t at = rd_u7(r);
        w.flags = (uint8_t)rd_u7(r);
        rd_text(r, w.label, MEL_ROWL_N);
        rd_text(r, w.value, MEL_VAL_N);
        if (r->bad || at >= (cmd == MEL_LROW ? (uint32_t)MEL_LIST_ROWS : (uint32_t)MEL_SHEET_ROWS))
            return 0;
        if (bytes_keep(cmd == MEL_LROW ? &v->lrow[at] : &v->srow[at], &w, sizeof w))
            d = cmd == MEL_LROW ? MEL_V2_LIST : MEL_V2_SHEET;
        break;
    }
    case MEL_SCALE: {
        char nm[MEL_SCALE_N + 1];
        uint32_t q = rd_u7(r), root = rd_u7(r), lo = rd_u7(r), hi = rd_u7(r), tr = rd_u7(r);
        uint16_t mask = (uint16_t)((lo | hi << 7) & 0xFFFu);
        rd_text(r, nm, MEL_SCALE_N);
        if (r->bad || q >= MEL_NQ || root > 11u)
            return 0;
        if (!(mask & 1u))
            mask |= 1u;                  /* (the root is always in its scale) */
        if (byte_keep(&v->q, q) | byte_keep(&v->root, root) | byte_keep(&v->trans, tr) | (v->mask != mask) |
            text_keep(v->scale, nm))
            d = MEL_V2_SCALE;
        v->mask = mask;
        break;
    }
    case MEL_PICTURE: {
        uint8_t pv[6] = {0, 0, 0, 0, 0, 0}, pn = 0, pnote[MEL_PIC_NOTES];
        char t[3][MEL_PIC_TEXT_N + 1];
        uint32_t kind = rd_u7(r), nv = 0, nt = 0;
        t[0][0] = t[1][0] = t[2][0] = 0;
        for (i = 0; i < MEL_PIC_NOTES; i++)
            pnote[i] = 0;
        switch (kind) {                  /* (how many values, notes and texts each kind has) */
        case MEL_PIC_FILTER: nv = 2; break;
        case MEL_PIC_ENV: nv = 4; break;
        case MEL_PIC_ARP: nv = 2; pn = 1; break;
        case MEL_PIC_FX: nv = 6; break;
        case MEL_PIC_CHORD: nv = 1; pn = 1; nt = 2; break;
        case MEL_PIC_LINES: nt = 3; break;
        }
        if (kind >= MEL_NPIC)
            return 0;
        for (i = 0; i < nv; i++)
            pv[i] = (uint8_t)rd_u7(r);
        if (pn) {
            pn = (uint8_t)rd_u7(r);
            for (i = 0; i < pn && !r->bad; i++) {
                uint32_t x = rd_u7(r);
                if (i < MEL_PIC_NOTES)
                    pnote[i] = (uint8_t)x;
            }
            if (pn > MEL_PIC_NOTES)
                r->bad = 1;
        }
        for (i = 0; i < nt; i++)
            rd_text(r, t[i], MEL_PIC_TEXT_N);
        if (r->bad)
            return 0;
        if (byte_keep(&v->pic, kind) | bytes_keep(v->pic_v, pv, sizeof pv) | byte_keep(&v->pic_n, pn) |
            bytes_keep(v->pic_note, pnote, sizeof pnote) | bytes_keep(v->pic_text, t, sizeof t))
            d = MEL_V2_PICTURE;
        break;
    }
    case MEL_FOOT: {
        char a[MEL_FOOT_N + 1], b[MEL_RIGHT_N + 1];
        uint32_t kind = rd_u7(r);
        rd_text(r, a, MEL_FOOT_N);
        rd_text(r, b, MEL_RIGHT_N);
        if (r->bad || kind > MEL_FT_NONE)
            return 0;
        if (byte_keep(&v->foot, kind) | text_keep(v->foot_text, a) | text_keep(v->foot_right, b))
            d = MEL_V2_FOOT;
        break;
    }
    case MEL_SHEET: {
        char t[MEL_ROWL_N + 1], sub[MEL_SUB_N + 1];
        uint8_t h[3];
        for (i = 0; i < 3u; i++)
            h[i] = (uint8_t)rd_u7(r);
        rd_text(r, t, MEL_ROWL_N);
        rd_text(r, sub, MEL_SUB_N);
        if (r->bad || h[0] > 1u || h[1] > MEL_SHEET_ROWS || (h[1] && h[2] >= h[1]))
            return 0;
        if (bytes_keep(&v->sheet_on, h, 3) | text_keep(v->sheet_title, t) | text_keep(v->sheet_sub, sub))
            d = MEL_V2_SHEET;
        break;
    }
    case MEL_ASK: {
        char q[MEL_ASK_N + 1], dt[MEL_ASK_N + 1], no[MEL_BUTTON_N + 1], yes[MEL_BUTTON_N + 1];
        uint8_t h[2];
        h[0] = (uint8_t)rd_u7(r);
        h[1] = (uint8_t)rd_u7(r);
        rd_text(r, q, MEL_ASK_N);
        rd_text(r, dt, MEL_ASK_N);
        rd_text(r, no, MEL_BUTTON_N);
        rd_text(r, yes, MEL_BUTTON_N);
        if (r->bad || h[0] > 1u || h[1] > MEL_AI_NONE)
            return 0;
        if (bytes_keep(&v->ask_on, h, 2) | text_keep(v->ask_q, q) | text_keep(v->ask_d, dt) | text_keep(v->ask_no, no) |
            text_keep(v->ask_yes, yes))
            d = MEL_V2_ASK;
        break;
    }
    case MEL_PICK: {                     /* shown, knob, flags, count, the one now, then each: its family, its name */
        char nm[MEL_PICK_ROWS][MEL_PICK_N + 1];
        uint8_t h[5], fam[MEL_PICK_ROWS] = {0, 0, 0, 0, 0};
        for (i = 0; i < 5u; i++)
            h[i] = (uint8_t)rd_u7(r);
        for (i = 0; i < MEL_PICK_ROWS; i++)
            nm[i][0] = 0;
        for (i = 0; i < h[3] && i < MEL_PICK_ROWS && !r->bad; i++) {
            fam[i] = (uint8_t)rd_u7(r);
            rd_text(r, nm[i], MEL_PICK_N);
        }
        if (r->bad || h[0] > 1u || h[1] > 3u || h[3] > MEL_PICK_ROWS || (h[3] && h[4] >= h[3]))
            return 0;
        if (byte_keep(&v->pick_on, h[0]) | byte_keep(&v->pick_knob, h[1]) | byte_keep(&v->pick_flags, h[2]) |
            byte_keep(&v->pick_n, h[3]) | byte_keep(&v->pick_now, h[4]) | bytes_keep(v->pick, nm, sizeof nm) |
            bytes_keep(v->pick_fam, fam, sizeof fam))
            d = MEL_V2_PICK;
        break;
    }
    case MEL_RACK: {
        mel_rack_t k2;
        uint32_t row = rd_u7(r);
        k2.flags = (uint8_t)rd_u7(r);
        k2.amount = (uint8_t)rd_u7(r);
        rd_text(r, k2.tag, MEL_TAG_N);
        rd_text(r, k2.name, MEL_RACKN_N);
        rd_text(r, k2.value, MEL_MIXV_N + 4);
        if (r->bad || row >= 4u)
            return 0;
        if (bytes_keep(&v->rack[row], &k2, sizeof k2))
            d = MEL_V2_RACK;
        break;
    }
    case MEL_MIX: {
        char db[MEL_MIXV_N + 1], pan[MEL_MIXV_N + 1];
        uint32_t pl = rd_u7(r), f = rd_u7(r), lv = rd_u7(r);
        rd_text(r, db, MEL_MIXV_N);
        rd_text(r, pan, MEL_MIXV_N);
        if (r->bad || pl >= MEL_PLACES)
            return 0;
        if (byte_keep(&v->trk[pl].mix_flags, f) | byte_keep(&v->trk[pl].mix_level, lv) |
            text_keep(v->trk[pl].mix_db, db) | text_keep(v->trk[pl].mix_pan, pan))
            d = MEL_V2_MIX;
        break;
    }
    case MEL_GRID: {
        uint8_t c[MEL_GRID_COLS][4];
        uint32_t pl = rd_u7(r), f = rd_u7(r);
        for (i = 0; i < MEL_GRID_COLS; i++)
            for (k = 0; k < 4u; k++) {
                c[i][k] = (uint8_t)rd_u7(r);
                if (k && c[i][k] > 10u)
                    c[i][k] = 10;
            }
        if (r->bad || pl >= MEL_PLACES)
            return 0;
        if (byte_keep(&v->trk[pl].grid_flags, f) | bytes_keep(v->trk[pl].cell, c, sizeof c))
            d = MEL_V2_GRID;
        break;
    }
    case MEL_GRIDTOP: {                  /* the page, the pages, the section lit, flags, the sections' letters */
        char song[MEL_SONG_N + 1];
        uint32_t pg = rd_u7(r), pgs = rd_u7(r), cur = rd_u7(r), f = rd_u7(r);
        rd_text(r, song, MEL_SONG_N);
        if (r->bad || !pgs || pgs > 2u || pg >= pgs)
            return 0;
        if (byte_keep(&v->grid_page, pg) | byte_keep(&v->grid_pages, pgs) | byte_keep(&v->song_cur, cur) |
            byte_keep(&v->grid_flags, f) | text_keep(v->song, song))
            d = MEL_V2_GRID;
        for (i = 0; v->song[i]; i++)
            ;
        v->song_n = (uint8_t)i;
        break;
    }
    case MEL_SCALES: {
        char fam[MEL_FAMILY_N + 1];
        uint32_t rank = rd_u7(r), total = rd_u7(r);
        rd_text(r, fam, MEL_FAMILY_N);
        if (r->bad)
            return 0;
        if (byte_keep(&v->sc_rank, rank) | byte_keep(&v->sc_total, total) | text_keep(v->sc_family, fam))
            d = MEL_V2_SCALES;
        break;
    }
    case MEL_SCROW: {
        mel_scrow_t w;
        uint32_t at = rd_u7(r);
        w.shade = (uint8_t)(rd_u7(r) & 3u);
        w.flags = (uint8_t)rd_u7(r);
        rd_text(r, w.name, MEL_SCALE_N);
        rd_text(r, w.detail, MEL_DETAIL_N);
        if (r->bad || at >= 5u)
            return 0;
        if (bytes_keep(&v->scrow[at], &w, sizeof w))
            d = MEL_V2_SCALES;
        break;
    }
    case MEL_LANES: {
        char ln[MEL_LANES_N][MEL_LANE_N + 1];
        uint32_t n = rd_u7(r);
        for (i = 0; i < MEL_LANES_N; i++)
            ln[i][0] = 0;
        for (i = 0; i < n && i < MEL_LANES_N && !r->bad; i++)
            rd_text(r, ln[i], MEL_LANE_N);
        if (r->bad || n > MEL_LANES_N)
            return 0;
        if (byte_keep(&v->nlanes, n) | bytes_keep(v->lane, ln, sizeof ln))
            d = MEL_V2_LANES;
        break;
    }
    default:
        return 0;
    }
    v->dirty |= d;                       /* (bytes past its body: a later version's, left alone) */
    return (int)cmd;
}

int mel_apply(mel_screen_t *s, const uint8_t *p, uint32_t n)
{
    mel_rd_t r = {p, n, 4, 0};
    uint32_t cmd;
    if (!mel_ours(p, n))
        return 0;
    cmd = p[3];
    switch (cmd) {
    case MEL_HELLO:                      /* (its version, if any: this end answers with its own) */
        return MEL_HELLO;
    case MEL_BYE:
    case MEL_CLEAR:
        mel_blank(s);
        return (int)cmd;
    case MEL_TITLE: {
        char t[MEL_TITLE_N + 1];
        uint16_t c = rd_colour(&r);
        rd_text(&r, t, MEL_TITLE_N);
        if (r.bad)
            return 0;
        if (c != s->colour || !text_same(t, s->title)) {
            s->colour = c;
            text_copy(s->title, t);
            s->dirty |= MEL_DIRTY_HEAD | MEL_DIRTY_FOOT | MEL_DIRTY_BODY;   /* the colour runs through them all */
        }
        return MEL_TITLE;
    }
    case MEL_INFO: {
        char t[MEL_INFO_N + 1];
        uint32_t tr = rd_u7(&r);
        rd_text(&r, t, MEL_INFO_N);
        if (r.bad)
            return 0;
        if (tr != s->transport || !text_same(t, s->info)) {
            s->transport = (uint8_t)tr;
            text_copy(s->info, t);
            s->dirty |= MEL_DIRTY_HEAD;
        }
        return MEL_INFO;
    }
    case MEL_BODY: {
        uint32_t b = rd_u7(&r);
        if (r.bad || b >= MEL_NBODY)
            return 0;
        if (b != s->body) {
            s->body = (uint8_t)b;
            s->dirty |= MEL_DIRTY_BODY;
        }
        return MEL_BODY;
    }
    case MEL_ROW: {
        mel_row_t w;
        uint32_t i = rd_u7(&r);
        w.flags = (uint8_t)rd_u7(&r);
        w.colour = rd_colour(&r);
        rd_text(&r, w.text, MEL_ROW_N);
        rd_text(&r, w.value, MEL_VALUE_N);
        if (r.bad || i >= MEL_ROWS)
            return 0;
        s->row[i] = w;
        if (s->body == MEL_BODY_LIST)
            s->dirty |= MEL_DIRTY_BODY;
        return MEL_ROW;
    }
    case MEL_LINE: {
        char t[MEL_LINE_N + 1];
        uint32_t i = rd_u7(&r);
        rd_text(&r, t, MEL_LINE_N);
        if (r.bad || i >= MEL_LINES)
            return 0;
        text_copy(s->line[i], t);
        if (s->body == MEL_BODY_TEXT)
            s->dirty |= MEL_DIRTY_BODY;
        return MEL_LINE;
    }
    case MEL_CELL: {
        mel_cell_t w;
        uint32_t i = rd_u7(&r);
        w.flags = (uint8_t)rd_u7(&r);
        w.pos = (uint8_t)rd_u7(&r);
        w.colour = rd_colour(&r);
        rd_text(&r, w.name, MEL_NAME_N);
        rd_text(&r, w.value, MEL_VALUE_N);
        if (r.bad || i >= MEL_CELLS + MEL_KNOBS)
            return 0;
        s->cell[i] = w;
        if (i >= MEL_CELLS)
            s->dirty |= MEL_DIRTY_FOOT;
        else if (s->body == MEL_BODY_CELLS)
            s->dirty |= MEL_DIRTY_BODY;
        return MEL_CELL;
    }
    case MEL_POPUP: {
        char a[MEL_POPUP_N + 1], b[MEL_POPUP_N + 1];
        uint32_t tenths = rd_u7(&r);
        rd_text(&r, a, MEL_POPUP_N);
        rd_text(&r, b, MEL_POPUP_N);
        if (r.bad)
            return 0;
        text_copy(s->popup[0], a);
        text_copy(s->popup[1], b);
        s->popup_tenths = (uint8_t)tenths;
        s->dirty |= MEL_DIRTY_POPUP;
        return MEL_POPUP;
    }
    case MEL_LIGHTS: {
        uint32_t bl = rd_bits(&r, 3), bd = rd_bits(&r, 3), kl = rd_bits(&r, 4), kd = rd_bits(&r, 4);
        uint32_t f = rd_u7(&r);
        if (r.bad)
            return 0;
        s->btn_lit = bl & 0x7FFFu;       /* 14 buttons and PLAY's green */
        s->btn_dim = bd & 0x7FFFu;
        s->keys_lit = kl & 0x7FFFFFFu;   /* 27 keys */
        s->keys_dim = kd & 0x7FFFFFFu;
        s->lights = (uint8_t)f;
        s->lights_set = 1;
        s->dirty |= MEL_DIRTY_LIGHTS;
        return MEL_LIGHTS;
    }
    case MEL_KEYS: {
        uint32_t ch = rd_u7(&r), f = rd_u7(&r), oct = rd_u7(&r);
        if (r.bad || ch > 15u)
            return 0;
        s->keys_ch = (uint8_t)ch;
        s->keys_flags = (uint8_t)f;
        s->octave = (uint8_t)oct;
        s->dirty |= MEL_DIRTY_KEYS;
        return MEL_KEYS;
    }
    }
    if (cmd >= MEL_PAGE && cmd <= MEL_PICK)
        return apply_v2(s, cmd, &r);
    return 0;
}

int mel_sx_byte(mel_sx_t *a, uint8_t b)
{
    if (b == 0xF0u) {
        a->open = 1;
        a->over = 0;
        a->n = 0;
        return 0;
    }
    if (b == 0xF7u) {
        int whole = a->open && !a->over;
        a->open = 0;
        return whole;
    }
    if (b & 0x80u) {                     /* any other status cuts it short */
        a->open = 0;
        return 0;
    }
    if (a->open) {
        if (a->n < MEL_SX_MAX)
            a->buf[a->n++] = b;
        else
            a->over = 1;
    }
    return 0;
}

/* ------------------------------------------------------------- writing --- */
uint32_t mel_hello(uint8_t *f, uint32_t major, uint32_t minor, uint32_t patch, uint32_t keys_ch, int32_t octave,
                  uint32_t keys_flags, uint32_t gen, uint32_t state)
{
    uint32_t n = 0;
    f[n++] = 0xF0u;
    f[n++] = MEL_ID0;
    f[n++] = MEL_ID1;
    f[n++] = MEL_ID2;
    f[n++] = MEL_HELLO;
    f[n++] = MEL_PROTO;
    f[n++] = (uint8_t)(major & 0x7Fu);
    f[n++] = (uint8_t)(minor & 0x7Fu);
    f[n++] = (uint8_t)(patch & 0x7Fu);
    f[n++] = (uint8_t)(keys_ch & 0x0Fu);
    f[n++] = (uint8_t)((64 + octave) & 0x7F);
    f[n++] = (uint8_t)(keys_flags & 0x7Fu);
    f[n++] = (uint8_t)(gen & 0x7Fu);    /* version 2: the screen's generation, whose it is */
    f[n++] = (uint8_t)((gen >> 7) & 0x7Fu);
    f[n++] = (uint8_t)(state & 0x7Fu);
    f[n++] = 0xF7u;
    return n;
}

uint32_t mel_state(uint8_t *f, uint32_t gen, uint32_t state, uint32_t why)
{
    uint32_t n = 0;
    f[n++] = 0xF0u;
    f[n++] = MEL_ID0;
    f[n++] = MEL_ID1;
    f[n++] = MEL_ID2;
    f[n++] = MEL_STATE;
    f[n++] = (uint8_t)(gen & 0x7Fu);
    f[n++] = (uint8_t)((gen >> 7) & 0x7Fu);
    f[n++] = (uint8_t)(state & 0x7Fu);
    f[n++] = (uint8_t)(why & 0x7Fu);
    f[n++] = 0xF7u;
    return n;
}

uint32_t mel_packets(const uint8_t *f, uint32_t n, uint32_t *pkts, uint32_t max)
{
    uint32_t i = 0, k = 0;
    while (i < n && k < max) {
        uint32_t left = n - i, take = left > 3u ? 3u : left, cin = 4u, j, pkt;
        if (left <= 3u)
            cin = 4u + take;             /* the last: 5, 6 or 7 for one, two or three bytes */
        pkt = cin;
        for (j = 0; j < take; j++)
            pkt |= (uint32_t)f[i + j] << (8u + 8u * j);
        pkts[k++] = pkt;
        i += take;
    }
    return k;
}
