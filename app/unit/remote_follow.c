/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* One screen with Ellic Studio's, and the menu's settings kept by Ellic Studio (docs/PROTOCOL.md PAGE, PREFS).
 *   PAGE: the page shown, as a screen (Stage, a sound page, the key, the arpeggiator, the steps, PATTERNS, SONG, the
 *     mixer, TEMPO, the menu), told to Ellic Studio whenever the remote's controls change it; Ellic Studio's (the iPad moved)
 *     taken here as a page to go to. Neither side answers the other's: the last touched leads.
 *   PREFS: the remote keeps nothing; its menu's settings and Ellic Studio's own the menu shows (the click, the count-in ..)
 *     are Ellic Studio's, told on claim and told back as the menu changes them. */
static void link_send(uint32_t cmd, const uint8_t *body, uint32_t n, int port);   /* (link.c) */
static uint32_t wr14(uint8_t *p, int32_t v);
static int link_held(void);
#define L_PAGE_CMD 0x05u                 /* (link.c L_PAGE, L_PREFS) */
#define L_PREFS_CMD 0x06u
enum { SCR_STAGE, SCR_SOUND, SCR_SCALE, SCR_ARP, SCR_NOTES, SCR_PATTERNS, SCR_SONG, SCR_MIX, SCR_TEMPO, SCR_MENU };
enum { PREF_COLOR, PREF_FONT, PREF_HOLD, PREF_LIGHTS, PREF_PICKER, PREF_ACCEL, PREF_HOME_SEL, PREF_GLO_SEL, PREF_TRACKS,
       PREF_SCREEN, PREF_OWN, PREF_CLICK = 32, PREF_CLICK_LEVEL, PREF_COUNTIN, PREF_PREVIEW, PREF_CHORD_ADD, PREF_N };

static struct {
    uint8_t shown[4];                    /* screen, track, family, page: the last told or heard */
    uint8_t ready;                       /* (shown is) */
    int16_t prefs[PREF_N];               /* the preferences as last told or heard */
    uint8_t prefs_ready;
} fol;

/* the page shown as PAGE says it: screen, track, family (1..4 a sound page's, 5 PRESETS), its place in the told set */
static void page_now(uint8_t s[4])
{
    const page_t *pg = cur_page();
    s[1] = song.sel;
    s[2] = 0;
    s[3] = 127;
    if (ui.menu)
        s[0] = SCR_MENU;
    else if (ui.home)
        s[0] = SCR_STAGE;
    else if (pg->scope == SC_TOLD) {
        static const uint8_t TF[FAM_COUNT] = {[FAM_EDIT] = TF_EDIT, [FAM_ENV] = TF_ENV, [FAM_LFO] = TF_LFO, [FAM_FX] = TF_FX};
        s[0] = SCR_SOUND;
        s[2] = pg->fam < FAM_COUNT ? TF[pg->fam] : 0;
        s[3] = (uint8_t)(ui.page - told_first);
    } else if (pg->graph == GR_BROWSE) {
        s[0] = SCR_SOUND;
        s[2] = 5;
    } else if (pg->fam == FAM_SCL)
        s[0] = SCR_SCALE;
    else if (pg->fam == FAM_ARP)
        s[0] = SCR_ARP;
    else if (pg->fam == FAM_TRK)
        s[0] = SCR_MIX;
    else if (pg->graph == GR_PATGRID)
        s[0] = SCR_PATTERNS;
    else if (pg->graph == GR_SONG)
        s[0] = SCR_SONG;
    else if (pg->graph == GR_ROLL || pg->graph == GR_STEPS)
        s[0] = SCR_NOTES;
    else
        s[0] = SCR_TEMPO;
}

