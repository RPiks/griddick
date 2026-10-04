#ifndef KISS_ADC_H
#define KISS_ADC_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * kiss_adc.h - This file is part of Griddick TNC.
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

void kiss_adc_init(void);
void kiss_adc_start(void);
void kiss_adc_stop(void);
int kiss_adc_pop(int32_t *q30);

/** 1 after a Q30 ring drop; sticky until reboot. */
int kiss_adc_ring_ovfl(void);

/**
 * @brief Snapshot ISR counters since last take, then zero them.
 * @param n      IRQ count (48 kHz if keeping up).
 * @param us     Sum of ISR wall time (µs).
 * @param max_us Longest single ISR (µs).
 */
void kiss_adc_stat_take(uint32_t *n, uint32_t *us, uint32_t *max_us);

#endif /* KISS_ADC_H */
