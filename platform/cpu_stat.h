#ifndef CPU_STAT_H
#define CPU_STAT_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * cpu_stat.h - This file is part of Griddick TNC.
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

#ifdef __cplusplus
extern "C" {
#endif

void cpu_stat_init(void);
void cpu_stat_mark_loop(void);
void cpu_stat_busy_begin(void);
void cpu_stat_busy_end(void);

/**
 * @brief If ≥1 s elapsed, write period length and loop count, then reset.
 * @return 1 if *elapsed_us and *loops are valid, else 0.
 */
int cpu_stat_take(uint32_t *elapsed_us, uint32_t *loops);

#ifdef __cplusplus
}
#endif

#endif /* CPU_STAT_H */
