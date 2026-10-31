/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee Remote: the FM-1 as Melodee's remote, over Bluetooth MIDI or USB. Melodee draws the screen and lights the
 * panel (SysEx: docs/PROTOCOL.md): version 1's generic screen, or version 2's mirror of Melodee FM-1's pages, drawn
 * with Melodee FM-1's own drawing (ui/); every key, button, encoder and MASTER goes back to it as MIDI. The FM-1 keeps
 * only the keys' octave, their scale (QNT WHITE, SNAP, ALL: Melodee's key, MEL_SCALE) and its SETUP page (OCT- and
 * OCT+ held) to itself, and a screen of its own while no Melodee is there. No sound: the audio runs silent. Part of the
 * unity build after the platform (device/unity.c, host/plat_host.c): fm1-base's gfx.c and announce.c are in scope;
 * the screen is drawn with Melodee FM-1's gfx.c, its names apart (ui/gfx_names.h). */
#include "mel_proto.c"
#include "fastmath.h"
#include "ui/gfx_names.h"                   /* Melodee FM-1's renderer under names of its own, the UI it draws */
#include "ui/gfx.c"
#include "ui/icons.c"
#include "ui/ui_base.c"
#include "ui/ui_draw.c"
#include "ui/ui_stage.c"
#include "ui/ui_view.c"
#include "ui/ui_pages.c"
#include "ui/ui_list.c"
#include "ui/ui_scales.c"
#include "ui/ui_popup.c"
#include "ui/ui_patterns.c"
#include "ui/ui_layer.c"
#include "ui/ui_generic.c"
#include "ui/ui_frame.c"

#define SET_MAGIC 0x4D4C5231u            /* "MLR1": the settings record; another magic loads defaults */
typedef struct {
    uint32_t magic;
    uint8_t keys_ch;                     /* 0..15: the keys' channel while no Melodee says otherwise */
    int8_t curve;                        /* velocity, touch_curve: -5 .. +5 */
    uint8_t pressure;                    /* poly pressure from the held keys */
    uint8_t ble_off;                     /* Bluetooth switched off in SETUP; 0, the default: on */
    uint8_t theme;                       /* Melodee FM-1's palette (ui/gfx.c), stored as its tagged id */
    uint8_t rsv[3];
} settings_t;
static settings_t set;

#define NOTE_F3 53                       /* key 0 */
#define OCT_MAX 3
#define BOTH_HOLD_MS 1500u               /* OCT- and OCT+ held this long: SETUP */
#define SAY_MS 1300u
#define SILENT_MS 7000u                  /* a Melodee that PINGs and goes this long without a word: let go */
#define KEY_SILENT 0xFFu                 /* a key the scale leaves silent (QNT WHITE's black keys) */

static struct {
    mel_screen_t melodee;                /* what Melodee drew */
    mel_screen_t own;                    /* the FM-1's own: waiting for Melodee, SETUP */
    uint8_t host;                        /* where Melodee is: 0 nowhere, else 1 + PORT_USB / PORT_BLE */
    uint8_t setup;                       /* SETUP is up */
    uint8_t setup_row;                   /* its row last changed */
    uint8_t set_dirty;                   /* SETUP changed the settings: saved as it closes */
    int8_t octave;
    uint32_t keys, btn;                  /* held at the last input */
    uint8_t key_note[NKEYS], key_ch[NKEYS], prs[NKEYS];   /* what each held key sent, its pressure */
    uint32_t key_btn;                    /* held keys that went out as buttons (MEL_K_BUTTONS) */
    uint32_t both_t;                     /* OCT- and OCT+ both down since (0: not both) */
    uint8_t both_used;                   /* that hold has opened or closed SETUP */
    int32_t master_sent;                 /* MASTER as last sent, 0..16383 (-1: never) */
    uint8_t ble_was;                     /* plat_ble_connected at the last frame */
    uint32_t usb_link;                   /* plat_midi_out_link at the last frame */
    uint32_t usb_gone_t;                 /* Melodee on USB and no USB host since (0: there) */
    uint8_t hello_owed;                  /* TO_* still owed a HELLO (the out queue was full) */
    uint32_t popup_until;                /* Melodee's popup goes (0: it stays) */
    char say[2][MEL_POPUP_N + 1];        /* the platform's messages (app_say), over everything for a moment */
    uint32_t say_until;
    uint32_t frames_in;                  /* Melodee's messages taken (the simulator's checks) */
    uint8_t shown_own;                   /* the screen shown is the FM-1's own */
    uint8_t redraw;                      /* MEL_DIRTY_* of the screen shown, beyond its own */
    uint8_t popup_up;                    /* a popup is on the screen now */
    uint16_t gen;                        /* the screen's generation: changes each time Melodee's screen goes unasked */
    uint8_t why;                         /* .. why it last changed (MEL_WHY_*) */
    uint8_t pings;                       /* the ports that PING (TO_*): their silence lets the remote go */
    uint32_t heard_t;                    /* the claiming port's last protocol message */
    uint32_t link_t[2];                  /* each port's link up since (| 1; 0: down): unclaimed, HELLO again now and then */
    uint8_t link_hellos[2];              /* .. HELLOs sent since */
    uint32_t n_hello, n_state;           /* HELLOs and STATEs sent (the simulator's checks) */
    uint8_t redraw_all;                  /* the screen again, whole, at the next frame (app_redraw) */
} mel PLAT_POOL;

