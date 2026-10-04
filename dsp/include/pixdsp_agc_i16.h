#ifndef PIXDSP_AGC_I16_H
#define PIXDSP_AGC_I16_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_agc_i16.h - This file is part of Griddick TNC.
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
 * @file pixdsp_agc_i16.h
 * @brief Peak/valley AGC, int16 mag in, Q15 (x−mid)/span out.
 *
 * Attack ≈ 0.6875 (>>1+>>3+>>4). Decay ≈ 2^−13 via a 16-bit residue
 * so peak still moves when |in−peak| < 8192. One 32/16 divide per
 * sample.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pixdsp_agc_i16 {
    int16_t peak;
    int16_t valley;
    int16_t dec_acc_p;
    int16_t dec_acc_v;
    int initialized;
} pixdsp_agc_i16_t;

/**
 * @brief Zero state and mark initialized.
 * @return 0, or −1 if s is NULL.
 */
int pixdsp_agc_i16_init(pixdsp_agc_i16_t *s);

/** @brief Zero peak/valley and decay residue. */
void pixdsp_agc_i16_reset(pixdsp_agc_i16_t *s);

/**
 * @brief One envelope sample.
 * @param out Q15 ≈ (x−mid)/span. 0 if span ≤ 0.
 * @return 0, or −1.
 */
int pixdsp_agc_i16_process(pixdsp_agc_i16_t *s, int32_t in, int16_t *out);

#ifdef __cplusplus
}
#endif

#endif /* PIXDSP_AGC_I16_H */
