#ifndef MODEM_MODE_H
#define MODEM_MODE_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * modem_mode.h - This file is part of Griddick TNC.
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
 * @file modem_mode.h
 * @brief Pluggable modem mode vtable (CW first; GFSK/FSK/BPSK/MFSK later).
 *
 * Slice 1: API shape only. CW TX/RX lands in later slices.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct modem_session;
struct modem_tx_request;

typedef struct modem_mode {
    const char *name;
    void *impl;

    void (*destroy)(struct modem_mode *mode);

    /**
     * @brief Prepare TX for request; return 0 or -1.
     * Session then pulls PCM via tx_pull_pcm until done.
     */
    int (*tx_prepare)(struct modem_mode *mode, struct modem_session *s,
                      const struct modem_tx_request *req);

    /**
     * @brief Fill up to max_count PCM levels (0..pwm_wrap). Return count, 0=done, -1=err.
     */
    int (*tx_pull_pcm)(struct modem_mode *mode, uint16_t *out, int max_count);

    /**
     * @brief RX path: de-meaned or raw float PCM @ session sample rate.
     */
    void (*on_pcm)(struct modem_mode *mode, const float *samples, int count);
} modem_mode_t;

#ifdef __cplusplus
}
#endif

#endif /* MODEM_MODE_H */
