#ifndef PIXDSP_I32_H
#define PIXDSP_I32_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_i32.h - This file is part of Griddick TNC.
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
 * @file pixdsp_i32.h
 * @brief Q30 samples / Q31 coefficients for int32 hot paths.
 *
 * PCM ±1.0 maps to ±PIXDSP_I32_ONE (Q30) so a full-scale sample fits
 * int32. FIR coefficients are Q31 (round(h·2^31), saturating). MAC uses
 * int64; a unity-gain FIR is (acc >> 31) back to Q30.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PIXDSP_I32_Q    30
#define PIXDSP_I32_ONE  (1 << PIXDSP_I32_Q)

/**
 * @brief Saturating float → Q30.
 * @param x Real value; ±1.0 → ±PIXDSP_I32_ONE.
 */
int32_t pixdsp_f32_to_q30(float x);

/**
 * @brief Q30 → float (x / 2^30).
 */
float pixdsp_q30_to_f32(int32_t x);

/**
 * @brief Saturating float → Q31 (FIR taps).
 * @param h Coefficient, typically |h| << 1.
 */
int32_t pixdsp_f32_to_q31(float h);

#ifdef __cplusplus
}
#endif

#endif /* PIXDSP_I32_H */
