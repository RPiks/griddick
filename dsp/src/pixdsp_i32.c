/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_i32.c - This file is part of Griddick TNC.
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

#include "pixdsp_i32.h"

#include <math.h>
#include <stdint.h>

int32_t pixdsp_f32_to_q30(float x)
{
    double y;

    y = (double)x * (double)PIXDSP_I32_ONE;
    if (y > 2147483647.0) {
        return 2147483647;
    }
    if (y < -2147483648.0) {
        return (int32_t)(-2147483647 - 1);
    }
    return (int32_t)llround(y);
}

float pixdsp_q30_to_f32(int32_t x)
{
    return (float)((double)x / (double)PIXDSP_I32_ONE);
}

int32_t pixdsp_f32_to_q31(float h)
{
    double y;

    y = (double)h * 2147483648.0;
    if (y > 2147483647.0) {
        return 2147483647;
    }
    if (y < -2147483648.0) {
        return (int32_t)(-2147483647 - 1);
    }
    return (int32_t)llround(y);
}
