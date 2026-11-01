/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The song's punch lane (Melodee FM-1's ui_song_punch.c: its master punch effects) is not on the remote. */
static uint32_t song_punch_leds(void) { return 0; }
static void song_punch_input(uint32_t notes, uint32_t pressed, uint32_t oct, int erase)
{
    (void)notes; (void)pressed; (void)oct; (void)erase;
}
static void song_punch_draw(void) {}
static uint32_t punch_selector_leds(int32_t fx) { (void)fx; return 0; }
