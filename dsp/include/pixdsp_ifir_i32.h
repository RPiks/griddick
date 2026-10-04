#ifndef PIXDSP_IFIR_I32_H
#define PIXDSP_IFIR_I32_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_ifir_i32.h - This file is part of Griddick TNC.
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

/**
 * @file pixdsp_ifir_i32.h
 * @brief All-int32 IFIR bandpass preselector (no int64). Cortex-M0+ hot path.
 *
 * int16-range in → sparse Hamming bandpass (L=4, 1000–2400 Hz, +90 dB into
 * Q30) → 27-tap Hamming image LPF → Q30 out. Both stages linear-phase;
 * GD = 252 + 13 = 265 samples at 48 kHz.
 *
 * Envelope: |x| ≤ 2^13 for the documented wrap-free budget. Init fails if
 * L1(sparse taps) ≥ 2^16.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PIXDSP_IFIR_L            4
#define PIXDSP_IFIR_PROTO_TAPS   127
#define PIXDSP_IFIR_MASK_TAPS    27
#define PIXDSP_IFIR_SPARSE_GD    252
#define PIXDSP_IFIR_MASK_GD      13 /* (27-1)/2 */
#define PIXDSP_IFIR_GD_SAMP \
    (PIXDSP_IFIR_SPARSE_GD + PIXDSP_IFIR_MASK_GD)
#define PIXDSP_IFIR_P1_PROTO_TAPS 165
#define PIXDSP_IFIR_P1_MASK_TAPS  39
#define PIXDSP_IFIR_P1_SPARSE_GD  328
#define PIXDSP_IFIR_P1_MASK_GD    19
#define PIXDSP_IFIR_SPARSE_MAX    (PIXDSP_IFIR_P1_PROTO_TAPS * PIXDSP_IFIR_L)
#define PIXDSP_IFIR_MASK_MAX     PIXDSP_IFIR_P1_MASK_TAPS

typedef struct pixdsp_ifir_i32 {
    int32_t sparse_c[PIXDSP_IFIR_SPARSE_MAX];
    int32_t sparse_d[2 * PIXDSP_IFIR_SPARSE_MAX];
    int sparse_n;
    int sparse_pos;
    int32_t mask_c[PIXDSP_IFIR_MASK_MAX];
    int32_t mask_d[2 * PIXDSP_IFIR_MASK_MAX];
    int mask_n;
    int mask_pos;
} pixdsp_ifir_i32_t;

/** @brief Load zero-stuffed proto (n taps → n*L sparse). Mask is built-in. */
int pixdsp_ifir_i32_init(pixdsp_ifir_i32_t *s, const float *h_sparse, int n);

/** @brief Hamming 127-tap 1000–2400 Hz proto @ 12 kHz, L=4, 27-tap mask. */
int pixdsp_ifir_i32_init_bell202(pixdsp_ifir_i32_t *s);

/** @brief P1: 165-tap truncated 1014–2386 Hz proto @ 12 kHz, L=4, 39-tap mask. */
int pixdsp_ifir_i32_init_bell202_p1(pixdsp_ifir_i32_t *s);

void pixdsp_ifir_i32_reset(pixdsp_ifir_i32_t *s);

int32_t pixdsp_ifir_i32_process_fir_only(pixdsp_ifir_i32_t *s, int32_t x);
int32_t pixdsp_ifir_i32_process_mask_only(pixdsp_ifir_i32_t *s, int32_t x);
int32_t pixdsp_ifir_i32_process(pixdsp_ifir_i32_t *s, int32_t x);

#ifdef __cplusplus
}
#endif

#endif /* PIXDSP_IFIR_I32_H */
