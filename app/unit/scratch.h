/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The main loop's scratch (Melodee FM-1's scratch.h is SLOOP's, not the remote's): the scope's snapshot, all the
 * remote's pages use of it. */
#pragma once
static struct { int16_t snap[SCOPE_N]; } main_scratch;
