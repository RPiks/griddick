#ifndef LOG_H
#define LOG_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * log.h - This file is part of Griddick TNC.
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

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_LINE_MAX 512
#define LOG_RING_N   16

void log_init(void);

/** @brief If 0, only Runtime, Cfg, and Time enqueue. Boot default is 1. */
void log_set_events(int on);

/**
 * @brief Queue one ASCII line: "<stamp> [<tag>] <fmt…>".
 * Overwrites the oldest line if the ring is full. Main thread only.
 */
void log_printf(const char *tag, const char *fmt, ...);

/**
 * @brief Pop one queued line (no newline, no NUL counted in return).
 * @return Bytes written, or 0 if empty, or −1.
 */
int log_pop(char *out, int max);

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
