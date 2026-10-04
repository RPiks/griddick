/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_hypot_i32.c - This file is part of Griddick TNC.
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

#include "pixdsp_hypot_i32.h"

#ifdef PICO_ON_DEVICE
__attribute__((section(".time_critical.pixdsp_hypot_i32"), noinline))
#endif
int32_t pixdsp_hypot_i32(int32_t a, int32_t b)
{
    uint32_t ua;
    uint32_t ub;
    uint32_t mx;
    uint32_t mn;
    int32_t i;
    int32_t q;
    int32_t ca;
    int32_t cb;
    int32_t x;
    int sh;

    ua = (uint32_t)a;
    if (a < 0) {
        ua = ~ua + 1u;
    }
    ub = (uint32_t)b;
    if (b < 0) {
        ub = ~ub + 1u;
    }
    if (ua >= ub) {
        mx = ua;
        mn = ub;
    } else {
        mx = ub;
        mn = ua;
    }
    sh = 0;
    while (mx > 32767u) {
        mx >>= 1;
        mn >>= 1;
        sh++;
    }
    if (mx == 0u) {
        return 0;
    }
    i = (int32_t)mx;
    q = (int32_t)mn;
    if (4 * q <= i) {
        ca = 4096;
        cb = 504;
    } else if (2 * q <= i) {
        ca = 3865;
        cb = 1430;
    } else if (4 * q <= 3 * i) {
        ca = 3498;
        cb = 2162;
    } else {
        ca = 3102;
        cb = 2690;
    }
    x = ca * i + cb * q;
    x = ((1 << 11) + x) >> 12;
    if (sh == 0) {
        return x;
    }
    if (x > (2147483647 >> sh)) {
        return 2147483647;
    }
    return x << sh;
}
