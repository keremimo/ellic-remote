/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's HAL as its pages and controls call it (its hal/fm1_input.h, lcd.c and the clock), on fm1-base:
 * the panel by printed label both ways (each FM-1's own calibration is fm1-base's), the LCD through app.c. */

/* the matrix: key id at (column, packed row bit); ids 0..13 the buttons, 14..40 the note keys (Melodee FM-1's
 * FM1_KEYMAP, the panel's wiring) */
#define FM1_NCOL 11u
#define FM1_NKEY 41u
#define FM1_NENC 7u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
#define FM1_TICKS_PER_US 1u      /* fm1_ticks: microseconds */

/* what the scan left (fm1_input.h): the debounced buttons by matrix id and keys, their edges, the knobs' detents
 * by matrix encoder. hal_poll fills them from fm1-base's, once a pass of the main loop */
static volatile struct {
    uint32_t notes, buttons;
    uint32_t pressed, released, notes_pressed;
    uint32_t note_t0[27];
    int16_t enc_steps[FM1_NENC];
} fm1_in;
static struct { uint32_t press_n, press_sum, press_max, kb_n, kb_sum, kb_max, dac_sum, dac_max; } fm1_in_stat;
static uint8_t fm1_led[FM1_NCOL];
static uint8_t fm1_led_dim[3][FM1_NCOL];
static uint8_t fm1_led_dim_mask[3];

static uint32_t fm1_ticks(void) { return plat_ms() * 1000u; }
static void fm1_wdt_feed(void) {}
static void fm1_irq_off(void) {}         /* (no audio interrupt shares the model here) */
static void fm1_irq_on(void) {}
static uint32_t fm1_icfg(void) { return 0; }
static void fm1_icfg_set(uint32_t v) { (void)v; }
static void fm1_delay_ms(uint32_t ms) { uint32_t t = plat_ms(); while (plat_ms() - t < ms) plat_yield(); }

static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = e < FM1_NENC ? fm1_in.enc_steps[e] : 0;
    if (e < FM1_NENC)
        fm1_in.enc_steps[e] = 0;
    return s;
}
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = fm1_in.pressed;
    if (released)
        *released = fm1_in.released;
    fm1_in.pressed = fm1_in.released = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void)
{
    uint32_t p = fm1_in.notes_pressed;
    fm1_in.notes_pressed = 0;
    return p;
}
static void fm1_led_key(uint32_t id, int on)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                if (on)
                    fm1_led[p] |= (uint8_t)(1u << r);
                else
                    fm1_led[p] &= (uint8_t)~(1u << r);
            }
}

/* the LCD: fm1-base's (and its mirror's) */
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { app_lcd_fill(x, y, w, h, c); }
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p) { app_lcd_blit(x, y, w, h, p); }
static void lcd_sync(void) { app_lcd_sync(); }

static struct {
    uint32_t stage, page, home, ui_frames;
} melodee_dbg;

/* --------------------------------------------- fm1-base's panel in --- */
/* Melodee FM-1's panel map (panel.c) is defined below this file: these read it through a pointer filled at start */
static const uint8_t *hal_btn;           /* matrix id of each label (panel.btn) */
static const uint8_t *hal_enc;           /* matrix encoder of each role (panel.enc) */
static const int8_t *hal_dir;
static void hal_poll(void)
{
    static uint32_t buttons, keys;
    uint32_t b = plat_buttons(), k = plat_keys() & 0x7FFFFFFu, m = 0, i, t = fm1_ticks();
    for (i = 0; i < 14u; i++)
        if ((b >> i) & 1u)
            m |= 1u << hal_btn[i];
    fm1_in.pressed |= m & ~fm1_in.buttons;
    fm1_in.released |= fm1_in.buttons & ~m;
    fm1_in.buttons = m;
    for (i = 0; i < 27u; i++)
        if ((k & ~keys) >> i & 1u)
            fm1_in.note_t0[i] = t;
    fm1_in.notes_pressed |= k & ~keys;
    fm1_in.notes = k;
    keys = k;
    buttons = b;
    for (i = 0; i < 7u; i++) {
        int32_t s = plat_enc((int)i);
        if (s)
            fm1_in.enc_steps[hal_enc[i]] = (int16_t)(fm1_in.enc_steps[hal_enc[i]] + s * hal_dir[i]);
    }
}

/* -------------------------------------------- the LEDs out, by label --- */
/* Melodee FM-1's picture (fm1_led: lit; fm1_led_dim's planes: the keys' and the buttons' glow, the breathing) read
 * back by key id and given to fm1-base by label: lit, its glow (dim), the breathing (its mid level) */
static void hal_lights(void)
{
    uint32_t i, id, lit_b = 0, lit_k = 0, dim_b = 0, dim_k = 0, mid_b = 0, mid_k = 0;
    static uint32_t last[6];
    for (i = 0; i < 14u + 27u; i++) {
        uint32_t p, r, q = 0xFFu, on, dim, mid;
        id = i < 14u ? hal_btn[i] : i;
        for (p = 0; p < FM1_NCOL && q == 0xFFu; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    q = p << 3 | r;
        if (q == 0xFFu)
            continue;
        on = fm1_led[q >> 3] >> (q & 7u) & 1u;
        dim = (fm1_led_dim[0][q >> 3] | fm1_led_dim[1][q >> 3]) >> (q & 7u) & 1u;
        mid = fm1_led_dim[2][q >> 3] >> (q & 7u) & 1u;
        if (i < 14u) {
            lit_b |= on << i;
            dim_b |= dim << i;
            mid_b |= mid << i;
        } else {
            lit_k |= on << (i - 14u);
            dim_k |= dim << (i - 14u);
            mid_k |= mid << (i - 14u);
        }
    }
    {
        uint32_t now[6] = {lit_b, lit_k, dim_b, dim_k, mid_b, mid_k};
        if (!memcmp(now, last, sizeof now))
            return;
        memcpy(last, now, sizeof now);
    }
    /* the keys' glow at Melodee FM-1's LIGHTS level (its plane 0's duty: 0 full, else less) */
    plat_lights(lit_b, lit_k, dim_b, dim_k, fm1_led_dim_mask[0] == 0u ? 2 : 1);
    plat_lights_mid(mid_b, mid_k);
}
