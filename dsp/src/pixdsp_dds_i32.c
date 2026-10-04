/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_dds_i32.c - This file is part of Griddick TNC.
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

#include "pixdsp_dds_i32.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DDS_LUT_N      1024
#define DDS_LUT_SHIFT  22
#define DDS_LUT_MASK   (DDS_LUT_N - 1)
#define DDS_FRAC_MASK  ((1u << DDS_LUT_SHIFT) - 1u)

static int32_t s_sin_q31[DDS_LUT_N];
static int s_lut_ready;

static void lut_init(void)
{
    int i;

    if (s_lut_ready) {
        return;
    }
    for (i = 0; i < DDS_LUT_N; i++) {
        double th = 2.0 * M_PI * (double)i / (double)DDS_LUT_N;

        s_sin_q31[i] = pixdsp_f32_to_q31((float)sin(th));
    }
    s_lut_ready = 1;
}

static int32_t sin_q31(uint32_t phase)
{
    unsigned i0;
    unsigned i1;
    uint32_t frac;
    int32_t s0;
    int32_t s1;

    i0 = (unsigned)(phase >> DDS_LUT_SHIFT) & DDS_LUT_MASK;
    i1 = (i0 + 1u) & DDS_LUT_MASK;
    frac = phase & DDS_FRAC_MASK;
    s0 = s_sin_q31[i0];
    s1 = s_sin_q31[i1];
    return s0 + (int32_t)(((int64_t)(s1 - s0) * (int64_t)frac) >> DDS_LUT_SHIFT);
}

static uint32_t rad_to_phase(float rad)
{
    double t;
    double two_pi = 2.0 * M_PI;

    t = fmod((double)rad, two_pi);
    if (t < 0.0) {
        t += two_pi;
    }
    return (uint32_t)llround(t * 4294967296.0 / two_pi);
}

int pixdsp_dds_i32_init(pixdsp_dds_i32_t *s, const pixdsp_dds_params_t *params)
{
    double mag;
    uint32_t umag;
    float nyquist;

    if (s == NULL || params == NULL) {
        return -1;
    }
    if (params->sample_rate_hz <= 0.0f) {
        return -1;
    }
    if (params->mode != PIXDSP_DDS_MIX_DOWN &&
        params->mode != PIXDSP_DDS_MIX_UP) {
        return -1;
    }
    nyquist = params->sample_rate_hz * 0.5f;
    if (fabsf(params->lo_freq_hz) >= nyquist) {
        return -1;
    }

    lut_init();
    mag = 4294967296.0 * (double)params->lo_freq_hz / (double)params->sample_rate_hz;
    if (mag < 0.0) {
        mag = -mag;
    }
    if (mag >= 4294967296.0) {
        return -1;
    }
    umag = (uint32_t)llround(mag);
    memset(s, 0, sizeof(*s));
    s->mode = params->mode;
    s->step = (params->mode == PIXDSP_DDS_MIX_DOWN) ? (0u - umag) : umag;
    s->phase0 = rad_to_phase(params->lo_phase_rad);
    s->phase = s->phase0;
    s->initialized = 1;
    return 0;
}

void pixdsp_dds_i32_reset(pixdsp_dds_i32_t *s)
{
    if (s == NULL || !s->initialized) {
        return;
    }
    s->phase = s->phase0;
}

void pixdsp_dds_i32_process(pixdsp_dds_i32_t *s, int32_t x, int32_t *re,
                            int32_t *im)
{
    int32_t c;
    int32_t sn;
    int32_t xr;
    int32_t xi;

    sn = sin_q31(s->phase);
    c = sin_q31(s->phase + 0x40000000u);
    xr = (int32_t)(((int64_t)x * (int64_t)c) >> 31);
    xi = (int32_t)(((int64_t)x * (int64_t)sn) >> 31);
    *re = xr;
    if (s->mode == PIXDSP_DDS_MIX_DOWN) {
        *im = -xi;
    } else {
        *im = xi;
    }
    s->phase += s->step;
}
