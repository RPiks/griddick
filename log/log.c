/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * log.c - This file is part of Griddick TNC.
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

#include "log.h"

#include "wall_time.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char buf[LOG_LINE_MAX];
    int len;
} log_slot_t;

static log_slot_t s_q[LOG_RING_N];
static int s_rd;
static int s_wr;
static int s_n;
static int s_events;

void log_init(void)
{
    s_rd = 0;
    s_wr = 0;
    s_n = 0;
    s_events = 1;
}

void log_set_events(int on)
{
    s_events = on ? 1 : 0;
}

void log_printf(const char *tag, const char *fmt, ...)
{
    char line[LOG_LINE_MAX];
    int n;
    int m;
    va_list ap;

    if (tag == NULL) {
        tag = "?";
    }
    if (fmt == NULL) {
        fmt = "";
    }
    /* Runtime + Cfg + Time always (TIME reply is clock sync). */
    if (!s_events && strcmp(tag, "Runtime") != 0 &&
        strcmp(tag, "Cfg") != 0 && strcmp(tag, "Time") != 0) {
        return;
    }

    n = wall_time_format(line, (int)sizeof(line));
    if (n < 0) {
        n = 0;
        line[0] = '\0';
    }
    m = snprintf(line + n, sizeof(line) - (size_t)n, " [%s] ", tag);
    if (m < 0) {
        return;
    }
    n += m;
    if (n >= (int)sizeof(line)) {
        n = (int)sizeof(line) - 1;
    } else {
        va_start(ap, fmt);
        m = vsnprintf(line + n, sizeof(line) - (size_t)n, fmt, ap);
        va_end(ap);
        if (m < 0) {
            return;
        }
        n += m;
        if (n >= (int)sizeof(line)) {
            n = (int)sizeof(line) - 1;
        }
    }
    line[n] = '\0';

    memcpy(s_q[s_wr].buf, line, (size_t)n);
    s_q[s_wr].len = n;
    s_wr = (s_wr + 1) % LOG_RING_N;
    if (s_n < LOG_RING_N) {
        s_n++;
    } else {
        s_rd = (s_rd + 1) % LOG_RING_N;
    }
}

int log_pop(char *out, int max)
{
    int n;

    if (out == NULL || max < 1 || s_n <= 0) {
        return s_n <= 0 ? 0 : -1;
    }
    n = s_q[s_rd].len;
    if (n > max) {
        n = max;
    }
    memcpy(out, s_q[s_rd].buf, (size_t)n);
    s_rd = (s_rd + 1) % LOG_RING_N;
    s_n--;
    return n;
}