/* the remote's page changed by its own controls: told (a frame's pass, link.c) */
static void page_poll(void)
{
    uint8_t s[4], m[24];
    uint32_t n;
    const char *title;
    page_now(s);
    if (fol.ready && !memcmp(s, fol.shown, sizeof s))
        return;
    memcpy(fol.shown, s, sizeof s);
    fol.ready = 1;
    title = ui.home || ui.menu ? "" : cur_page()->title;
    memcpy(m, s, 4);
    for (n = 0; title[n] && n < 12u; n++)
        m[4 + n] = (uint8_t)title[n];
    m[4 + n] = 0;
    link_send(L_PAGE_CMD, m, 5u + n, -1);
}

/* page i shown, as its button would */
static void page_show(uint32_t i)
{
    if (i >= NPAGES || !page_visible(i))
        return;
    ui.home = 0;
    ui.page = (uint8_t)i;
    if (PAGES[i].fam < FAM_COUNT)
        ui.fam_last[PAGES[i].fam] = (uint8_t)i;
    page_entered();
    ui.force = 1;
}
static uint32_t page_by_graph(uint32_t graph)
{
    uint32_t i;
    for (i = 0; i < NPAGES && PAGES[i].graph != graph; i++)
        ;
    return i;
}

/* Ellic Studio's screen (the iPad moved): the remote there, unless its menu, a dialog or a sheet is open; not told back */
static void page_follow(const uint8_t *b)
{
    static const uint8_t FAM[TF_N] = {FAM_HOME, FAM_EDIT, FAM_ENV, FAM_LFO, FAM_FX};
    if (ui.menu || ui.confirm || pop.on || ui.layer)
        return;
    switch (b[0]) {
    case SCR_STAGE:
        if (!ui.home)
            go_home();
        break;
    case SCR_SOUND:
        if (b[2] == 5u)
            page_show(page_by_graph(GR_BROWSE));
        else if (b[3] < TOLD_PAGES && told_visible(told_first + b[3]))
            page_show(told_first + b[3]);
        else if (b[2] >= 1u && b[2] < TF_N && (ui.home || cur_page()->fam != FAM[b[2]]))
            open_family(FAM[b[2]]);
        break;
    case SCR_SCALE:
        if (ui.home || cur_page()->fam != FAM_SCL)
            open_family(FAM_SCL);
        break;
    case SCR_ARP:
        if (ui.home || cur_page()->fam != FAM_ARP)
            open_family(FAM_ARP);
        break;
    case SCR_NOTES:
        if (ui.home || (cur_page()->graph != GR_ROLL && cur_page()->graph != GR_STEPS))
            page_show(page_by_graph(GR_ROLL));
        break;
    case SCR_PATTERNS:
        page_show(page_by_graph(GR_PATGRID));
        break;
    case SCR_SONG:
        page_show(page_by_graph(GR_SONG));
        break;
    case SCR_MIX:
        if (ui.home || cur_page()->fam != FAM_TRK)
            open_global();
        break;
    default:
        return;
    }
    page_now(fol.shown);                     /* (where it went is Ellic Studio's: not told back) */
    fol.ready = 1;
}

