#ifndef PIXDSP_DDS_I32_H
#define PIXDSP_DDS_I32_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_dds_i32.h - This file is part of Griddick TNC.
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
 * @file pixdsp_dds_i32.h
 * @brief uint32 phase-accumulator mixer (no libm in process).
 *
 * Real input: re = x·cos, im = ±x·sin. 1024-entry Q31 LUT + linear
 * interpolation. MIX_DOWN uses a negative phase step.
 */

#include "pixdsp_i32.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum pixdsp_dds_mode {
    PIXDSP_DDS_MIX_DOWN = 0,
    PIXDSP_DDS_MIX_UP = 1
} pixdsp_dds_mode_t;

typedef struct pixdsp_dds_params {
    float sample_rate_hz;
    float lo_freq_hz;
    float lo_phase_rad; /**< initial LO phase at n=0 */
    pixdsp_dds_mode_t mode;
} pixdsp_dds_params_t;

typedef struct pixdsp_dds_i32 {
    uint32_t phase;
    uint32_t phase0;
    uint32_t step;
    pixdsp_dds_mode_t mode;
    int initialized;
} pixdsp_dds_i32_t;

/**
 * @brief Set step = ±2^32 · lo_freq / fs (sign from MIX_DOWN).
 * @return 0, or −1.
 */
int pixdsp_dds_i32_init(pixdsp_dds_i32_t *s, const pixdsp_dds_params_t *params);

/** @brief Phase back to lo_phase_rad. */
void pixdsp_dds_i32_reset(pixdsp_dds_i32_t *s);

/**
 * @brief Mix one Q30 sample. re/im are Q30.
 */
void pixdsp_dds_i32_process(pixdsp_dds_i32_t *s, int32_t x, int32_t *re,
                            int32_t *im);

#ifdef __cplusplus
}
#endif

#endif /* PIXDSP_DDS_I32_H */
