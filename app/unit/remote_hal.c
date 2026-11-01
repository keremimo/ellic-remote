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
static uint32_t hal_touched_ms;          /* a key, a button or a knob last moved (the LEDs follow it at once) */
static void hal_poll(void)
{
    static uint32_t buttons, keys;
    uint32_t b = plat_buttons(), k = plat_keys() & 0x7FFFFFFu, m = 0, i, t = fm1_ticks();
    if (b != buttons || k != keys)
        hal_touched_ms = plat_ms();
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
        if (s) {
            fm1_in.enc_steps[hal_enc[i]] = (int16_t)(fm1_in.enc_steps[hal_enc[i]] + s * hal_dir[i]);
            hal_touched_ms = plat_ms();
        }
    }
}

/* -------------------------------------------- the LEDs out, by label --- */
/* Melodee FM-1's picture (fm1_led: lit; fm1_led_dim's three planes, each lit on the frames fm1_led_dim_mask leaves: the
 * keys' glow at the LIGHTS level, the idle buttons' quarter, the breathing) read back by key id and given to fm1-base by
 * label as it is: lit, and the planes with their masks (the scan's plat_lights_planes: LIGHTS FULL is lit, LOW .. HIGH
 * an eighth .. a half). A pass of its own over the picture as the pages left it: a light going darker (lit to a plane,
 * a plane to a fainter one or dark) holds HAL_HOLD_MS first unless the panel moved meanwhile, so a picture the mirror
 * leaves half told for a frame never blinks; brighter shows at once, the breathing as it breathes */
#define HAL_NLED (14u + 27u)
#define HAL_HOLD_MS 24u
static uint8_t hal_led_pos[HAL_NLED];    /* (column << 3 | row bit) of each light, buttons by label then keys; 0xFF none */
static uint8_t hal_shown[HAL_NLED];      /* what each shows: bit 0 lit, 1 and 2 the planes 0, 1 (the simulator checks it) */
static uint32_t hal_level(uint32_t bits) /* how bright: lit 256, a plane 256 / (its mask + 1) */
{
    uint32_t v = 0, j;
    if (bits & 1u)
        return 256u;
    for (j = 0; j < 2u; j++)
        if ((bits >> (j + 1u)) & 1u && 256u / (fm1_led_dim_mask[j] + 1u) > v)
            v = 256u / (fm1_led_dim_mask[j] + 1u);
    return v;
}
static void hal_lights(void)
{
    static uint8_t ready;
    static uint32_t since[HAL_NLED], last[8];
    uint32_t i, now = plat_ms(), local = now - hal_touched_ms < 2u * HAL_HOLD_MS;
    uint32_t b[4] = {0, 0, 0, 0}, k[4] = {0, 0, 0, 0};
    uint8_t mask[3];
    if (!ready) {
        for (i = 0; i < HAL_NLED; i++) {
            uint32_t p, r, id = i < 14u ? hal_btn[i] : i;
            hal_led_pos[i] = 0xFFu;
            for (p = 0; p < FM1_NCOL; p++)
                for (r = 1; r < 5u; r++)
                    if (FM1_KEYMAP[r][p] == (int8_t)id)
                        hal_led_pos[i] = (uint8_t)(p << 3 | r);
        }
        ready = 1;
    }
    for (i = 0; i < HAL_NLED; i++) {
        uint32_t q = hal_led_pos[i], c, bit, want, j;
        if (q == 0xFFu)
            continue;
        c = q >> 3;
        bit = q & 7u;
        want = (fm1_led[c] >> bit & 1u) | (fm1_led_dim[0][c] >> bit & 1u) << 1 | (fm1_led_dim[1][c] >> bit & 1u) << 2;
        if (want != hal_shown[i]) {
            if (local || hal_level(want) >= hal_level(hal_shown[i]) || (since[i] && now - since[i] >= HAL_HOLD_MS)) {
                hal_shown[i] = (uint8_t)want;
                since[i] = 0;
            } else if (!since[i]) {
                since[i] = now | 1u;              /* (going darker: held a moment) */
            }
        } else {
            since[i] = 0;
        }
        want = hal_shown[i] | (fm1_led_dim[2][c] >> bit & 1u) << 3;   /* (the breathing: as it breathes) */
        for (j = 0; j < 4u; j++)
            if ((want >> j) & 1u) {
                if (i < 14u)
                    b[j] |= 1u << i;
                else
                    k[j] |= 1u << (i - 14u);
            }
    }
    mask[0] = fm1_led_dim_mask[0];
    mask[1] = fm1_led_dim_mask[1];
    mask[2] = fm1_led_dim_mask[2];
    {
        uint32_t pic[8] = {b[0], k[0], b[1], k[1], b[2], k[2], b[3] | (uint32_t)mask[0] << 16 | (uint32_t)mask[1] << 24,
                           k[3] | (uint32_t)mask[2] << 27};
        if (!memcmp(pic, last, sizeof pic))
            return;
        memcpy(last, pic, sizeof pic);
    }
    plat_lights(b[0], k[0], 0, 0, 0);
    plat_lights_planes(b + 1, k + 1, mask);
}
