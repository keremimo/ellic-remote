/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee FM-1's projects and tracks (its project.c, tracks.c) on a pure remote: Melodee's. SAVE > PROJECT's slots
 * are the projects Melodee names for them (link.c), a save, a load, a new song asked of Melodee; a track added or
 * removed asked of Melodee too, the tracks as Melodee then says them. No template, no settings kept here (yet). */
#define PROJ_NAME_LEN 12u                     /* (a project's name, as Melodee FM-1's project.c keeps it) */
static struct { uint8_t used; char name[PROJ_NAME_LEN + 1u]; } proj_slots[PROJ_SLOTS];   /* (Melodee's: link.c) */
static char proj_current[PROJ_NAME_LEN + 1u];
static char link_save_name[PROJ_NAME_LEN + 1u];      /* the name a save asks for ("": the project's own) */

static int project_used(uint32_t slot) { return proj_slots[slot % PROJ_SLOTS].used; }
static int project_name(uint32_t slot, char *b)
{
    str_cpy(b, proj_slots[slot % PROJ_SLOTS].name, PROJ_NAME_LEN + 1u);
    return proj_slots[slot % PROJ_SLOTS].used;
}
static void project_cur_name(char *b) { str_cpy(b, proj_current, PROJ_NAME_LEN + 1u); }
static int project_old(uint32_t slot) { (void)slot; return 0; }
static int project_dirty(void) { return 0; }
static int project_free_pct(void) { return 100; }
static int project_save_as(uint32_t slot, const char *name)
{
    str_cpy(link_save_name, name ? name : "", sizeof link_save_name);
    link_request(RQ_SAVE, slot % PROJ_SLOTS, 0, 0);
    return 0;
}
static int project_save(uint32_t slot) { return project_save_as(slot, 0); }
static void project_quick_save(void) { link_save_name[0] = 0; link_request(RQ_SAVE, 127u, 0, 0); }
static void project_autosave(void) {}
static void project_load(uint32_t slot) { link_request(RQ_LOAD, slot % PROJ_SLOTS, 0, 0); }
static void project_new_blank(void) { link_request(RQ_NEW, 0, 0, 0); }
static int project_erase(uint32_t slot) { (void)slot; ui_say("PROJECTS ", "ON MELODEE"); return 1; }
static int project_rename(uint32_t slot, const char *name) { (void)slot; (void)name; ui_say("PROJECTS ", "ON MELODEE"); return 1; }

static int template_used(void) { return 0; }
static int template_old(void) { return 0; }
static void template_save(void) { ui_say("TEMPLATE ", "ON MELODEE"); }
static void template_load(void) {}
static uint32_t template_tracks(void) { return NPART; }
static void template_key(uint32_t *root, uint32_t *scale, int32_t *bpm) { (void)root; (void)scale; (void)bpm; }

static void settings_save(void) {}
static void settings_old_accept(void) {}
static uint32_t chain_prepare_from(uint32_t start) { return start; }

static void track_add(void) { link_request(RQ_TRACK_ADD, trk_n(), 0, 0); }
static int track_create_to(uint32_t k) { link_request(RQ_TRACK_ADD, k, 0, 0); return 0; }
static int track_remove(uint32_t k) { link_request(RQ_TRACK_REMOVE, k, 0, 0); return 0; }
static void track_blank(uint32_t k) { (void)k; }