static mel_sx_t mel_ble_sx;              /* Bluetooth's SysEx, gathered (outside mel: mel_ble_sysex may come first) */

int mel_ble_off(void) { return set.ble_off; }

/* ----------------------------------------------------------------- audio --- */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i;
    (void)master_q12;
    for (i = 0; i < 2u * n; i++)
        lr[i] = 0;
}

void app_panic(void) {}

/* -------------------------------------------------------------- settings --- */
static void settings_defaults(void)
{
    uint32_t i;
    uint8_t *p = (uint8_t *)&set;
    for (i = 0; i < sizeof set; i++)
        p[i] = 0;
    set.magic = SET_MAGIC;
    set.pressure = 1;
    set.theme = (uint8_t)palette_to_stored(UI_DEFAULT_INDEX);
}

static void settings_save(void)
{
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0)
        mel.set_dirty = 0;
    else
        app_say("SETUP", "NOT SAVED");
}

void app_init(void)
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.keys_ch > 15u || set.curve < -5 || set.curve > 5 || set.pressure > 1u || set.ble_off > 1u)
        settings_defaults();
    if (!palette_stored_ok(set.theme))   /* (version 1's fm1-base themes: NIGHT, Melodee FM-1's own) */
        set.theme = (uint8_t)palette_to_stored(UI_DEFAULT_INDEX);
    mel.melodee.keys_ch = set.keys_ch;
    mel_reset(&mel.melodee);
    mel_reset(&mel.own);
    mel.master_sent = -1;
    mel.gen = (uint16_t)((plat_master() * 2654435761u ^ plat_ms() * 40503u) & 0x3FFFu);   /* (a new run: unlike the last) */
    mel.why = MEL_WHY_POWER;
}

/* ------------------------------------------------------------- MIDI out --- */
static uint32_t mel_to(void)             /* where the panel's MIDI goes: to Melodee, or anyone while it is not there */
{
    if (mel.host == 1u + PORT_USB)
        return TO_USB;
    if (mel.host == 1u + PORT_BLE)
        return TO_BLE;
    return TO_USB | TO_BLE;
}

static void mel_send(uint32_t status, uint32_t d1, uint32_t d2)
{
    plat_midi_send(status >> 4 | status << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24, mel_to());
}

static uint32_t keys_ch(void) { return mel.host ? mel.melodee.keys_ch : set.keys_ch; }
static uint32_t keys_flags(void) { return mel.host ? mel.melodee.keys_flags : 0u; }

/* the version from APP_VERSION ("1.2.3"; a dev build's "DEV" is 0.0.0) */
static uint32_t version_part(int which)
{
    const char *v = APP_VERSION;
    uint32_t n = 0;
    for (; which > 0 && *v; v++)
        if (*v == '.')
            which--;
    while (*v >= '0' && *v <= '9')
        n = n * 10u + (uint32_t)(*v++ - '0');
    return n;
}

/* whose the screen is, to port: MEL_ST_* */
static uint32_t state_for(uint32_t port) { return !mel.host ? MEL_ST_FREE : mel.host == 1u + port ? MEL_ST_YOURS : MEL_ST_TAKEN; }

