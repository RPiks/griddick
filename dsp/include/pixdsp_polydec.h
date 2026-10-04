#ifndef PIXDSP_POLYDEC_H
#define PIXDSP_POLYDEC_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_polydec.h - This file is part of Griddick TNC.
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
 * This file is proprietary to Roman Piksaykin. It is not licensed under
 * the GNU LGPL or any other open-source licence. You may include this
 * header when building Griddick TNC firmware. The corresponding
 * implementation is supplied only as a prebuilt Pico object archive.
 */

/**
 * @file pixdsp_polydec.h
 * @brief ADC-rate decimator. State layout is private.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pixdsp_polydec {
    uint64_t _opaque[32];
} pixdsp_polydec_t;

void pixdsp_polydec_reset(pixdsp_polydec_t *s);

/** Consume one ADC group. Returns Q30, DC blocked. */
int32_t pixdsp_polydec_process_m5(pixdsp_polydec_t *s, const uint16_t *adc5);

#ifdef __cplusplus
}
#endif

#endif /* PIXDSP_POLYDEC_H */
