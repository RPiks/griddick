#ifndef PIXDSP_HYPOT_I32_H
#define PIXDSP_HYPOT_I32_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_hypot_i32.h - This file is part of Griddick TNC.
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
 * @file pixdsp_hypot_i32.h
 * @brief Integer hypot, same units in and out.
 *
 * 4-piece Q12 α-max+β-min; Y = ((1<<11) + q12) >> 12.
 * |a|,|b| > 32767 are shifted down and back. int32 only. No libm.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t pixdsp_hypot_i32(int32_t a, int32_t b);

#ifdef __cplusplus
}
#endif

#endif /* PIXDSP_HYPOT_I32_H */
