/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1 on the remote: one compilation unit, as Melodee FM-1's own (firmware/src/melodee.c), its files in its
 * order from fm1/src (forked from Melodee FM-1, fm1/SOURCE, and the remote's own since). Where Melodee FM-1 has its hardware (the LCD, the panel's scan, USB, Bluetooth,
 * the flash) or makes sound (the engine table, the audio interrupt), the remote has its own sections (remote_*.c):
 * the screen, the panel and MIDI are fm1-base's (app.c, through unit.h), the engines and the music Melodee's
 * (link.c). Everything Melodee FM-1's pages and controls do is its own code; its model (the sequencer, the patterns,
 * the takes, the song, the projects) is Melodee's, mirrored (remote_model.c), a few of its pure functions as they
 * are (fm1/src/excerpt_*.c).
 * Melodee FM-1's engines, voices, mixer and effects, its own sounds, user presets and projects in flash, its sound
 * designer, its NAME and NEW SONG screens, MIDI knobs and MOTION are not here (cut from fm1/src); what its pages still
 * ask of them upreset.c, absent_engines.h and absent_sound.h answer: absent. SAVE asks Melodee to save its project. */
#include <stdint.h>
#include <stddef.h>
#include "unit.h"

/* ---------------------------------------------------- build options --- */
#define MELODEE_FLASH 0          /* nothing kept in the FM-1's flash: the project is Melodee's */
#define MELODEE_RAM_AUDIO 0
#define MELODEE_VERSION "REMOTE"
#define MELODEE_ID "FM-1_1400"
#define MELODEE_RES
#define RESOURCE_WORDS 8192u    /* the working arena (resources.c): 32 KB, for the undo histories */
#define NMIDI 28                 /* core.h: 32 tracks (NPART 4 + 28), as many as the pool holds; every one a part */
#define RES_OK() 1               /* Rubik's faces are built in (gen: ui_fonts.h), not in a resource region */
#include "../../fm1/src/build_options.h"
#include "../../fm1/src/performance_options.h"

#ifdef PLAT_HOST
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define __attribute__(x)         /* (its sections, alignments: the FM-1's; as Melodee FM-1's host builds) */
#endif
#ifndef PLAT_HOST                /* (freestanding: fm1-base's libc) */
void *memset(void *d, int c, __SIZE_TYPE__ n);
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memmove(void *d, const void *s, __SIZE_TYPE__ n);
int memcmp(const void *a, const void *b, __SIZE_TYPE__ n);
#endif
/* Melodee FM-1's few names with external linkage, its own here (fm1-base has a panel and a boot guard too) */
#define panel fm1u_panel
#define bootguard fm1u_bootguard
#define settings fm1u_settings
#define memset melodee_memset
#define memcpy melodee_memcpy
#define memcmp melodee_memcmp
#include "melodee_tables.h"      /* (generated: fm1.toml gen) */
#include "../../fm1/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
#include "remote_hal.c"          /* Melodee FM-1's HAL calls: the panel, its LEDs, the LCD, the clock */
#include "../../fm1/src/gfx.c"

/* ------------------------------------------- the mirror of Melodee's --- */
#include "../../fm1/src/core.h"
#include "../../fm1/src/dsp.c"                 /* (its helpers: clamp, the curves the pages draw; nothing of it plays) */
#include "../../fm1/src/eng_midi.c"            /* (a MIDI Track: the tracks after the four) */
#include "remote_sounds.c"       /* Melodee FM-1's engines.c: Melodee's instruments, told over the link */
#include "../../fm1/src/params.c"
#include "absent_sound.h"        /* (Melodee FM-1's punch effects and inserts: not here) */
#include "remote_model.c"        /* what the pages read and ask of Melodee FM-1's model: Melodee's, mirrored */
#include "../../fm1/src/excerpt_midi_track.c"
#include "../../fm1/src/excerpt_seq.c"
#include "../../fm1/src/excerpt_voice.c"
#include "../../fm1/src/chord.c"               /* a chord key's chord, the chords' names */
#include "../../fm1/src/excerpt_song_chain.c"
#include "../../fm1/src/excerpt_recording.c"
#include "../../fm1/src/excerpt_mod.c"
#include "remote_audio.c"        /* audio.c: the scope, Melodee's */
#include "remote_keys.c"         /* the keys as Melodee FM-1 lays them out: notes to Melodee */

/* --------------------------------------------------------------- UI --- */
#include "../../fm1/src/panel.c"
#include "../../fm1/src/ui.c"
#include "../../fm1/src/icons.c"
#include "../../fm1/src/ui_graph.c"
#include "../../fm1/src/ui_draw.c"
#include "../../fm1/src/ui_menu.c"
#include "../../fm1/src/ui_input.c"
#include "../../fm1/src/ui_layer.c"

#include "upreset.c"             /* (here: the user's sounds are Melodee's) */
#include "remote_project.c"      /* project.c, tracks.c: Melodee's projects, tracks asked of Melodee */

#include "link.c"                /* the model kept with Melodee's */
#include "remote_main.c"         /* main.c: Melodee FM-1's start and main loop, as unit.h calls them */
