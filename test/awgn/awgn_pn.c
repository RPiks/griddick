/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 *
 * LICENCE: GNU Lesser General Public License v2.1 or later.
 * https://www.gnu.org/licenses/lgpl-2.1.html
 */
#include "awgn_pn.h"

#ifdef PICO
#include "pico.h"
#define AWGN_RAM __not_in_flash_func
#else
#define AWGN_RAM
#endif

const int8_t awgn_shift[AWGN_IDX_MAX + 1][3] = {
    { -1, -1, -1 }, /* 0 off */
    {  2,  3,  4 }, /* 1 ~3.3 dB */
    {  2,  3,  5 }, /* 2 ~4.0 */
    {  2,  3,  6 }, /* 3 ~4.3 */
    {  2,  3, -1 }, /* 4 ~4.7 */
    {  2,  4,  5 }, /* 5 ~5.9 */
    {  2,  4, -1 }, /* 6 ~6.4 */
    {  2,  5, -1 }, /* 7 ~7.7 */
    {  2,  6, -1 }, /* 8 ~8.2 */
};

uint32_t AWGN_RAM(awgn_xs32)(uint32_t *st)
{
    uint32_t x = *st;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *st = x;
    return x;
}

int32_t AWGN_RAM(awgn_raw)(uint32_t *st)
{
    int32_t a = 0;
    int i;

    for (i = 0; i < AWGN_PN_N; i++) {
        a += (int32_t)awgn_xs32(st) >> 16;
    }
    return a;
}

int32_t AWGN_RAM(awgn_scaled)(uint32_t *st, int idx)
{
    int32_t a;
    int32_t y = 0;
    int k;

    if (idx <= 0 || idx > AWGN_IDX_MAX) {
        return 0;
    }
    a = awgn_raw(st);
    for (k = 0; k < 3; k++) {
        int sh = awgn_shift[idx][k];

        if (sh >= 0) {
            y += a >> sh;
        }
    }
    return y;
}
