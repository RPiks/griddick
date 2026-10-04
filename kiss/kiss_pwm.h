#ifndef KISS_PWM_H
#define KISS_PWM_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * kiss_pwm.h - This file is part of Griddick TNC.
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

#include "kiss_tx.h"

void kiss_pwm_init(void);

/** @brief PTT high, play @p tx until kiss_tx_next returns 0. */
void kiss_pwm_start(kiss_tx_t *tx);

/** @brief If the burst finished, drop PTT and hold mid-scale. */
void kiss_pwm_service(void);

int kiss_pwm_busy(void);

/* RAM-only. snr_cb is hundredths of dB (325 = 3.25). */
void kiss_pwm_set_awgn(int on, int snr_cb, uint32_t seed);

#endif /* KISS_PWM_H */
