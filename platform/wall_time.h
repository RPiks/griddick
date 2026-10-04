#ifndef WALL_TIME_H
#define WALL_TIME_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * wall_time.h - This file is part of Griddick TNC.
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

#define WALL_TIME_STAMP_LEN 23 /* YYYY-MM-DD HH:MM:SS.mmm */

void wall_time_init(void);

/** @brief Set current UTC to @p utc_ms (unix epoch milliseconds). */
void wall_time_set_utc_ms(uint64_t utc_ms);

/** @brief Unix epoch milliseconds. 0 at boot until set. */
uint64_t wall_time_utc_ms(void);

/**
 * @brief Write "YYYY-MM-DD HH:MM:SS.mmm" (UTC) into @p out.
 * @return Bytes written (WALL_TIME_STAMP_LEN), or −1.
 */
int wall_time_format(char *out, int max);

#ifdef __cplusplus
}
#endif

#endif /* WALL_TIME_H */