/* ------------------------------------------------------------ PREFS --- */
static int32_t pref_get(uint32_t id)
{
    switch (id) {
    case PREF_COLOR: return (int32_t)settings.palette;
    case PREF_FONT: return (PREF_BITS & PREF_PIXEL) != 0u;
    case PREF_HOLD: return settings_hold % 4u;
    case PREF_LIGHTS: return settings_lights % LIGHTS_N;
    case PREF_PICKER: return !(PREF_BITS & PREF_PICK_OFF);
    case PREF_ACCEL: return (PREF_BITS & PREF_ACCEL_OFF) != 0u;
    case PREF_HOME_SEL: return (int32_t)home_select();
    case PREF_GLO_SEL: return (int32_t)glo_select();
    case PREF_TRACKS: return !(GLO_SELECT & TRACKS_HIDE);
    case PREF_SCREEN: return (int32_t)scr_get();
    case PREF_CLICK: return settings_click % 3u;
    case PREF_CLICK_LEVEL: return settings_click_level;
    case PREF_COUNTIN: return settings_countin % 3u;
    case PREF_PREVIEW: return settings_preview != 0u;
    case PREF_CHORD_ADD: return settings_chord_add != 0u;
    default: return 0;
    }
}
/* a preference as Ellic Studio keeps it (PREFS in), at once */
static void pref_set(uint32_t id, int32_t v)
{
    switch (id) {
    case PREF_COLOR:
        settings.palette = (uint32_t)clamp(v, 0, NPALETTES - 1);
        palette_set(settings.palette);
        break;
    case PREF_FONT: PREF_BITS = (uint8_t)(v ? PREF_BITS | PREF_PIXEL : PREF_BITS & ~PREF_PIXEL); palette_set(settings.palette); break;
    case PREF_HOLD: settings_hold = (uint8_t)clamp(v, 0, 3); break;
    case PREF_LIGHTS: settings_lights = (uint8_t)clamp(v, 0, LIGHTS_N - 1); break;
    case PREF_PICKER: PREF_BITS = (uint8_t)(v ? PREF_BITS & ~PREF_PICK_OFF : PREF_BITS | PREF_PICK_OFF); break;
    case PREF_ACCEL: PREF_BITS = (uint8_t)(v ? PREF_BITS | PREF_ACCEL_OFF : PREF_BITS & ~PREF_ACCEL_OFF); break;
    case PREF_HOME_SEL: HOME_SELECT = (uint8_t)clamp(v, 0, HS_N - 1); break;
    case PREF_GLO_SEL: glo_select_set((uint32_t)clamp(v, 0, HS_N - 1)); break;
    case PREF_TRACKS: GLO_SELECT = (uint8_t)(v ? GLO_SELECT & ~TRACKS_HIDE : GLO_SELECT | TRACKS_HIDE); break;
    case PREF_SCREEN: scr_put((uint32_t)clamp(v, 0, 4)); break;
    case PREF_CLICK: settings_click = (uint8_t)clamp(v, 0, 2); break;
    case PREF_CLICK_LEVEL: settings_click_level = (uint8_t)clamp(v, 0, 100); break;
    case PREF_COUNTIN: settings_countin = (uint8_t)clamp(v, 0, 2); break;
    case PREF_PREVIEW: settings_preview = (uint8_t)(v != 0); break;
    case PREF_CHORD_ADD: settings_chord_add = (uint8_t)(v != 0); break;
    default: return;
    }
    if (id < PREF_N)
        fol.prefs[id] = (int16_t)pref_get(id);   /* (heard: not told back) */
    ui.force = 1;
}
static int pref_known(uint32_t id) { return id < PREF_OWN || (id >= PREF_CLICK && id < PREF_N); }

/* the menu changed a preference: told (all of them, the first pass after a claim: Ellic Studio's then tell it back) */
static void prefs_poll(void)
{
    uint8_t m[60];
    uint32_t id, n = 0;
    for (id = 0; id < PREF_N && n + 3u <= sizeof m; id++)
        if (pref_known(id) && (!fol.prefs_ready || fol.prefs[id] != pref_get(id))) {
            int32_t v = pref_get(id);
            fol.prefs[id] = (int16_t)v;
            m[n++] = (uint8_t)id;
            n += wr14(m + n, v);
        }
    fol.prefs_ready = 1;
    if (n && link_held())
        link_send(L_PREFS_CMD, m, n, -1);
}
/* a new claim: Ellic Studio hears where the remote is; its preferences come from Ellic Studio (PREFS), not the other way */
static void follow_claimed(void)
{
    uint32_t id;
    fol.ready = 0;
    for (id = 0; id < PREF_N; id++)
        if (pref_known(id))
            fol.prefs[id] = (int16_t)pref_get(id);
    fol.prefs_ready = 1;
}
