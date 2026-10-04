/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * kiss_tx.c - This file is part of Griddick TNC.
 *
 * DESCRIPTION
 *     The Griddick project provides off-grid comm function using a cheap
 * FM handheld radio and Pi Pico board.
 *
 * PLATFORM
 *     Raspberry Pi Pico (or Linux for host utils)
 *
 * PROJECT PAGE
 *     https://github.com/RPiks/griddick
 *
 * LICENCE
 *
 * Griddick TNC is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * See the GNU Lesser General Public License for more details.
 * https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "kiss_tx.h"

#include <string.h>

#define STAGE_IDLE  0
#define STAGE_LEAD  1
#define STAGE_BODY  2
#define STAGE_TRAIL 3
#define STAGE_DONE  4

#define MARK_HZ  1200u
#define SPACE_HZ 2200u
#define PEAK     262 /* round(0.35 * 750) */

static const int16_t k_cos_q15[256] = {
    32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
    32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
    30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
    27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
    23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
    18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
    12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
     6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
        0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
    -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
   -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
   -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
   -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
   -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
   -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
   -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
   -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
   -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
   -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
   -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
   -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
   -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
   -12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
    -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804,
        0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
     6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
    12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
    18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
    23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
    27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
    30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
    32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
};

static uint32_t step_for_hz(uint32_t hz)
{
    return (uint32_t)((((uint64_t)hz << 32) + (KISS_TX_FS_HZ / 2u)) /
                      (uint32_t)KISS_TX_FS_HZ);
}

static int clamp_flags(int n)
{
    if (n < 1) {
        return 1;
    }
    if (n > KISS_TX_LEAD_MAX) {
        return KISS_TX_LEAD_MAX;
    }
    return n;
}

void kiss_tx_init(kiss_tx_t *s)
{
    if (s == NULL) {
        return;
    }
    memset(s, 0, sizeof(*s));
}

int kiss_tx_flag_count_10ms(uint8_t n_10ms)
{
    /* ceil(n * 1.5) = (3n + 1) / 2 */
    unsigned f = ((unsigned)n_10ms * 3u + 1u) / 2u;

    return clamp_flags((int)f);
}

uint16_t kiss_tx_fcs(const uint8_t *data, int len)
{
    uint16_t crc = 0xFFFFu;
    int i;
    int b;

    if (data == NULL || len < 0) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (b = 0; b < 8; b++) {
            if (crc & 1u) {
                crc = (uint16_t)((crc >> 1) ^ 0x8408u);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return (uint16_t)(crc ^ 0xFFFFu);
}

int kiss_tx_start(kiss_tx_t *s, const uint8_t *payload, int n,
                  int lead_flags, int trail_flags)
{
    uint16_t fcs;

    if (s == NULL || payload == NULL || n < 1 || n > KISS_TX_PAYLOAD_MAX) {
        return -1;
    }
    memcpy(s->body, payload, (size_t)n);
    fcs = kiss_tx_fcs(payload, n);
    s->body[n] = (uint8_t)(fcs & 0xffu);
    s->body[n + 1] = (uint8_t)(fcs >> 8);
    s->body_len = n + 2;
    s->lead_flags = clamp_flags(lead_flags);
    s->trail_flags = clamp_flags(trail_flags);
    s->stage = STAGE_LEAD;
    s->flags_left = s->lead_flags;
    s->byte_i = 0;
    s->bit_i = 0;
    s->ones = 0;
    s->stuff_pending = 0;
    s->nrzi_tone = 0;
    s->sample_in_sym = 0;
    s->phase = 0;
    s->step = step_for_hz(MARK_HZ);
    s->active = 1;
    return 0;
}

int kiss_tx_active(const kiss_tx_t *s)
{
    return (s != NULL && s->active) ? 1 : 0;
}

static int next_dbit(kiss_tx_t *s)
{
    static const uint8_t flag_bits[8] = {0, 1, 1, 1, 1, 1, 1, 0};

    for (;;) {
        int bit;

        if (s->stage == STAGE_DONE || s->stage == STAGE_IDLE) {
            return -1;
        }
        if (s->stuff_pending) {
            s->stuff_pending = 0;
            s->ones = 0;
            return 0;
        }
        if (s->stage == STAGE_LEAD || s->stage == STAGE_TRAIL) {
            bit = flag_bits[s->bit_i];
            s->bit_i++;
            if (s->bit_i >= 8) {
                s->bit_i = 0;
                s->ones = 0;
                s->flags_left--;
                if (s->flags_left <= 0) {
                    if (s->stage == STAGE_LEAD) {
                        s->stage = STAGE_BODY;
                        s->byte_i = 0;
                        s->bit_i = 0;
                        s->ones = 0;
                    } else {
                        s->stage = STAGE_DONE;
                    }
                }
            }
            return bit;
        }
        if (s->byte_i >= s->body_len) {
            s->stage = STAGE_TRAIL;
            s->flags_left = s->trail_flags;
            s->bit_i = 0;
            s->ones = 0;
            continue;
        }
        bit = (s->body[s->byte_i] >> s->bit_i) & 1;
        s->bit_i++;
        if (s->bit_i >= 8) {
            s->bit_i = 0;
            s->byte_i++;
        }
        if (bit) {
            s->ones++;
            if (s->ones == 5) {
                s->stuff_pending = 1;
            }
        } else {
            s->ones = 0;
        }
        return bit;
    }
}

static uint16_t level_from_phase(uint32_t phase)
{
    unsigned i0;
    unsigned i1;
    int32_t c0;
    int32_t c1;
    int32_t frac;
    int32_t c;
    int32_t y;

    i0 = (unsigned)(phase >> 24);
    i1 = (i0 + 1u) & 255u;
    frac = (int32_t)((phase >> 16) & 0xffu);
    c0 = k_cos_q15[i0];
    c1 = k_cos_q15[i1];
    c = c0 + (((c1 - c0) * frac) >> 8);
    y = (c * PEAK) >> 15;
    return (uint16_t)((int32_t)KISS_TX_MID + y);
}

int kiss_tx_next(kiss_tx_t *s, uint16_t *level)
{
    int dbit;

    if (s == NULL || level == NULL || !s->active) {
        return 0;
    }
    if (s->sample_in_sym == 0) {
        dbit = next_dbit(s);
        if (dbit < 0) {
            s->active = 0;
            s->stage = STAGE_IDLE;
            return 0;
        }
        if (dbit == 0) {
            s->nrzi_tone ^= 1;
        }
        s->step = step_for_hz(s->nrzi_tone ? SPACE_HZ : MARK_HZ);
    }
    *level = level_from_phase(s->phase);
    s->phase += s->step;
    s->sample_in_sym++;
    if (s->sample_in_sym >= KISS_TX_SPS) {
        s->sample_in_sym = 0;
    }
    return 1;
}