/* a frame (F0 .. F7) to one port or both: sent whole or not at all (no room in its out queue: 0) */
static uint32_t frame_to(const uint8_t *f, uint32_t n, uint32_t to)
{
    uint32_t pk[8], k, i;
    k = mel_packets(f, n, pk, 8);
    if ((to & TO_BLE) && (!plat_ble_connected() || plat_midi_out_room(TO_BLE) < k))
        to &= ~TO_BLE;
    if ((to & TO_USB) && plat_midi_out_room(TO_USB) < k)
        to &= ~TO_USB;
    for (i = 0; to && i < k; i++)
        plat_midi_send(pk[i], to);
    return to;
}

/* our STATE to a port: the screen's generation, whose it is, why it last changed (PING's answer, or unasked) */
static void state_to(uint32_t port)
{
    uint8_t f[12];
    if (frame_to(f, mel_state(f, mel.gen, state_for(port), mel.why), port == PORT_USB ? TO_USB : TO_BLE))
        mel.n_state++;
}

/* Melodee's screen went without Melodee asking: a new generation, said to the port that had it (it paints again) */
static void gen_bump(uint32_t why, int port)
{
    mel.gen = (uint16_t)((mel.gen + 1u) & 0x3FFFu);
    mel.why = (uint8_t)why;
    if (port >= 0)
        state_to((uint32_t)port);
}

/* our HELLO to one port or both: sent whole or not yet (it waits for room in the out queue) */
static void hello_to(uint32_t to)
{
    uint8_t f[20];
    uint32_t pk[8], n, k, i;
    mel.hello_owed |= (uint8_t)to;
    n = mel_hello(f, version_part(0), version_part(1), version_part(2), keys_ch(), mel.octave, keys_flags(), mel.gen,
                  state_for((to & TO_USB) ? PORT_USB : PORT_BLE));
    k = mel_packets(f, n, pk, 8);
    if ((to & TO_BLE) && plat_ble_connected() && plat_midi_out_room(TO_BLE) < k)
        to &= ~TO_BLE;
    if ((to & TO_USB) && plat_midi_out_room(TO_USB) && plat_midi_out_room(TO_USB) < k)
        to &= ~TO_USB;
    for (i = 0; to && i < k; i++)
        plat_midi_send(pk[i], to);
    mel.n_hello += to != 0u;
    mel.hello_owed &= (uint8_t)~to;
}

static void master_send(int force)       /* MASTER as 14 bits: CC 7, then CC 39 */
{
    int32_t v = (int32_t)(plat_master() * 16383u / 4096u), d;
    if (v > 16383)
        v = 16383;
    d = v - mel.master_sent;
    if (!force && mel.master_sent >= 0 && d < 64 && d > -64 && !((v == 0 || v == 16383) && d))
        return;                          /* a step too small to be a turn (but its ends always go) */
    mel.master_sent = v;
    mel_send(0xB0u | MEL_CH, MEL_CC_MASTER, (uint32_t)v >> 7);
    mel_send(0xB0u | MEL_CH, MEL_CC_MASTER_LSB, (uint32_t)v & 0x7Fu);
}

/* ------------------------------------------------------ Melodee comes, goes --- */
static void mel_let_go(uint32_t why)
{
    if (mel.host) {
        uint32_t p = mel.host - 1u;
        gen_bump(why, why == MEL_WHY_LINK ? -1 : (int)p);
        if (why == MEL_WHY_SILENT) {     /* (gone quiet: greeted again from now, as a link that comes) */
            mel.link_t[p] = plat_ms() | 1u;
            mel.link_hellos[p] = 0;
        } else if (why == MEL_WHY_BYE) { /* (it said goodbye: not greeted again until its link comes again) */
            mel.link_hellos[p] = 5;
        }
    }
    mel.host = 0;
    mel.melodee.keys_ch = set.keys_ch;
    mel.melodee.keys_flags = 0;
    mel_reset(&mel.melodee);
    mel.popup_until = 0;
    mel.redraw = MEL_DIRTY_ALL;
}

