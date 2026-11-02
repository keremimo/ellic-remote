/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's projects and tracks (its project.c, tracks.c) on a pure remote: Ellic Studio's. SAVE > PROJECT's slots
 * are the projects Ellic Studio names for them (link.c), a save, a load, a new song asked of Ellic Studio; a track added or
 * removed asked of Ellic Studio too, the tracks as Ellic Studio then says them. No template, no settings kept here (yet). */
#define PROJ_NAME_LEN 12u                     /* (a project's name, as Melodee FM-1's project.c keeps it) */
static struct { uint8_t used; char name[PROJ_NAME_LEN + 1u]; } proj_slots[PROJ_SLOTS];   /* (Ellic Studio's: link.c) */
static char proj_current[PROJ_NAME_LEN + 1u];
static char link_save_name[PROJ_NAME_LEN + 1u];      /* the name a save asks for ("": the project's own) */

static void project_quick_save(void) { link_save_name[0] = 0; link_request(RQ_SAVE, 127u, 0, 0); }


static void settings_save(void) {}
static uint32_t chain_prepare_from(uint32_t start) { return start; }

static void track_add(void) { add_sheet_open(trk_n()); }   /* (the new track sheet first: ui_popup.c) */
static int track_create_to(uint32_t k) { add_sheet_open(k); return 0; }
static int track_remove(uint32_t k) { link_request(RQ_TRACK_REMOVE, k, 0, 0); return 0; }
