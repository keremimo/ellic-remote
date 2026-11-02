# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Checked changes to the pinned fm1-base, made in the disposable build copy; upstream files keep their notices.

Bluetooth MIDI SysEx both ways (fm1-base drops SysEx in and has no SysEx out over Bluetooth), USB's SysEx a byte at
a time to the app as well (felucca-bsp keeps one frame: a burst of Ellic Studio's lost all but its first), the name,
Saadet's fixes to the peripheral link (tools/base_overlay.py there, the same author), and Melodee FM-1's light levels
in the scan (three planes, each lit on the frames its mask leaves: full, 1/2, 1/4, 1/8 of lit)."""


def replace(s, old, new):
    if old not in s:
        raise ValueError(f"Pinned fm1-base input changed: {old[:90]!r}")
    return s.replace(old, new)


def function(s, name, body):
    """the whole function that starts with name, body in its place"""
    start = s.index(name)
    start = s.rfind('\n', 0, start) + 1
    opening = s.index('{', start)
    level = 1
    end = opening + 1
    while level:
        if s[end] == '{':
            level += 1
        if s[end] == '}':
            level -= 1
        end += 1
    return s[:start] + body.rstrip() + s[end:]


MIDI_PARSE = r'''
/* Ellic Remote: SysEx in as well, a byte at a time to the app (mel_ble_sysex), across packets. A BLE MIDI SysEx
 * starts with a timestamp and F0, goes on in the next packets straight after their header, and ends with a
 * timestamp and F7; real time may come between. Any other status cuts it short (the app drops what it had). */
extern void mel_ble_sysex(u8 b);
static u8 sx_in;                                 /* inside a SysEx */
static void midi_parse(const u8 *b, u16 len)
{
    u16 i = 1;
    u8 st = 0;
    while (i < len) {
        if (sx_in && !(b[i] & 0x80)) {           /* SysEx data: after its start, or at the head of a packet */
            mel_ble_sysex(b[i++]);
            continue;
        }
        if (b[i] & 0x80) {                       /* a timestamp low byte, then maybe a status */
            i++;
            if (i < len && (b[i] & 0x80)) {
                u8 s = b[i++];
                if (s >= 0xF8) {                 /* real time: anywhere, a SysEx's middle too */
                    ble_midi_receive(0xFu | (uint32_t)s << 8);
                    continue;
                }
                if (sx_in) {                     /* the SysEx ends, or is cut short */
                    sx_in = 0;
                    mel_ble_sysex(s);
                    if (s == 0xF7)
                        continue;
                }
                if (s == 0xF0) {
                    sx_in = 1;
                    mel_ble_sysex(0xF0);
                    st = 0;
                    continue;
                }
                if (s == 0xF7) {                 /* an end with no start */
                    st = 0;
                    continue;
                }
                st = s;
            }
        }
        if (!st)
            return;
        {
            int n = midi_len(st), k;
            u8 d[2] = {0, 0};
            for (k = 0; k < n; k++) {
                if (i >= len || (b[i] & 0x80))
                    return;
                d[k] = b[i++];
            }
            ble_midi_receive((uint32_t)cin_of(st, n) | (uint32_t)st << 8 | (uint32_t)d[0] << 16 | (uint32_t)d[1] << 24);
            if (st >= 0xF0)                      /* system common: no running status after it */
                st = 0;
        }
    }
}
'''

SYSEX_OUT = r'''        if (cin == 4 || cin == 6 || cin == 7 || (cin == 5 && (sx_out || st == 0xF7))) {
            /* Ellic Remote: SysEx, as USB-MIDI packs it (4 its start or middle, three bytes; 5, 6, 7 its end with
             * one, two, three): F0 and F7 each after a timestamp, the bytes between as they are, going on straight
             * after the header of the next packet */
            u32 c = cin == 4 ? 3 : cin - 4u, j;
            u8 sx = sx_out;
            for (j = 0; j < c; j++) {
                u8 v = (u8)(pkts[i] >> (8 + 8 * j));
                if (v == 0xF0 || v == 0xF7) {
                    m[k++] = (u8)(0x80 | (t & 0x7F));
                    m[k++] = v;
                    sx = v == 0xF0;
                } else if (!(v & 0x80)) {
                    m[k++] = v;
                }
            }
            if (len + k > send_mtu || len + k > sizeof buf)
                break;
            memcpy(buf + len, m, k);
            len += k;
            tp = t;
            run = 0;
            sx_out = sx;
            continue;
        }
        /* running status'''


USB_SYSEX = r'''    if (b >= 0xF8u)
        return;                                        /* realtime may occur anywhere in SysEx */
#if defined(APP_USB_SYSEX_BYTE) && !defined(FELUCCA_LOADER)
    app_usb_sysex_byte(b);                             /* Ellic Remote: every SysEx byte to the app as well, as it
                                                        * comes (the frame below holds one: a burst loses the rest) */
#endif'''


LED_MID = '''static uint8_t fm1_led_mid[FM1_NCOL];      /* a steady mid level (see top): lit 1 frame in FM1_LED_MID_N */'''

LED_PLANES_STATE = LED_MID + r'''
/* Ellic Remote: Melodee FM-1's levels, three planes of LEDs lit as a lit LED on the frames their masks leave (the
 * frame count & mask == 0: mask 0 every frame, full; 1 one frame in 2; 3 one in 4; 7 one in 8), all in step, dark in
 * the others. fm1__pl_ph counts the frames when column 0 comes round; plat_lights_planes writes them */
static uint8_t fm1_led_pl[3][FM1_NCOL];
static volatile uint8_t fm1_led_pl_mask[3];
static uint8_t fm1__pl_ph;'''

LED_LIT = r'''#define FM1__LIT(c) (fm1_led[c] | (fm1__br_lit ? fm1_led_breath[c] & ~fm1_led_dim[c] : 0u) | \
                     (!fm1__mid_ph ? fm1_led_mid[c] : 0u))'''

LED_PLANES_LIT = r'''#define FM1__PL(c) ((fm1__pl_ph & fm1_led_pl_mask[0] ? 0u : fm1_led_pl[0][c]) | \
                    (fm1__pl_ph & fm1_led_pl_mask[1] ? 0u : fm1_led_pl[1][c]) | \
                    (fm1__pl_ph & fm1_led_pl_mask[2] ? 0u : fm1_led_pl[2][c]))
#define FM1__LIT(c) (fm1_led[c] | (fm1__br_lit ? fm1_led_breath[c] & ~fm1_led_dim[c] : 0u) | \
                     (!fm1__mid_ph ? fm1_led_mid[c] : 0u) | FM1__PL(c))'''

LED_FRAME = '''        fm1__mid_ph = (uint8_t)(fm1__mid_ph + 1u >= fm1__mid_n ? 0u : fm1__mid_ph + 1u);'''

LED_PLANES_DEVICE = r'''/* Ellic Remote: Melodee FM-1's three planes (felucca-bsp fm1_led_pl): plane j's buttons and keys, lit on the frames
 * mask[j] leaves; bit NB in buttons PLAY's green LED */
void plat_lights_planes(const uint32_t *buttons, const uint32_t *keys, const uint8_t *mask)
{
    uint8_t pl[3][FM1_NCOL];
    uint32_t i, j, c;
    for (j = 0; j < 3u; j++) {
        for (c = 0; c < FM1_NCOL; c++)
            pl[j][c] = 0;
        for (i = 0; i < NB + NKEYS; i++) {
            uint32_t id = i < NB ? panel.btn[i] : 14u + (i - NB);
            uint8_t q = led_pos[id];
            if (q != 0xFF && (i < NB ? (buttons[j] >> i) & 1u : (keys[j] >> (i - NB)) & 1u))
                pl[j][q >> 3] |= (uint8_t)(1u << (q & 7u));
        }
        if (buttons[j] & LED_PLAY_GREEN)
            pl[j][8] |= 1u << 1;
    }
    for (j = 0; j < 3u; j++) {
        fm1_led_pl_mask[j] = mask[j];
        for (c = 0; c < FM1_NCOL; c++)
            fm1_led_pl[j][c] = pl[j][c];
    }
}
'''

HOST_GLOW = "static int glow_on = 1;                 /* the device's button glow (plat_glow) */"

LED_PLANES_HOST = r'''/* Ellic Remote: Melodee FM-1's three planes ("leds" shows them as the glow) */
static uint32_t pl_keys;
void plat_lights_planes(const uint32_t *buttons, const uint32_t *keys, const uint8_t *mask)
{
    (void)buttons;
    (void)mask;
    pl_keys = keys[0] | keys[1] | keys[2];
}
'''

PLAT_MID = 'void plat_lights_mid(uint32_t buttons, uint32_t keys);'

PLAT_PLANES = PLAT_MID + '''
/* Ellic Remote: three planes of levels, plane j lit on the frames whose count & mask[j] is 0 (mask 0: always, full;
 * 1, 3, 7: 1/2, 1/4, 1/8 of lit), unless lit; bit NB in buttons PLAY's green LED. Each call replaces them */
void plat_lights_planes(const uint32_t *buttons, const uint32_t *keys, const uint8_t *mask);'''


def apply(work):
    # Melodee FM-1's light levels in the scan: three planes, each lit on the frames its mask leaves (the remote's
    # LIGHTS: LOW 1/8 .. FULL, the idle buttons' 1/4, the breathing), beside fm1-base's lit, glow and mid.
    p = work / 'bsp/boards/fm1/fm1_input.h'
    s = p.read_text()
    s = replace(s, LED_MID, LED_PLANES_STATE)
    s = replace(s, LED_LIT, LED_PLANES_LIT)
    s = replace(s, LED_FRAME, LED_FRAME + '\n        fm1__pl_ph++;                              /* (Ellic Remote: the planes\' frames) */')
    p.write_text('/* Ellic Remote light levels Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)
    p = work / 'device/plat_fm1.c'
    s = p.read_text()
    s = replace(s, 'static uint8_t glow_on = 1, play_red;', LED_PLANES_DEVICE + 'static uint8_t glow_on = 1, play_red;')
    p.write_text('/* Ellic Remote light levels Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)
    p = work / 'host/plat_host.c'
    s = p.read_text()
    s = replace(s, HOST_GLOW, LED_PLANES_HOST + HOST_GLOW)
    s = replace(s, '(dim_keys | mid_keys) >> HOST_WHITE_K', '(dim_keys | mid_keys | pl_keys) >> HOST_WHITE_K')
    s = replace(s, '(dim_keys | mid_keys) >> HOST_BLACK_K', '(dim_keys | mid_keys | pl_keys) >> HOST_BLACK_K')
    p.write_text('/* Ellic Remote light levels Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)
    p = work / 'include/plat.h'
    s = p.read_text()
    s = replace(s, PLAT_MID, PLAT_PLANES)
    p.write_text(s)

    # USB SysEx: every byte to the app too (app.h APP_USB_SYSEX_BYTE), not one frame at a time.
    p = work / 'bsp/chip/usb.c'
    s = p.read_text()
    s = replace(s, '''    if (b >= 0xF8u)
        return;                                        /* realtime may occur anywhere in SysEx */''', USB_SYSEX)
    p.write_text('/* Ellic Remote USB SysEx to the app Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)

    # BLE-only builds have no sys_timer task: SDK timers naming that service belong to the main loop's timer pump,
    # as unnamed BLE timers already do (Saadet).
    p = work / 'ble/shim/systimer.c'
    s = p.read_text()
    s = replace(s, '            tm[i].task = task;',
                '            tm[i].task = !SHIM_WIFI && task && !strcmp(task, "sys_timer") ? 0 : task;')
    p.write_text('/* Ellic Remote timer routing (from Saadet) Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)

    p = work / 'ble/midi/le_midi.c'
    s = p.read_text()
    s = function(s, 'static void midi_parse(const u8 *b, u16 len)', MIDI_PARSE)
    s = replace(s, 'static u16 send_mtu = 20;',
                'static u8 sx_out;                                /* a SysEx goes on in the next packet */\n'
                'static u16 send_mtu = 20;')
    s = replace(s, 'u8 st = (u8)(pkts[i] >> 8), cin = pkts[i] & 0xF, m[4];',
                'u8 st = (u8)(pkts[i] >> 8), cin = pkts[i] & 0xF, m[8];')
    s = replace(s, '        /* running status', SYSEX_OUT)
    # The name: "Ellic" in the advertisement (its 31 bytes hold eight letters beside the MIDI service), the whole
    # "Ellic Remote" in the scan response and the GAP name.
    s = replace(s, 'static const char gap_device_name[] = "FM-1";',
                'static const char gap_device_name[] = "Ellic Remote";\n'
                'static const char adv_short_name[] = "Ellic";')
    s = replace(s, '''    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (void *)gap_device_name, sizeof gap_device_name - 1);
    if (offset > ADV_RSP_PACKET_MAX)''', '''    offset += make_eir_packet_data(&buf[offset], offset, 0x08 /* shortened local name */, (void *)adv_short_name, sizeof adv_short_name - 1);
    if (offset > ADV_RSP_PACKET_MAX)''')
    # A fresh link negotiates its own subscription, MTU and parameters, and no SysEx is half way (Saadet's, and
    # the SysEx states).
    s = replace(s, '    set_ble_work_state(BLE_ST_CONNECT);', '''    CCC_SET(handle, H_CCC, 0);
    send_mtu = 20;
    connection_update_cnt = 0;
    sx_in = sx_out = 0;
    set_ble_work_state(BLE_ST_CONNECT);''')
    s = replace(s, 'return con_handle && CCC_GET(con_handle, H_CCC);', 'return con_handle && (CCC_GET(con_handle, H_CCC) & 1u);')
    # ATT name reads may ask for a buffer larger than what is left of the name (Saadet's).
    s = replace(s, '        if (offset >= len || offset + buffer_size > len)\n            break;', '''        if (offset >= len)
            return 0;
        len -= offset;''')
    s = replace(s, '            memcpy(buffer, &gap_device_name[offset], buffer_size);\n            len = buffer_size;', '''            if (len > buffer_size) len = buffer_size;
            memcpy(buffer, &gap_device_name[offset], len);''')
    p.write_text('/* Ellic Remote SysEx and naming Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)

    p = work / 'device/radio.c'
    s = p.read_text()
    s = replace(s, 'BT ready: advertising as FM-1', 'BT ready: advertising as Ellic Remote')
    # Bluetooth switched off while it was still starting stays off (Saadet's).
    s = replace(s, 'static volatile uint8_t ble_state;', 'static volatile uint8_t ble_state;\nstatic uint8_t ble_off;')
    s = replace(s, 'static uint8_t ble_off;                           /* switched off by the player (plat_ble_enable) */', '')
    s = replace(s, '        bt_ble_init();\n        ble_state = BLE_READY;', '''        bt_ble_init();
        if (ble_off) {
            extern void ble_midi_enable(int on);
            ble_midi_enable(0);                   /* an OFF asked for while starting still wins */
        }
        ble_state = BLE_READY;''')
    p.write_text('/* Ellic Remote radio naming (with Saadet\'s start) Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)