static void mel_claim(int port, uint32_t cmd)
{
    if (mel.host && mel.host != 1u + (uint32_t)port)   /* (another's: taken from it, and it is told) */
        gen_bump(MEL_WHY_TAKEN, (int)mel.host - 1);
    mel_reset(&mel.melodee);             /* a new Melodee paints from blank */
    mel.host = (uint8_t)(1 + port);
    mel.usb_gone_t = 0;
    mel.popup_until = 0;
    mel.redraw = MEL_DIRTY_ALL;
    mel.heard_t = plat_ms();
    if (cmd != MEL_HELLO && cmd != MEL_CLEAR)   /* claimed by a stray message: blank but for it; said, so Melodee paints */
        gen_bump(MEL_WHY_STRAY, port);
}

/* a SysEx frame (the bytes between F0 and F7) from a port: 1 when it is the protocol's */
static int mel_frame(const uint8_t *p, uint32_t n, int port)
{
    int cmd;
    if (!mel_ours(p, n))
        return 0;
    cmd = p[3];
    if (cmd == MEL_PING) {               /* (never claims: answered with whose the screen is) */
        mel.pings |= (uint8_t)(port == PORT_USB ? TO_USB : TO_BLE);
        if (mel.host == 1u + (uint32_t)port)
            mel.heard_t = plat_ms();
        state_to((uint32_t)port);
        return 1;
    }
    if (mel.host && mel.host != 1u + (uint32_t)port && cmd != MEL_HELLO)
        return 1;                        /* another's: the remote is taken until a HELLO from here */
    if (cmd == MEL_BYE) {
        if (mel.host)
            mel_let_go(MEL_WHY_BYE);
        return 1;
    }
    if (mel.host != 1u + (uint32_t)port)
        mel_claim(port, cmd);
    mel.heard_t = plat_ms();
    if (!mel_apply(&mel.melodee, p, n))
        return 1;
    mel.frames_in++;
    if (cmd == MEL_HELLO) {
        hello_to(port == PORT_USB ? TO_USB : TO_BLE);
        master_send(1);
    }
    if (mel.melodee.dirty & MEL_DIRTY_KEYS) {
        if (mel.melodee.octave != MEL_OCT_SAME)
            mel.octave = (int8_t)clamp((int32_t)mel.melodee.octave - 64, -OCT_MAX, OCT_MAX);
        mel.melodee.dirty &= (uint8_t)~MEL_DIRTY_KEYS;
    }
    if (mel.melodee.dirty & MEL_DIRTY_POPUP) {
        mel.melodee.popup_shown = mel.melodee.popup[0][0] || mel.melodee.popup[1][0];
        mel.popup_until = mel.melodee.popup_tenths ? plat_ms() + 100u * mel.melodee.popup_tenths : 0u;
    }
    return 1;
}

/* USB's SysEx: on the FM-1 each byte as it comes (app_usb_sysex_byte, from USB's interrupt: felucca-bsp keeps one
 * frame, and Melodee sends bursts), gathered by the main loop (mel_usb_take); the frames fm1-base hands over are then
 * only let go. The simulator hands whole frames (its script's sysex) */
#define MEL_USB_Q 4096u
static volatile uint8_t mel_usb_q[MEL_USB_Q] PLAT_POOL;
static volatile uint32_t mel_usb_w, mel_usb_r;
static volatile uint8_t mel_usb_over;   /* bytes lost: the frame being gathered is dropped */
static mel_sx_t mel_usb_sx;
void app_usb_sysex_byte(uint8_t b)
{
    uint32_t w = mel_usb_w;
    if (w - mel_usb_r >= MEL_USB_Q) {
        mel_usb_over = 1;
        return;
    }
    mel_usb_q[w % MEL_USB_Q] = b;
    __asm__ volatile("" ::: "memory");   /* (the byte before the index) */
    mel_usb_w = w + 1u;
}
static void mel_usb_take(void)
{
    while (mel_usb_r != mel_usb_w) {
        uint8_t b = mel_usb_q[mel_usb_r % MEL_USB_Q];
        __asm__ volatile("" ::: "memory");
        mel_usb_r++;
        if (mel_usb_over && b != 0xF0u)  /* (lost bytes: nothing until the next frame's start) */
            continue;
        mel_usb_over = 0;
        if (mel_sx_byte(&mel_usb_sx, b))
            mel_frame(mel_usb_sx.buf, mel_usb_sx.n, PORT_USB);
    }
}
#ifdef PLAT_HOST
int app_sysex(const uint8_t *p, uint32_t n) { return mel_frame(p, n, PORT_USB); }
#else
int app_sysex(const uint8_t *p, uint32_t n) { return mel_ours(p, n); }   /* (had byte by byte: let go) */
#endif

