/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The keys, as Melodee FM-1 plays them (its seq.c keyboard_block, key_on, key_off): the selected track's, laid out
 * as its QNT and scale say (kb_map), a chord key's chord (chord.c chord_build), the drum grid's lanes, a layer's keys
 * the layer's (ui_layer.c kb_mask); NAME and the patterns' selection take them all. The notes go to Ellic Studio as MIDI,
 * track k's on channel (k mod 15) + 1 (16 is the remote's: MASTER), a key's release ending the notes its press began.
 * Nothing sounds here. */
static uint32_t kb_prev;
static uint8_t kb_note[27], kb_trk[27];

static void key_send(uint32_t k, uint32_t on)
{
    uint32_t tr = kb_trk[k] % NTRK, i;
    uint32_t ch = midi_track_only(&trk[tr]) ? midi_track_channel(&trk[tr]) : tr % 15u;   /* (16: the remote's own) */
    for (i = 0; i < kb_chn[k]; i++) {
        uint32_t x = kb_chord[k][i];
        midi_out_event(on ? 0x09u | (0x90u | ch) << 8 | x << 16 | 100u << 24 : 0x08u | (0x80u | ch) << 8 | x << 16);
        if (on && !drum_track(&trk[tr]))
            live_held[tr][x >> 5] |= 1u << (x & 31u);
        else if (!on)
            live_held[tr][x >> 5] &= ~(1u << (x & 31u));
    }
    if (on)
        last_note = kb_note[k];
}

static void keys_block(void)
{
    uint32_t cur = fm1_in.notes, ch = cur ^ kb_prev, k;
    if (!ch)
        return;
    for (k = 0; k < 27u; k++) {
        if (!((ch >> k) & 1u))
            continue;
        if ((cur >> k) & 1u) {
            kb_trk[k] = song.sel;
            kb_chn[k] = 0;
            if (kb_asleep) {
                kb_note[k] = KB_SILENT;
                continue;
            }
            if ((fm1_in.buttons & kb_mask) || kb_lock) {        /* a layer's button held: the key is the layer's */
                kb_note[k] = KB_SILENT;
                kb_layer |= 1u << k;
                continue;
            }
            if (song.grid == 2u)                                /* NAME / pattern selection: the UI's */
                kb_note[k] = KB_SILENT;
            else if (song.grid)                                 /* the drum grid: a lane key its lane's note */
                kb_note[k] = key_black(k) && key_place(k) < NLANE ? DRUM_LANE_NOTE[key_place(k)] : KB_SILENT;
            else
                kb_note[k] = (uint8_t)kb_map(&trk[kb_trk[k]], k);
            if (kb_note[k] == KB_SILENT)
                continue;
            kb_chn[k] = (uint8_t)chord_build(&trk[kb_trk[k]], kb_note[k], kb_chord[k]);
            key_send(k, 1);
        } else {
            if ((kb_layer >> k) & 1u) {
                kb_layer &= ~(1u << k);
                continue;
            }
            if (kb_note[k] == KB_SILENT)
                continue;
            key_send(k, 0);
            kb_chn[k] = 0;
        }
    }
    for (k = 0; k < NTRK; k++)
        memcpy(live_last[k], live_held[k], sizeof live_last[k]);
    kb_prev = cur;
}
