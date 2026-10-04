/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_dpll.c - This file is part of Griddick TNC.
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

#include "pixdsp_dpll.h"

#include <math.h>
#include <string.h>

static int32_t inertia_to_q(float a)
{
    int32_t q;

    q = (int32_t)llround((double)a * 10000.0);
    if (q < 1) {
        q = 1;
    }
    if (q > 10000) {
        q = 10000;
    }
    return q;
}

static int popcount32(uint32_t x)
{
    int n = 0;

    while (x != 0u) {
        n += (int)(x & 1u);
        x >>= 1;
    }
    return n;
}

static void dcd_each_symbol(pixdsp_dpll_t *s)
{
    int good_minus_bad;
    int n;

    s->good_hist = (uint8_t)((s->good_hist << 1) | (uint8_t)(s->good_flag != 0));
    s->good_flag = 0;
    s->bad_hist = (uint8_t)((s->bad_hist << 1) | (uint8_t)(s->bad_flag != 0));
    s->bad_flag = 0;

    good_minus_bad = popcount32(s->good_hist) - popcount32(s->bad_hist);
    s->score = (s->score << 1) | (uint32_t)(good_minus_bad >= 2);
    n = popcount32(s->score);
    if (n >= PIXDSP_DPLL_DCD_THRESH_ON) {
        s->data_detect = 1;
    } else if (n <= PIXDSP_DPLL_DCD_THRESH_OFF) {
        s->data_detect = 0;
    }
}

static void dcd_on_transition(pixdsp_dpll_t *s)
{
    if (s->pll > -PIXDSP_DPLL_DCD_GOOD_ABS && s->pll < PIXDSP_DPLL_DCD_GOOD_ABS) {
        s->good_flag = 1;
    } else {
        s->bad_flag = 1;
    }
}

int pixdsp_dpll_init(pixdsp_dpll_t *s, const pixdsp_dpll_params_t *params)
{
    double st;

    if (s == NULL || params == NULL) {
        return -1;
    }
    if (params->sample_rate_hz < 1.0f || params->baud < 1.0f) {
        return -1;
    }
    if (params->sample_rate_hz < 2.0f * params->baud) {
        return -1;
    }
    if (params->inertia > 1.0f || params->searching_inertia > 1.0f) {
        return -1;
    }
    st = 4294967296.0 * (double)params->baud / (double)params->sample_rate_hz;
    if (st < 1.0 || st >= 2147483648.0) {
        return -1;
    }
    memset(s, 0, sizeof(*s));
    s->step = (int32_t)llround(st);
    if (s->step < 1) {
        return -1;
    }
    if (params->inertia > 0.0f) {
        s->inertia = params->inertia;
    } else {
        s->inertia = PIXDSP_DPLL_INERTIA_LOCKED;
    }
    if (params->searching_inertia > 0.0f) {
        s->searching_inertia = params->searching_inertia;
    } else {
        s->searching_inertia = PIXDSP_DPLL_INERTIA_SEARCHING;
    }
    /* int32 NCO * float32 rounds by ~2^8 near wrap and slips a sample.
     * Direwolf nudges at pll≈0 (float is exact there); we may not. */
    s->locked_q = inertia_to_q(s->inertia);
    s->searching_q = inertia_to_q(s->searching_inertia);
    s->initialized = 1;
    return 0;
}

void pixdsp_dpll_reset(pixdsp_dpll_t *s)
{
    if (s == NULL || !s->initialized) {
        return;
    }
    s->pll = 0;
    s->prev_sign = 0;
    s->have_prev = 0;
    s->good_flag = 0;
    s->bad_flag = 0;
    s->good_hist = 0;
    s->bad_hist = 0;
    s->score = 0;
    s->data_detect = 0;
}

int pixdsp_dpll_seed_phase(pixdsp_dpll_t *s, int phase_samples)
{
    uint32_t d;

    if (s == NULL || !s->initialized || phase_samples < 0) {
        return -1;
    }
    d = (uint32_t)s->step * (uint32_t)phase_samples;
    s->pll = (int32_t)((uint32_t)s->pll - d);
    return 0;
}

int pixdsp_dpll_dcd(const pixdsp_dpll_t *s)
{
    if (s == NULL || !s->initialized) {
        return 0;
    }
    return s->data_detect != 0;
}

#ifdef PICO_ON_DEVICE
__attribute__((section(".time_critical.pixdsp_dpll"), noinline))
#endif
int pixdsp_dpll_process(pixdsp_dpll_t *s, int32_t demod_out, int *strobe,
                        int *bit)
{
    int32_t prev;
    int32_t q;
    int cur;
    int fire;

    if (s == NULL || !s->initialized || strobe == NULL || bit == NULL) {
        return -1;
    }
    cur = (demod_out > 0) ? 1 : 0;
    prev = s->pll;
    s->pll = (int32_t)((uint32_t)s->pll + (uint32_t)s->step);
    fire = (s->pll < 0 && prev > 0) ? 1 : 0;
    if (fire) {
        *bit = cur;
        dcd_each_symbol(s);
    } else {
        *bit = 0;
    }
    if (s->have_prev && cur != s->prev_sign) {
        dcd_on_transition(s);
        q = s->data_detect ? s->locked_q : s->searching_q;
        if (q < 10000) {
            s->pll = (int32_t)(((int64_t)s->pll * (int64_t)q) / 10000);
        }
    }
    s->prev_sign = cur;
    s->have_prev = 1;
    *strobe = fire;
    return 0;
}
