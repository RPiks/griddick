/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_agc_i16.c - This file is part of Griddick TNC.
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

#include "pixdsp_agc_i16.h"

#include <string.h>

static int16_t sat_i16(int32_t v)
{
    if (v > 32767) {
        return 32767;
    }
    if (v < -32767) {
        return -32767;
    }
    return (int16_t)v;
}

static int16_t mix_attack(int16_t in, int16_t state)
{
    int32_t d = (int32_t)in - (int32_t)state;

    return sat_i16((int32_t)state + (d >> 1) + (d >> 3) + (d >> 4));
}

static int16_t mix_decay(int16_t in, int16_t state, int16_t *acc)
{
    int32_t a = (int32_t)*acc + ((int32_t)in - (int32_t)state);
    int32_t step;

    a = sat_i16(a);
    step = a >> 13;
    *acc = (int16_t)(a - (step << 13));
    return sat_i16((int32_t)state + step);
}

int pixdsp_agc_i16_init(pixdsp_agc_i16_t *s)
{
    if (s == NULL) {
        return -1;
    }
    memset(s, 0, sizeof(*s));
    s->initialized = 1;
    return 0;
}

void pixdsp_agc_i16_reset(pixdsp_agc_i16_t *s)
{
    if (s == NULL || !s->initialized) {
        return;
    }
    s->peak = 0;
    s->valley = 0;
    s->dec_acc_p = 0;
    s->dec_acc_v = 0;
}

#ifdef PICO_ON_DEVICE
__attribute__((section(".time_critical.pixdsp_agc_i16"), noinline))
#endif
int pixdsp_agc_i16_process(pixdsp_agc_i16_t *s, int32_t in, int16_t *out)
{
    int16_t xin;
    int16_t x;
    int16_t mid;
    int16_t span;

    if (s == NULL || !s->initialized || out == NULL) {
        return -1;
    }
    xin = sat_i16(in);
    if (xin < 0) {
        xin = 0;
    }
    if (xin >= s->peak) {
        s->peak = mix_attack(xin, s->peak);
        s->dec_acc_p = 0;
    } else {
        s->peak = mix_decay(xin, s->peak, &s->dec_acc_p);
    }
    if (xin <= s->valley) {
        s->valley = mix_attack(xin, s->valley);
        s->dec_acc_v = 0;
    } else {
        s->valley = mix_decay(xin, s->valley, &s->dec_acc_v);
    }
    if (s->valley < 0) {
        s->valley = 0;
    }
    if (s->peak < s->valley) {
        s->peak = s->valley;
    }

    x = xin;
    if (x > s->peak) {
        x = s->peak;
    }
    if (x < s->valley) {
        x = s->valley;
    }
    span = (int16_t)(s->peak - s->valley);
    if (span <= 0) {
        *out = 0;
        return 0;
    }
    mid = (int16_t)(((int32_t)s->peak + (int32_t)s->valley) >> 1);
    *out = (int16_t)(((int32_t)(x - mid) << 15) / (int32_t)span);
    return 0;
}
