/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Editing a CZ-1 tone on its pages (Melodee FM-1's cz_edit.h) is not on the remote: no track is one. */
#pragma once
static const param_desc_t CZ_ACTIONS[4];
static int16_t cz_ed_cell[4];
static int cz_ed_active(const uint8_t *d, uint32_t id) { (void)d; (void)id; return 0; }
static const param_desc_t *cz_ed_desc(uint32_t tr, uint32_t id, uint32_t slot, int16_t **valp)
{
    (void)tr; (void)id; (void)slot;
    *valp = 0;
    return 0;
}
static int cz_ed_put(uint32_t tr, uint32_t id, uint32_t v, uint8_t *raw) { (void)tr; (void)id; (void)v; (void)raw; return 0; }