void mel_ble_sysex(uint8_t byte)
{
    if (mel_sx_byte(&mel_ble_sx, byte))
        mel_frame(mel_ble_sx.buf, mel_ble_sx.n, PORT_BLE);
}

/* the links: a computer or a phone that comes is greeted (a Melodee among them claims the remote); Melodee's link that
 * goes lets the remote go. Unclaimed, a link that is up is greeted again 1, 2, 4, 8 and 16 s after it came (a HELLO
 * sent before Melodee listened is lost: one did, and the remote waited while its keys played); a Melodee that PINGs and
 * then says nothing for SILENT_MS lets it go */
static void link_up(uint32_t p, int up)
{
    mel.link_t[p] = up ? plat_ms() | 1u : 0u;
    mel.link_hellos[p] = 0;
    if (up)
        hello_to(p == PORT_USB ? TO_USB : TO_BLE);
}
static void links(void)
{
    uint32_t now = plat_ms(), link = plat_midi_out_link(), p;
    uint8_t ble = (uint8_t)plat_ble_connected();
    if (ble != mel.ble_was)
        link_up(PORT_BLE, ble);
    if (!ble && mel.ble_was) {
        mel.pings &= (uint8_t)~TO_BLE;
        if (mel.host == 1u + PORT_BLE)
            mel_let_go(MEL_WHY_LINK);
    }
    mel.ble_was = ble;
    if (link != mel.usb_link) {
        mel.usb_link = link;
        link_up(PORT_USB, 1);
    }
    if (mel.host == 1u + PORT_USB) {     /* unplugged: no host for a second */
        if (plat_midi_out_room(TO_USB))
            mel.usb_gone_t = 0;
        else if (!mel.usb_gone_t)
            mel.usb_gone_t = now | 1u;
        else if (now - mel.usb_gone_t > 1000u) {
            mel.pings &= (uint8_t)~TO_USB;
            mel_let_go(MEL_WHY_LINK);
        }
    }
    for (p = 0; p < 2u; p++) {           /* unclaimed: HELLO again, 1, 2, 4, 8, 16 s after the link came */
        uint32_t to = p == PORT_USB ? TO_USB : TO_BLE;
        if (!mel.host && mel.link_t[p] && mel.link_hellos[p] < 5u &&
            now - (mel.link_t[p] & ~1u) >= 1000u << mel.link_hellos[p] && (p == PORT_BLE ? ble : plat_midi_out_room(TO_USB) != 0)) {
            mel.link_hellos[p]++;
            hello_to(to);
        }
    }
    if (mel.host && (mel.pings & (mel.host == 1u + PORT_USB ? TO_USB : TO_BLE)) && now - mel.heard_t > SILENT_MS) {
        mel.pings &= (uint8_t)~(mel.host == 1u + PORT_USB ? TO_USB : TO_BLE);
        mel_let_go(MEL_WHY_SILENT);      /* (a Melodee that went quiet: gone; its keys to everyone again) */
    }
    if (mel.hello_owed)
        hello_to(mel.hello_owed);
}

/* --------------------------------------------------------------- SETUP --- */
enum { ROW_CH, ROW_VEL, ROW_PRS, ROW_BLE, ROW_THEME, ROW_VERSION };

static void setup_open(int on)
{
    mel.setup = (uint8_t)on;
    mel.setup_row = ROW_CH;
    if (!on && mel.set_dirty)
        settings_save();
    mel.redraw = MEL_DIRTY_ALL;
}

