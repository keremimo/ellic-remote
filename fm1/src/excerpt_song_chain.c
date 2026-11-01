/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Excerpts of Melodee FM-1's song_chain.c as they are (tools/sync_fm1.py): the song's rows as the pages read them (song_chain.c). */
static void chain_defaults(chain_config_t *c)
{
    uint32_t i;
    memset(c, 0, sizeof *c);
    for (i = 0; i < CHAIN_ROWS; i++)
        c->row[i].repeat = 1;
}
static uint32_t chain_origin(uint32_t row, uint32_t track)
{
    while (row && chain_patterns[row][track] == CHAIN_HOLD) row--;
    return row;
}
static uint32_t chain_bank(uint32_t row, uint32_t track)
{
    return chain_patterns[chain_origin(row, track)][track];
}
static uint32_t chain_block_end(uint32_t row, uint32_t track)
{
    while (row + 1u < chain_config.count && chain_patterns[row+1u][track] == CHAIN_HOLD) row++;
    return row + 1u;
}
static uint32_t chain_fx_count(uint32_t row)
{
    uint32_t n = 0;
    for (uint32_t j = 0; j < CHAIN_FX_EVENTS; j++) n += chain_config.fx[row][j].fx != 0;
    return n;
}
static const step_t *seq_steps(const track_t *t)
{
    return t->step;
}
