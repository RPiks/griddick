/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_aprs.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gt_aprs.h"
#include "gt_ax25.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int aprs_split_ll(const char *in, double *lat, double *lon)
{
    char buf[64];
    char *comma;
    char *end;

    if (in == NULL || strlen(in) >= sizeof(buf)) {
        return -1;
    }
    memcpy(buf, in, strlen(in) + 1);
    comma = strchr(buf, ',');
    if (comma == NULL) {
        comma = strchr(buf, ' ');
    }
    if (comma == NULL) {
        return -1;
    }
    *comma = '\0';
    *lat = strtod(buf, &end);
    if (end == buf || *end != '\0') {
        return -1;
    }
    *lon = strtod(comma + 1, &end);
    if (end == comma + 1 || *end != '\0') {
        return -1;
    }
    if (*lat < -90.0 || *lat > 90.0 || *lon < -180.0 || *lon > 180.0) {
        return -1;
    }
    return 0;
}

static void aprs_fmt_ll(double deg, int is_lat, char *out, size_t out_sz)
{
    double a = (deg < 0.0) ? -deg : deg;
    int d = (int)a;
    int mm = (int)((a - (double)d) * 6000.0 + 0.5);
    char hemi;

    if (mm >= 6000) {
        d++;
        mm = 0;
    }
    if (is_lat) {
        hemi = (deg >= 0.0) ? 'N' : 'S';
        snprintf(out, out_sz, "%02d%02d.%02d%c", d, mm / 100, mm % 100, hemi);
    } else {
        hemi = (deg >= 0.0) ? 'E' : 'W';
        snprintf(out, out_sz, "%03d%02d.%02d%c", d, mm / 100, mm % 100, hemi);
    }
}

int gt_aprs_build_pos(uint8_t *out, int max, const char *ll,
                      const char *sym, const char *comment)
{
    double lat, lon;
    char slat[32];
    char slon[32];
    char table = '/';
    char icon = '-';
    int n;

    if (aprs_split_ll(ll, &lat, &lon) != 0) {
        return -1;
    }
    if (sym != NULL && sym[0] != '\0') {
        if ((sym[0] != '/' && sym[0] != '\\') || sym[1] == '\0' || sym[2] != '\0') {
            return -2;
        }
        table = sym[0];
        icon = sym[1];
    }
    aprs_fmt_ll(lat, 1, slat, sizeof(slat));
    aprs_fmt_ll(lon, 0, slon, sizeof(slon));
    n = snprintf((char *)out, (size_t)max, "!%s%c%s%c%s",
                 slat, table, slon, icon, comment ? comment : "");
    if (n < 0 || n >= max) {
        return -3;
    }
    return n;
}

int gt_aprs_build_status(uint8_t *out, int max, const char *text)
{
    int n;

    if (text == NULL) {
        text = "";
    }
    n = snprintf((char *)out, (size_t)max, ">%s", text);
    if (n < 0 || n >= max) {
        return -1;
    }
    return n;
}

int gt_aprs_build_msg(uint8_t *out, int max, const char *to, const char *text)
{
    struct gt_ax25_addr a;
    char call[7];
    char dest[10];
    int i;
    int n;

    if (gt_ax25_parse_call(to, &a) != 0) {
        return -1;
    }
    memcpy(call, a.call, 6);
    call[6] = '\0';
    for (i = 5; i >= 0 && call[i] == ' '; i--) {
        call[i] = '\0';
    }
    memset(dest, ' ', 9);
    dest[9] = '\0';
    n = (int)strlen(call);
    if (a.ssid != 0) {
        int add = snprintf(dest, 10, "%s-%d", call, a.ssid);

        if (add < 0 || add > 9) {
            return -1;
        }
        for (; add < 9; add++) {
            dest[add] = ' ';
        }
        dest[9] = '\0';
    } else {
        memcpy(dest, call, (size_t)n);
    }
    if (text == NULL) {
        text = "";
    }
    n = snprintf((char *)out, (size_t)max, ":%s:%s", dest, text);
    if (n < 0 || n >= max) {
        return -1;
    }
    return n;
}

static int aprs_parse_coord(const char *s, int is_lat, double *deg)
{
    int nd = is_lat ? 2 : 3;
    int d = 0;
    int i;
    char *end;
    double mm;

    if (s == NULL) {
        return -1;
    }
    for (i = 0; i < nd; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return -1;
        }
        d = d * 10 + (s[i] - '0');
    }
    mm = strtod(s + nd, &end);
    if (end == s + nd) {
        return -1;
    }
    *deg = (double)d + mm / 60.0;
    if (*end == 'S' || *end == 'W') {
        *deg = -*deg;
    } else if (*end != 'N' && *end != 'E') {
        return -1;
    }
    return (int)(end - s) + 1;
}

int gt_aprs_format_rx(const uint8_t *info, int n, char *out, int max)
{
    char kind;
    const char *p;
    const char *end;
    double lat;
    double lon;
    int used;

    if (info == NULL || out == NULL || n <= 0 || max < 2) {
        return 0;
    }
    kind = (char)info[0];
    p = (const char *)info + 1;
    end = (const char *)info + n;
    if (kind == '>') {
        return snprintf(out, (size_t)max, "status %.*s", n - 1, p);
    }
    if (kind == ':') {
        return snprintf(out, (size_t)max, "msg %.*s", n - 1, p);
    }
    if (kind != '!' && kind != '=' && kind != '/' && kind != '@') {
        return 0;
    }
    if ((kind == '/' || kind == '@') && (end - p) >= 7) {
        p += 7;
    }
    used = aprs_parse_coord(p, 1, &lat);
    if (used < 8 || p + used >= end) {
        return snprintf(out, (size_t)max, "pos %.*s", n - 1,
                        (const char *)info + 1);
    }
    p += used;
    if (p >= end) {
        return 0;
    }
    p++;
    used = aprs_parse_coord(p, 0, &lon);
    if (used < 9) {
        return snprintf(out, (size_t)max, "pos %.*s", n - 1,
                        (const char *)info + 1);
    }
    p += used;
    if (p < end) {
        p++;
    }
    if (p < end) {
        return snprintf(out, (size_t)max, "pos %.5f,%.5f %.*s",
                        lat, lon, (int)(end - p), p);
    }
    return snprintf(out, (size_t)max, "pos %.5f,%.5f", lat, lon);
}
