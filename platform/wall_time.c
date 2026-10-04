/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * wall_time.c - This file is part of Griddick TNC.
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

#include "wall_time.h"

#include "pico/time.h"

#include <stdio.h>

static uint64_t s_origin_utc_ms;
static uint32_t s_origin_boot_ms;

void wall_time_init(void)
{
    s_origin_utc_ms = 0;
    s_origin_boot_ms = to_ms_since_boot(get_absolute_time());
}

void wall_time_set_utc_ms(uint64_t utc_ms)
{
    s_origin_utc_ms = utc_ms;
    s_origin_boot_ms = to_ms_since_boot(get_absolute_time());
}

uint64_t wall_time_utc_ms(void)
{
    uint32_t now = to_ms_since_boot(get_absolute_time());

    return s_origin_utc_ms + (uint32_t)(now - s_origin_boot_ms);
}

/* Days since 1970-01-01 → civil UTC date (Howard Hinnant). */
static void civil_from_days(int z, int *year, int *month, int *day)
{
    int era;
    unsigned doe;
    unsigned yoe;
    unsigned doy;
    unsigned mp;
    int y;

    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned)(z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = (int)yoe + era * 400;
    doy = doe - (365u * yoe + yoe / 4 - yoe / 100);
    mp = (5u * doy + 2u) / 153u;
    *day = (int)(doy - (153u * mp + 2u) / 5u) + 1;
    *month = (int)(mp < 10u ? mp + 3u : mp - 9u);
    if (*month <= 2) {
        y++;
    }
    *year = y;
}

int wall_time_format(char *out, int max)
{
    uint64_t ms;
    uint64_t sec;
    unsigned msec;
    int days;
    unsigned sod;
    int year;
    int month;
    int day;
    unsigned hh;
    unsigned mm;
    unsigned ss;
    int n;

    if (out == NULL || max < WALL_TIME_STAMP_LEN + 1) {
        return -1;
    }
    ms = wall_time_utc_ms();
    sec = ms / 1000u;
    msec = (unsigned)(ms % 1000u);
    days = (int)(sec / 86400u);
    sod = (unsigned)(sec % 86400u);
    civil_from_days(days, &year, &month, &day);
    hh = sod / 3600u;
    mm = (sod % 3600u) / 60u;
    ss = sod % 60u;
    n = snprintf(out, (size_t)max, "%04d-%02d-%02d %02u:%02u:%02u.%03u",
                 year, month, day, hh, mm, ss, msec);
    if (n != WALL_TIME_STAMP_LEN) {
        return -1;
    }
    return n;
}
