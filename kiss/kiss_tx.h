#ifndef KISS_TX_H
#define KISS_TX_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * kiss_tx.h - This file is part of Griddick TNC.
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

#include <stdint.h>

#define KISS_TX_FS_HZ     12000
#define KISS_TX_BAUD      1200
#define KISS_TX_SPS       10
#define KISS_TX_WRAP      1499
#define KISS_TX_MID       750
#define KISS_TX_BODY_MAX  512
#define KISS_TX_PAYLOAD_MAX (KISS_TX_BODY_MAX - 2)
#define KISS_TX_LEAD_MAX  128
#define KISS_TX_TRAIL_DEFAULT 2

typedef struct kiss_tx {
    uint8_t body[KISS_TX_BODY_MAX];
    int body_len;
    int lead_flags;
    int trail_flags;
    int stage;
    int flags_left;
    int byte_i;
    int bit_i;
    int ones;
    int stuff_pending;
    int nrzi_tone;
    int sample_in_sym;
    uint32_t phase;
    uint32_t step;
    int active;
} kiss_tx_t;

void kiss_tx_init(kiss_tx_t *s);

/** @brief ceil(n · 10 ms · 1200 / 8), clamped 1..128. */
int kiss_tx_flag_count_10ms(uint8_t n_10ms);

uint16_t kiss_tx_fcs(const uint8_t *data, int len);

/**
 * @brief Arm a burst from a KISS DATA payload (Address..Info, no FCS).
 * @return 0, or −1.
 */
int kiss_tx_start(kiss_tx_t *s, const uint8_t *payload, int n,
                  int lead_flags, int trail_flags);

int kiss_tx_active(const kiss_tx_t *s);

/**
 * @brief Next 12 kHz PWM level in 0..KISS_TX_WRAP.
 * @return 1 if *level is valid, 0 when idle/done.
 */
int kiss_tx_next(kiss_tx_t *s, uint16_t *level);

#endif /* KISS_TX_H */
