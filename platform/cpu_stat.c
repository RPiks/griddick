/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * cpu_stat.c - This file is part of Griddick TNC.
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

#include "cpu_stat.h"

#include "pico/time.h"

#define PERIOD_US 1000000u

static uint64_t s_period_start;
static uint64_t s_busy_start;
static uint32_t s_busy_us;
static uint32_t s_loops;
static int s_in_busy;

void cpu_stat_init(void)
{
    s_period_start = time_us_64();
    s_busy_start = 0;
    s_busy_us = 0;
    s_loops = 0;
    s_in_busy = 0;
}

void cpu_stat_mark_loop(void)
{
    s_loops++;
}

void cpu_stat_busy_begin(void)
{
    if (s_in_busy) {
        return;
    }
    s_in_busy = 1;
    s_busy_start = time_us_64();
}

void cpu_stat_busy_end(void)
{
    uint64_t now;

    if (!s_in_busy) {
        return;
    }
    now = time_us_64();
    s_busy_us += (uint32_t)(now - s_busy_start);
    s_in_busy = 0;
}

int cpu_stat_take(uint32_t *elapsed_us, uint32_t *loops)
{
    uint64_t now;
    uint32_t elapsed;

    if (elapsed_us == NULL || loops == NULL) {
        return 0;
    }
    now = time_us_64();
    elapsed = (uint32_t)(now - s_period_start);
    if (elapsed < PERIOD_US) {
        return 0;
    }
    *elapsed_us = elapsed;
    *loops = s_loops;
    s_period_start = now;
    s_busy_us = 0;
    s_loops = 0;
    return 1;
}
