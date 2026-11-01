/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Excerpts of Melodee FM-1's midi_track.c as they are (tools/sync_fm1.py): a MIDI Track's channel (midi_track.c). */
static uint32_t midi_track_channel(const track_t *t)
{ return (uint32_t)clamp(t->p[P_E0], 1, 16) - 1u; }
