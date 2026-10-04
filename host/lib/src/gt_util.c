/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_util.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#define _POSIX_C_SOURCE 200809L

#include "gt_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

uint64_t gt_utc_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000L);
}

void gt_stamp(char *out, size_t out_sz)
{
    struct timespec ts;
    struct tm tm;
    int ms;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    ms = (int)(ts.tv_nsec / 1000000L);
    snprintf(out, out_sz, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
}

int gt_parse_hex_byte(const char *s, uint8_t *out)
{
    char *end;
    unsigned long v;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    v = strtoul(s, &end, 16);
    if (end == s || *end != '\0' || v > 0xffu) {
        return -1;
    }
    *out = (uint8_t)v;
    return 0;
}

static int hex_nibble(int c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int gt_parse_hex_blob(const char *s, uint8_t *out, int max)
{
    int n = 0;
    int hi = -1;
    const char *p;

    for (p = s; *p != '\0'; p++) {
        int v;

        if (*p == ' ' || *p == '\t' || *p == ':' || *p == '-' || *p == ',') {
            continue;
        }
        v = hex_nibble((unsigned char)*p);
        if (v < 0) {
            return -1;
        }
        if (hi < 0) {
            hi = v;
        } else {
            if (n >= max) {
                return -1;
            }
            out[n++] = (uint8_t)((hi << 4) | v);
            hi = -1;
        }
    }
    if (hi >= 0) {
        return -1;
    }
    return n;
}

static int read_all(FILE *fp, uint8_t *out, int max)
{
    size_t n = fread(out, 1, (size_t)max + 1, fp);

    if (ferror(fp)) {
        return -1;
    }
    if (n > (size_t)max) {
        return -2;
    }
    return (int)n;
}

int gt_load_file(const char *path, uint8_t *out, int max)
{
    FILE *fp;
    int n;

    if (strcmp(path, "-") == 0) {
        return read_all(stdin, out, max);
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    n = read_all(fp, out, max);
    fclose(fp);
    return n;
}

int gt_load_text(const char *path, char *out, int max)
{
    FILE *fp;
    size_t n;

    if (strcmp(path, "-") == 0) {
        fp = stdin;
    } else {
        fp = fopen(path, "r");
        if (fp == NULL) {
            return -1;
        }
    }
    n = fread(out, 1, (size_t)max - 1, fp);
    if (fp != stdin) {
        fclose(fp);
    }
    if (n < 1) {
        return -1;
    }
    out[n] = '\0';
    return 0;
}
