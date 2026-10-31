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

/* a text: its length, then its characters (printable ASCII; others shown as '?'), cut to max */
static void rd_text(mel_rd_t *r, char *d, uint32_t max)
{
    uint32_t len = rd_u7(r), k, o = 0;
    for (k = 0; k < len && !r->bad; k++) {
        uint32_t c = rd_u7(r);
        if (o < max)
            d[o++] = (char)(c >= 0x20u && c < 0x7Fu ? c : '?');
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
                  uint32_t keys_flags)
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