/* K1..K4 the first four rows, PRESET the theme */
static void setup_turn(int enc, int32_t d)
{
    switch (enc) {
    case EN_K1:
        set.keys_ch = (uint8_t)clamp((int32_t)set.keys_ch + d, 0, 15);
        mel.setup_row = ROW_CH;
        break;
    case EN_K2:
        set.curve = (int8_t)clamp(set.curve + d, -5, 5);
        touch_curve(set.curve);
        mel.setup_row = ROW_VEL;
        break;
    case EN_K3:
        set.pressure = d > 0;
        mel.setup_row = ROW_PRS;
        break;
    case EN_K4:
        if ((d > 0) == !set.ble_off)
            return;
        set.ble_off = d < 0;
        plat_ble_enable(!set.ble_off);
        mel.setup_row = ROW_BLE;
        break;
    case EN_PRESET: {
        uint32_t t = palette_from_stored(set.theme);
        t = (t + NPALETTES + (uint32_t)(d > 0 ? 1 : NPALETTES - 1)) % NPALETTES;
        set.theme = (uint8_t)palette_to_stored(t);
        palette_set(t);
        mel.setup_row = ROW_THEME;
        mel.redraw = MEL_DIRTY_ALL;
        break;
    }
    default:
        return;
    }
    mel.set_dirty = 1;
}

/* ----------------------------------------------------------------- input --- */
static void octave_step(int32_t d)
{
    mel.octave = (int8_t)clamp(mel.octave + d, -OCT_MAX, OCT_MAX);
}

/* (Melodee FM-1's seq.c) degree d of the scale (Melodee's key: MEL_SCALE) from C4 = ROOT, offset semitones on; a note
 * off the MIDI range is silent when strict (else clamped) */
static uint32_t scale_degree_map(const mel_v2_t *v, int32_t degree, int32_t offset, int strict)
{
    uint32_t mask = v->mask | 1u, i, count = 0;
    int32_t oct, n;
    for (i = 0; i < 12u; i++)
        count += (mask >> i) & 1u;
    oct = degree / (int32_t)count;
    degree %= (int32_t)count;
    if (degree < 0) {
        degree += (int32_t)count;
        oct--;
    }
    for (i = 0; i < 12u; i++)
        if ((mask >> i) & 1u) {
            if (!degree)
                break;
            degree--;
        }
    n = 60 + (int32_t)v->root + 12 * oct + (int32_t)i + offset + (int32_t)v->trans - 64;
    if (strict && (n < 0 || n > 127))
        return KEY_SILENT;
    return (uint32_t)clamp(n, 0, 127);
}
/* key k's note through the selected track's QNT (seq.c kb_map): SNAP every key rounded down to the scale; WHITE the
 * white keys walk the scale from C4 = ROOT, the black keys silent; ALL every key the next degree; OFF as it is (with
 * the transpose). KEY_SILENT: none */
static uint32_t kb_map(uint32_t k)
{
    static const int8_t DEGREE[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    const mel_v2_t *v = &mel.melodee.v2;
    int32_t n = NOTE_F3 + (int32_t)k, degree, trans = (int32_t)v->trans - 64;
    uint32_t mask = v->mask | 1u, guard = 12;
    if (!mel.host || mel.setup || v->q == MEL_Q_OFF)
        return (uint32_t)clamp(n + 12 * mel.octave + (mel.host && !mel.setup ? trans : 0), 0, 127);
    if (v->q == MEL_Q_SNAP) {                           /* SNAP: every key, rounded down to the scale */
        n += 12 * mel.octave + trans;
        while (guard-- && !((mask >> (uint32_t)((n - (int32_t)v->root + 120) % 12)) & 1u))
            n--;
        return (uint32_t)clamp(n, 0, 127);
    }
    if (v->q == MEL_Q_ALL)
        return scale_degree_map(v, n - 60, 12 * mel.octave, 1);
    if ((degree = DEGREE[n % 12]) < 0)                  /* WHITE */
        return KEY_SILENT;
    return scale_degree_map(v, degree + (n / 12 - 5) * 7, 12 * mel.octave, 0);
}

static void keys_in(uint32_t keys)
{
    uint32_t k, flags = keys_flags();
    int as_buttons = (flags & MEL_K_BUTTONS) && !mel.setup;
    int pressure = set.pressure && !(flags & MEL_K_NO_PRESSURE) && !as_buttons;
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(mel.keys & bit)) {
            uint32_t v = touch_vel(k), ch = as_buttons ? MEL_CH : keys_ch();
            uint32_t note = as_buttons ? MEL_NOTE_KEY + k : kb_map(k);
            mel.key_note[k] = (uint8_t)note;
            mel.key_ch[k] = (uint8_t)ch;
            mel.prs[k] = 0;
            mel.key_btn = as_buttons ? mel.key_btn | bit : mel.key_btn & ~bit;
            if (note != KEY_SILENT)
                mel_send(0x90u | ch, note, v ? v : 1u);
        } else if (!(keys & bit) && (mel.keys & bit)) {
            if (mel.key_note[k] != KEY_SILENT)
                mel_send(0x80u | mel.key_ch[k], mel.key_note[k], 0);
        } else if ((keys & bit) && pressure && !(mel.key_btn & bit) && mel.key_note[k] != KEY_SILENT &&
                   touch_pressure(k) != mel.prs[k]) {
            mel.prs[k] = touch_pressure(k);
            mel_send(0xA0u | mel.key_ch[k], mel.key_note[k], mel.prs[k]);
        }
    }
    mel.keys = keys;
}

