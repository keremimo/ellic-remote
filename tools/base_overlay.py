# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Checked changes to the pinned fm1-base, made in the disposable build copy; upstream files keep their notices.

Bluetooth MIDI SysEx both ways (fm1-base drops SysEx in and has no SysEx out over Bluetooth), USB's SysEx a byte at
a time to the app as well (felucca-bsp keeps one frame: a burst of Melodee's lost all but its first), the name, and
Saadet's fixes to the peripheral link (tools/base_overlay.py there, the same author)."""


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
/* Melodee Remote: SysEx in as well, a byte at a time to the app (mel_ble_sysex), across packets. A BLE MIDI SysEx
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
            /* Melodee Remote: SysEx, as USB-MIDI packs it (4 its start or middle, three bytes; 5, 6, 7 its end with
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
    app_usb_sysex_byte(b);                             /* Melodee Remote: every SysEx byte to the app as well, as it
                                                        * comes (the frame below holds one: a burst loses the rest) */
#endif'''


def apply(work):
    # USB SysEx: every byte to the app too (app.h APP_USB_SYSEX_BYTE), not one frame at a time.
    p = work / 'bsp/chip/usb.c'
    s = p.read_text()
    s = replace(s, '''    if (b >= 0xF8u)
        return;                                        /* realtime may occur anywhere in SysEx */''', USB_SYSEX)
    p.write_text('/* Melodee Remote USB SysEx to the app Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)

    # BLE-only builds have no sys_timer task: SDK timers naming that service belong to the main loop's timer pump,
    # as unnamed BLE timers already do (Saadet).
    p = work / 'ble/shim/systimer.c'
    s = p.read_text()
    s = replace(s, '            tm[i].task = task;',
                '            tm[i].task = !SHIM_WIFI && task && !strcmp(task, "sys_timer") ? 0 : task;')
    p.write_text('/* Melodee Remote timer routing (from Saadet) Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)

    p = work / 'ble/midi/le_midi.c'
    s = p.read_text()
    s = function(s, 'static void midi_parse(const u8 *b, u16 len)', MIDI_PARSE)
    s = replace(s, 'static u16 send_mtu = 20;',
                'static u8 sx_out;                                /* a SysEx goes on in the next packet */\n'
                'static u16 send_mtu = 20;')
    s = replace(s, 'u8 st = (u8)(pkts[i] >> 8), cin = pkts[i] & 0xF, m[4];',
                'u8 st = (u8)(pkts[i] >> 8), cin = pkts[i] & 0xF, m[8];')
    s = replace(s, '        /* running status', SYSEX_OUT)
    # The name: "Melodee" in the advertisement (its 31 bytes hold eight letters beside the MIDI service), the whole
    # "Melodee Remote" in the scan response and the GAP name.
    s = replace(s, 'static const char gap_device_name[] = "FM-1";',
                'static const char gap_device_name[] = "Melodee Remote";\n'
                'static const char adv_short_name[] = "Melodee";')
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
    p.write_text('/* Melodee Remote SysEx and naming Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)

    p = work / 'device/radio.c'
    s = p.read_text()
    s = replace(s, 'BT ready: advertising as FM-1', 'BT ready: advertising as Melodee Remote')
    # Bluetooth switched off while it was still starting stays off (Saadet's).
    s = replace(s, 'static volatile uint8_t ble_state;', 'static volatile uint8_t ble_state;\nstatic uint8_t ble_off;')
    s = replace(s, 'static uint8_t ble_off;                           /* switched off by the player (plat_ble_enable) */', '')
    s = replace(s, '        bt_ble_init();\n        ble_state = BLE_READY;', '''        bt_ble_init();
        if (ble_off) {
            extern void ble_midi_enable(int on);
            ble_midi_enable(0);                   /* an OFF asked for while starting still wins */
        }
        ble_state = BLE_READY;''')
    p.write_text('/* Melodee Remote radio naming (with Saadet\'s start) Copyright (C) 2026 Kerem Kilic (Ellic Studio) */\n' + s)
