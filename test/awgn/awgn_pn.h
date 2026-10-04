/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 *
 * Shared xorshift-sum AWGN + shift-sum gain. Host mixer now; Pico later.
 * No integer division. Index 0 = off.
 *
 * LICENCE: GNU Lesser General Public License v2.1 or later.
 * https://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef AWGN_PN_H
#define AWGN_PN_H

#include <stdint.h>

#define AWGN_PN_N       6
#define AWGN_IDX_MAX    8

/* idx 1..AWGN_IDX_MAX: y = (raw>>s0) + (raw>>s1) + (raw>>s2), s<0 skipped. */
extern const int8_t awgn_shift[AWGN_IDX_MAX + 1][3];

uint32_t awgn_xs32(uint32_t *st);
int32_t awgn_raw(uint32_t *st);
int32_t awgn_scaled(uint32_t *st, int idx);

#endif