static void buttons_in(uint32_t btn)
{
    uint32_t k, now = plat_ms(), both = 1u << B_OCTDN | 1u << B_OCTUP;
    int oct_local = !(keys_flags() & MEL_K_OCT_MELODEE) || mel.setup;
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k, down = (btn & bit) && !(mel.btn & bit), up = !(btn & bit) && (mel.btn & bit);
        if (!down && !up)
            continue;
        if (!mel.setup)                  /* every button is Melodee's to read, OCT too */
            mel_send(down ? 0x90u | MEL_CH : 0x80u | MEL_CH, MEL_NOTE_BUTTON + k, down ? 127u : 0u);
        if (!down)
            continue;
        if ((k == B_OCTDN || k == B_OCTUP) && oct_local) {
            if ((btn & both) == both)    /* both: back to 0 */
                mel.octave = 0;
            else
                octave_step(k == B_OCTUP ? 1 : -1);
            mel.redraw |= MEL_DIRTY_FOOT;
        } else if (mel.setup && k == B_HOME) {
            setup_open(0);
        }
    }
    if ((btn & both) == both) {          /* held both: SETUP opens or closes */
        if (!mel.both_t)
            mel.both_t = now | 1u;
        else if (!mel.both_used && now - mel.both_t >= BOTH_HOLD_MS) {
            mel.both_used = 1;
            setup_open(!mel.setup);
        }
    } else {
        mel.both_t = 0;
        mel.both_used = 0;
    }
    mel.btn = btn;
}

static void encoders_in(void)
{
    int e;
    for (e = 0; e < NE; e++) {
        int32_t d = plat_enc(e);
        if (!d)
            continue;
        if (mel.setup)
            setup_turn(e, d);
        else {
            mel_send(0xB0u | MEL_CH, MEL_CC_ENC + (uint32_t)e, (uint32_t)clamp(64 + d, 1, 127));
            if (mel.host && e >= EN_K1)  /* (its card lights at once: no wait for Melodee) */
                ui_knob_turned((uint32_t)(e - EN_K1));
        }
    }
}

static void input(void)
{
    mel_usb_take();                      /* Melodee's messages from USB first */
    touch_poll();                        /* the sensed keys' presses, velocities and pressure */
    keys_in(touch_keys());
    buttons_in(plat_buttons());
    encoders_in();
    if (!mel.setup)
        master_send(0);
}

void app_input_only(void) { input(); }

/* ---------------------------------------------------------------- lights --- */
static void lights(void)
{
    const mel_screen_t *s = &mel.melodee;
    if (mel.host && s->lights_set && !mel.setup) {
        uint32_t bl = s->btn_lit, kl = s->keys_lit;
        if (s->lights & MEL_L_HELD_KEYS)
            kl |= mel.keys;
        if (s->lights & MEL_L_HELD_BUTTONS)
            bl |= mel.btn;
        plat_lights(bl, kl, s->btn_dim, s->keys_dim, s->lights & MEL_L_DIM);
    } else {                             /* the FM-1's own: what is held, the octave away from 0, SETUP's ways out */
        uint32_t b = mel.btn;
        if (mel.octave < 0)
            b |= 1u << B_OCTDN;
        if (mel.octave > 0)
            b |= 1u << B_OCTUP;
        if (mel.setup)
            b |= 1u << B_HOME;
        plat_glow(1);
        plat_leds(b, mel.keys);
    }
}

/* -------------------------------------------------------------- the screen --- */
#include "mel_draw.c"
#include "ui/gfx_names_end.h"               /* (fm1-base's gfx.c names back for the platform after the app) */
