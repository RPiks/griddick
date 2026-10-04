/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_ifir_i32.c - This file is part of Griddick TNC.
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

#include "pixdsp_ifir_i32.h"

#include <math.h>
#include <string.h>

#ifdef PICO_ON_DEVICE
#define PIXDSP_IFIR_I32_FN(name) \
    __attribute__((section(".time_critical." #name), noinline)) name
#else
#define PIXDSP_IFIR_I32_FN(name) name
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * 27-tap Hamming LPF image mask, fc = 6000 Hz @ 48 kHz. Symmetric → GD = 13.
 */
static const float image_mask_taps[PIXDSP_IFIR_MASK_TAPS] = {
    -0.00138512f,  0.0f,          0.00271507f,  0.00622887f,
     0.00696970f,  0.0f,         -0.01558043f, -0.03158944f,
    -0.03165149f,  0.0f,          0.06634694f,  0.15076906f,
     0.22207046f,  0.25f,         0.22207046f,  0.15076906f,
     0.06634694f,  0.0f,         -0.03165149f, -0.03158944f,
    -0.01558043f,  0.0f,          0.00696970f,  0.00622887f,
     0.00271507f,  0.0f,         -0.00138512f
};

#define SPARSE_COEFF_Q   13
#define MASK_COEFF_Q     14
#define MASK_INPUT_SHIFT 16

static inline int32_t sat_lshift(int32_t v, int shift)
{
    int32_t hi = 2147483647 >> shift;

    if (v > hi) {
        return 2147483647;
    }
    if (v < -(hi + 1)) {
        return -2147483647 - 1;
    }
    return v << shift;
}

static int32_t quantize_tap(float h, int q)
{
    float scaled = h * (float)(1u << q);

    if (scaled >= 2147483647.0f) {
        scaled = 2147483647.0f;
    }
    if (scaled <= -2147483648.0f) {
        scaled = -2147483648.0f;
    }
    return (int32_t)scaled;
}

static int ifir_load(pixdsp_ifir_i32_t *s, const float *proto, int nproto,
                     const float *mask, int nmask)
{
    int i;
    int32_t coeff_sum;

    if (s == NULL || proto == NULL || mask == NULL) {
        return -1;
    }
    if (nproto <= 0 || (nproto & 1) == 0 ||
        (nproto * PIXDSP_IFIR_L) > PIXDSP_IFIR_SPARSE_MAX) {
        return -1;
    }
    if (nmask < 3 || (nmask & 1) == 0 || nmask > PIXDSP_IFIR_MASK_MAX) {
        return -1;
    }

    memset(s, 0, sizeof(*s));
    s->sparse_n = nproto * PIXDSP_IFIR_L;
    s->sparse_pos = s->sparse_n - 1;
    coeff_sum = 0;
    for (i = 0; i < nproto; i++) {
        int32_t c = quantize_tap(proto[i], SPARSE_COEFF_Q);

        s->sparse_c[i * PIXDSP_IFIR_L] = c;
        coeff_sum += (c >= 0) ? c : -c;
    }
    if (coeff_sum >= (1 << 16)) {
        return -1;
    }

    s->mask_n = nmask;
    s->mask_pos = nmask - 1;
    for (i = 0; i < nmask; i++) {
        s->mask_c[i] = quantize_tap(mask[i], MASK_COEFF_Q);
    }

    pixdsp_ifir_i32_reset(s);
    return 0;
}

int pixdsp_ifir_i32_init(pixdsp_ifir_i32_t *s, const float *h_sparse, int n)
{
    return ifir_load(s, h_sparse, n, image_mask_taps, PIXDSP_IFIR_MASK_TAPS);
}

static void bell202_proto(float *h, int n)
{
    int M = n - 1;
    float f1 = 1000.0f / 12000.0f;
    float f2 = 2400.0f / 12000.0f;
    double wc = 2.0 * M_PI * ((double)(f1 + f2) / 2.0);
    double re = 0.0;
    double im = 0.0;
    int i;

    for (i = 0; i < n; i++) {
        float x = (float)i - (float)M / 2.0f;
        float lp1;
        float lp2;

        if (x == 0.0f) {
            lp2 = 2.0f * f2;
            lp1 = 2.0f * f1;
        } else {
            lp2 = sinf(2.0f * (float)M_PI * f2 * x) / ((float)M_PI * x);
            lp1 = sinf(2.0f * (float)M_PI * f1 * x) / ((float)M_PI * x);
        }
        h[i] = (lp2 - lp1) *
               (0.54f - 0.46f * cosf(2.0f * (float)M_PI * (float)i / (float)M));
        re += (double)h[i] * cos(wc * (double)i);
        im += (double)h[i] * sin(wc * (double)i);
    }
    {
        float gain = (float)sqrt(re * re + im * im);

        for (i = 0; i < n; i++) {
            h[i] /= gain;
        }
    }
}

int pixdsp_ifir_i32_init_bell202(pixdsp_ifir_i32_t *s)
{
    float proto[PIXDSP_IFIR_PROTO_TAPS];

    if (s == NULL) {
        return -1;
    }
    bell202_proto(proto, PIXDSP_IFIR_PROTO_TAPS);
    return pixdsp_ifir_i32_init(s, proto, PIXDSP_IFIR_PROTO_TAPS);
}

/* Profile A edges @ 12 kHz proto rate. */
#define P1_F1 (1014.0f / 12000.0f)
#define P1_F2 (2386.0f / 12000.0f)
#define P1_MASK_FC (6000.0f / 48000.0f)

static void proto_trunc_bp(float *h, int n, float f1, float f2)
{
    int M = n - 1;
    double wc = 2.0 * M_PI * ((double)(f1 + f2) / 2.0);
    double re = 0.0;
    double im = 0.0;
    int i;

    for (i = 0; i < n; i++) {
        float x = (float)i - (float)M / 2.0f;
        float lp1;
        float lp2;

        if (x == 0.0f) {
            lp2 = 2.0f * f2;
            lp1 = 2.0f * f1;
        } else {
            lp2 = sinf(2.0f * (float)M_PI * f2 * x) / ((float)M_PI * x);
            lp1 = sinf(2.0f * (float)M_PI * f1 * x) / ((float)M_PI * x);
        }
        h[i] = lp2 - lp1;
        re += (double)h[i] * cos(wc * (double)i);
        im += (double)h[i] * sin(wc * (double)i);
    }
    {
        float gain = (float)sqrt(re * re + im * im);

        if (gain > 0.0f) {
            for (i = 0; i < n; i++) {
                h[i] /= gain;
            }
        }
    }
}

static void mask_hamming_lp(float *h, int n, float fc)
{
    int M = n - 1;
    float sum = 0.0f;
    int i;

    for (i = 0; i < n; i++) {
        float x = (float)i - (float)M / 2.0f;
        float sinc;
        float w;

        if (x == 0.0f) {
            sinc = 2.0f * fc;
        } else {
            sinc = sinf(2.0f * (float)M_PI * fc * x) / ((float)M_PI * x);
        }
        w = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * (float)i / (float)M);
        h[i] = sinc * w;
        sum += h[i];
    }
    if (sum > 0.0f) {
        for (i = 0; i < n; i++) {
            h[i] /= sum;
        }
    }
}

int pixdsp_ifir_i32_init_bell202_p1(pixdsp_ifir_i32_t *s)
{
    float proto[PIXDSP_IFIR_P1_PROTO_TAPS];
    float mask[PIXDSP_IFIR_P1_MASK_TAPS];

    if (s == NULL) {
        return -1;
    }
    proto_trunc_bp(proto, PIXDSP_IFIR_P1_PROTO_TAPS, P1_F1, P1_F2);
    mask_hamming_lp(mask, PIXDSP_IFIR_P1_MASK_TAPS, P1_MASK_FC);
    return ifir_load(s, proto, PIXDSP_IFIR_P1_PROTO_TAPS, mask,
                     PIXDSP_IFIR_P1_MASK_TAPS);
}

void pixdsp_ifir_i32_reset(pixdsp_ifir_i32_t *s)
{
    if (s == NULL) {
        return;
    }
    memset(s->sparse_d, 0, sizeof(s->sparse_d));
    memset(s->mask_d, 0, sizeof(s->mask_d));
    if (s->sparse_n > 0) {
        s->sparse_pos = s->sparse_n - 1;
    }
    if (s->mask_n > 0) {
        s->mask_pos = s->mask_n - 1;
    }
}

int32_t PIXDSP_IFIR_I32_FN(pixdsp_ifir_i32_process_fir_only)(pixdsp_ifir_i32_t *s,
                                                            int32_t x)
{
    int32_t *p_delay;
    int n;
    int half;
    int m;
    int32_t acc;

    s->sparse_pos++;
    if (s->sparse_pos >= s->sparse_n) {
        s->sparse_pos = 0;
    }
    s->sparse_d[s->sparse_pos] = x;
    s->sparse_d[s->sparse_pos + s->sparse_n] = x;
    p_delay = &s->sparse_d[s->sparse_pos];
    n = s->sparse_n / PIXDSP_IFIR_L;
    half = n / 2;
    acc = 0;
    for (m = 0; m < half; m++) {
        int i = m * PIXDSP_IFIR_L;

        acc += s->sparse_c[i] * (p_delay[i] + p_delay[(n - 1) * PIXDSP_IFIR_L - i]);
    }
    if (n & 1) {
        acc += s->sparse_c[half * PIXDSP_IFIR_L] * p_delay[half * PIXDSP_IFIR_L];
    }
    return sat_lshift(acc, 15 - SPARSE_COEFF_Q);
}

int32_t PIXDSP_IFIR_I32_FN(pixdsp_ifir_i32_process_mask_only)(pixdsp_ifir_i32_t *s,
                                                             int32_t x)
{
    int32_t xs;
    int32_t *p_delay;
    int n;
    int half;
    int m;
    int32_t acc;

    s->mask_pos++;
    if (s->mask_pos >= s->mask_n) {
        s->mask_pos = 0;
    }
    xs = x >> MASK_INPUT_SHIFT;
    s->mask_d[s->mask_pos] = xs;
    s->mask_d[s->mask_pos + s->mask_n] = xs;
    p_delay = &s->mask_d[s->mask_pos];
    n = s->mask_n;
    half = n / 2;
    acc = 0;
    for (m = 0; m < half; m++) {
        acc += s->mask_c[m] * (p_delay[m] + p_delay[n - 1 - m]);
    }
    if (n & 1) {
        acc += s->mask_c[half] * p_delay[half];
    }
    return sat_lshift(acc, MASK_INPUT_SHIFT - MASK_COEFF_Q);
}

int32_t PIXDSP_IFIR_I32_FN(pixdsp_ifir_i32_process)(pixdsp_ifir_i32_t *s,
                                                   int32_t x)
{
    return pixdsp_ifir_i32_process_mask_only(
        s, pixdsp_ifir_i32_process_fir_only(s, x));
}
