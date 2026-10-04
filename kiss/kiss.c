/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * kiss.c - This file is part of Griddick TNC.
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

#include "kiss.h"

#ifdef GRIDDICK_FW_STAMP
#include "griddick_build.h"
#endif
#ifndef GRIDDICK_FW_VERSION
#define GRIDDICK_FW_VERSION "0.9.7"
#endif
#ifndef GRIDDICK_BUILD_UTC
#define GRIDDICK_BUILD_UTC ""
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void kiss_rx_init(kiss_rx_t *s)
{
    s->len = 0;
    s->frame_len = 0;
    s->escaped = 0;
    s->in_frame = 0;
}

int kiss_rx_byte(kiss_rx_t *s, uint8_t b)
{
    if (b == KISS_FEND) {
        int ready;

        ready = (s->in_frame && s->len > 0 && !s->escaped) ? 1 : 0;
        if (ready) {
            s->frame_len = s->len;
        }
        s->escaped = 0;
        s->in_frame = 1;
        s->len = 0;
        return ready;
    }

    if (!s->in_frame) {
        return 0;
    }

    if (s->escaped) {
        s->escaped = 0;
        if (b == KISS_TFEND) {
            b = KISS_FEND;
        } else if (b == KISS_TFESC) {
            b = KISS_FESC;
        } else {
            s->in_frame = 0;
            s->len = 0;
            return 0;
        }
    } else if (b == KISS_FESC) {
        s->escaped = 1;
        return 0;
    }

    if (s->len >= KISS_MAX_FRAME) {
        s->in_frame = 0;
        s->len = 0;
        s->escaped = 0;
        return 0;
    }
    s->buf[s->len++] = b;
    return 0;
}

static int put_esc(uint8_t *out, int max, int n, uint8_t b)
{
    if (b == KISS_FEND) {
        if (n + 2 > max) {
            return -1;
        }
        out[n++] = KISS_FESC;
        out[n++] = KISS_TFEND;
        return n;
    }
    if (b == KISS_FESC) {
        if (n + 2 > max) {
            return -1;
        }
        out[n++] = KISS_FESC;
        out[n++] = KISS_TFESC;
        return n;
    }
    if (n + 1 > max) {
        return -1;
    }
    out[n++] = b;
    return n;
}

int kiss_encode(uint8_t cmd, const uint8_t *data, int n_data,
                uint8_t *out, int max)
{
    int n;
    int i;

    if (out == NULL || max < 2 || n_data < 0) {
        return -1;
    }
    if (n_data > 0 && data == NULL) {
        return -1;
    }

    n = 0;
    out[n++] = KISS_FEND;
    n = put_esc(out, max, n, cmd);
    if (n < 0) {
        return -1;
    }
    for (i = 0; i < n_data; i++) {
        n = put_esc(out, max, n, data[i]);
        if (n < 0) {
            return -1;
        }
    }
    if (n + 1 > max) {
        return -1;
    }
    out[n++] = KISS_FEND;
    return n;
}

uint16_t kiss_crc16(const uint8_t *data, int n)
{
    uint16_t crc = 0xFFFFu;
    int i;
    int bit;

    if (data == NULL || n <= 0) {
        return crc;
    }
    for (i = 0; i < n; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0; bit < 8; bit++) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

int kiss_json_span(const char *s, int n, int *off)
{
    int i;
    int a = -1;
    int b = -1;

    if (s == NULL || n <= 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (s[i] == '{') {
            a = i;
            break;
        }
    }
    for (i = n - 1; i >= 0; i--) {
        if (s[i] == '}') {
            b = i;
            break;
        }
    }
    if (a < 0 || b < 0 || b < a) {
        return -1;
    }
    if (off != NULL) {
        *off = a;
    }
    return b - a + 1;
}

int kiss_cfg_wrap(char *s, int json_n, int max)
{
    uint16_t crc;
    int n;

    if (s == NULL || json_n < 2 || max < json_n + 6) {
        return -1;
    }
    crc = kiss_crc16((const uint8_t *)s, json_n);
    n = snprintf(s + json_n, (size_t)(max - json_n), ",$%04X", (unsigned)crc);
    if (n != 6) {
        return -1;
    }
    return json_n + n;
}

static int hex4(const char *p)
{
    int i;
    int v = 0;

    for (i = 0; i < 4; i++) {
        int c = (unsigned char)p[i];
        int d;

        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else {
            return -1;
        }
        v = (v << 4) | d;
    }
    return v;
}

int kiss_cfg_unwrap(const char *s, int n, int *json_off, int *json_n)
{
    int off = 0;
    int jn;
    int got;

    jn = kiss_json_span(s, n, &off);
    if (jn < 2) {
        return -1;
    }
    if (off + jn + 6 > n) {
        return 0;
    }
    if (s[off + jn] != ',' || s[off + jn + 1] != '$') {
        return 0;
    }
    got = hex4(s + off + jn + 2);
    if (got < 0) {
        return 0;
    }
    if ((uint16_t)got != kiss_crc16((const uint8_t *)(s + off), jn)) {
        return 0;
    }
    if (json_off != NULL) {
        *json_off = off;
    }
    if (json_n != NULL) {
        *json_n = jn;
    }
    return 1;
}

static int json_u8(const char *js, const char *key, int *out)
{
    const char *p;
    char *end;
    long v;
    size_t klen = strlen(key);

    p = js;
    while ((p = strstr(p, key)) != NULL) {
        const char *q = p + klen;

        if (p > js && p[-1] != '"' && p[-1] != '{' && p[-1] != ' ' &&
            p[-1] != '\t' && p[-1] != '\n') {
            p = q;
            continue;
        }
        if (*q == '"') {
            q++;
        }
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') {
            q++;
        }
        if (*q != ':') {
            p = q;
            continue;
        }
        q++;
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') {
            q++;
        }
        if (*q == '/' && q[1] == '/') {
            p = q;
            continue;
        }
        v = strtol(q, &end, 10);
        if (end == q || v < 0 || v > 255) {
            return -1;
        }
        *out = (int)v;
        return 1;
    }
    return 0;
}

static int json_bool(const char *js, const char *key, int *out)
{
    const char *p;
    size_t klen = strlen(key);

    p = js;
    while ((p = strstr(p, key)) != NULL) {
        const char *q = p + klen;

        if (p > js && p[-1] != '"' && p[-1] != '{' && p[-1] != ' ' &&
            p[-1] != '\t' && p[-1] != '\n') {
            p = q;
            continue;
        }
        if (*q == '"') {
            q++;
        }
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') {
            q++;
        }
        if (*q != ':') {
            p = q;
            continue;
        }
        q++;
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') {
            q++;
        }
        if (strncmp(q, "true", 4) == 0) {
            *out = 1;
            return 1;
        }
        if (strncmp(q, "false", 5) == 0) {
            *out = 0;
            return 1;
        }
        if (*q == '1') {
            *out = 1;
            return 1;
        }
        if (*q == '0') {
            *out = 0;
            return 1;
        }
        return -1;
    }
    return 0;
}

static const char *json_after_key(const char *js, const char *key)
{
    const char *p;
    size_t klen = strlen(key);

    p = js;
    while ((p = strstr(p, key)) != NULL) {
        const char *q = p + klen;

        if (p > js && p[-1] != '"' && p[-1] != '{' && p[-1] != ' ' &&
            p[-1] != '\t' && p[-1] != '\n') {
            p = q;
            continue;
        }
        if (*q == '"') {
            q++;
        }
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') {
            q++;
        }
        if (*q != ':') {
            p = q;
            continue;
        }
        q++;
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') {
            q++;
        }
        return q;
    }
    return NULL;
}

/* 3.25 → 325 hundredths. One or two fraction digits. */
static int json_centi(const char *js, const char *key, int *out, int lo, int hi)
{
    const char *q = json_after_key(js, key);
    char *end;
    long ip;
    int frac = 0;
    int nd = 0;

    if (q == NULL) {
        return 0;
    }
    ip = strtol(q, &end, 10);
    if (end == q) {
        return -1;
    }
    if (*end == '.') {
        const char *f = end + 1;

        while (f[nd] >= '0' && f[nd] <= '9' && nd < 2) {
            frac = frac * 10 + (f[nd] - '0');
            nd++;
        }
        if (nd == 1) {
            frac *= 10;
        }
        if (nd == 0) {
            return -1;
        }
        end = (char *)(f + nd);
    }
    ip = ip * 100 + (ip < 0 ? -frac : frac);
    if (ip < lo || ip > hi) {
        return -1;
    }
    *out = (int)ip;
    return 1;
}

static int json_int(const char *js, const char *key, int *out, int lo, int hi)
{
    const char *q = json_after_key(js, key);
    char *end;
    long v;

    if (q == NULL) {
        return 0;
    }
    v = strtol(q, &end, 10);
    if (end == q || v < lo || v > hi) {
        return -1;
    }
    *out = (int)v;
    return 1;
}

static int json_str(const char *js, const char *key, char *out, int max)
{
    const char *q = json_after_key(js, key);
    int i = 0;

    if (q == NULL) {
        return 0;
    }
    if (*q != '"') {
        return -1;
    }
    q++;
    while (*q != '\0' && *q != '"' && i < max - 1) {
        if (*q == '\\') {
            return -1;
        }
        out[i++] = *q++;
    }
    if (*q != '"') {
        return -1;
    }
    out[i] = '\0';
    return 1;
}

int kiss_cfg_from_json(const char *js, int n, kiss_cfg_t *c)
{
    char tmp[KISS_MAX_FRAME];
    int r;

    if (js == NULL || c == NULL || n < 2) {
        return -1;
    }
    if (n >= (int)sizeof(tmp)) {
        n = (int)sizeof(tmp) - 1;
    }
    memcpy(tmp, js, (size_t)n);
    tmp[n] = '\0';
    memset(c, 0, sizeof(*c));
    r = json_u8(tmp, "txDelay", &c->txdelay);
    if (r < 0) {
        return -1;
    }
    c->have_txdelay = r;
    r = json_u8(tmp, "persist", &c->persist);
    if (r < 0) {
        return -1;
    }
    c->have_persist = r;
    r = json_u8(tmp, "slotTime", &c->slottime);
    if (r < 0) {
        return -1;
    }
    c->have_slottime = r;
    r = json_u8(tmp, "txTail", &c->txtail);
    if (r < 0) {
        return -1;
    }
    c->have_txtail = r;
    r = json_u8(tmp, "fullDup", &c->fulldup);
    if (r < 0) {
        return -1;
    }
    c->have_fulldup = r;
    if (c->have_fulldup && c->fulldup != 0 && c->fulldup != 1) {
        return -1;
    }
    r = json_bool(tmp, "legacyMode", &c->legacy);
    if (r < 0) {
        return -1;
    }
    c->have_legacy = r;
    r = json_bool(tmp, "logEvents", &c->log_events);
    if (r < 0) {
        return -1;
    }
    c->have_log_events = r;
    {
        char mode[8];

        r = json_str(tmp, "linkMode", mode, (int)sizeof(mode));
        if (r < 0) {
            return -1;
        }
        if (r) {
            if (strcmp(mode, "UI") == 0) {
                c->linkmode = 0;
            } else if (strcmp(mode, "I") == 0) {
                c->linkmode = 1;
            } else {
                return -1;
            }
            c->have_linkmode = 1;
        }
    }
    r = json_str(tmp, "myCall", c->mycall, (int)sizeof(c->mycall));
    if (r < 0) {
        return -1;
    }
    c->have_mycall = r;
    r = json_int(tmp, "paclen", &c->paclen, 1, 256);
    if (r < 0) {
        return -1;
    }
    c->have_paclen = r;
    r = json_int(tmp, "maxframe", &c->maxframe, 1, 7);
    if (r < 0) {
        return -1;
    }
    c->have_maxframe = r;
    r = json_int(tmp, "t1_ms", &c->t1_ms, 1, 60000);
    if (r < 0) {
        return -1;
    }
    c->have_t1 = r;
    r = json_int(tmp, "t2_ms", &c->t2_ms, 0, 60000);
    if (r < 0) {
        return -1;
    }
    c->have_t2 = r;
    r = json_int(tmp, "t3_ms", &c->t3_ms, 0, 600000);
    if (r < 0) {
        return -1;
    }
    c->have_t3 = r;
    r = json_int(tmp, "n2", &c->n2, 1, 16);
    if (r < 0) {
        return -1;
    }
    c->have_n2 = r;
    r = json_int(tmp, "bitFixCnt", &c->bitfix_cnt, 0, 1);
    if (r < 0) {
        return -1;
    }
    c->have_bitfix_cnt = r;
    r = json_int(tmp, "bitFixAlgo", &c->bitfix_algo, 0, 0);
    if (r < 0) {
        return -1;
    }
    c->have_bitfix_algo = r;
    {
        char san[12];

        r = json_str(tmp, "bitFixSanity", san, (int)sizeof(san));
        if (r < 0) {
            return -1;
        }
        if (r) {
            if (strcmp(san, "none") == 0) {
                c->bitfix_sanity = 0;
            } else if (strcmp(san, "ASCII") == 0) {
                c->bitfix_sanity = 1;
            } else {
                return -1;
            }
            c->have_bitfix_sanity = 1;
        }
    }
    r = json_bool(tmp, "txAwgnInject", &c->tx_awgn_on);
    if (r < 0) {
        return -1;
    }
    c->have_tx_awgn_on = r;
    r = json_centi(tmp, "txAwgnSNRdB", &c->tx_awgn_cb, 200, 1200);
    if (r < 0) {
        return -1;
    }
    c->have_tx_awgn_cb = r;
    r = json_int(tmp, "txAwgnSeed", &c->tx_awgn_seed, 1, 2147483647);
    if (r < 0) {
        return -1;
    }
    c->have_tx_awgn_seed = r;
    return 0;
}

int kiss_cfg_to_json(const kiss_cfg_t *c, const char *version,
                     char *out, int max)
{
    int n;

    if (c == NULL || out == NULL || max < 8) {
        return -1;
    }
    if (version == NULL || version[0] == '\0') {
        version = GRIDDICK_FW_VERSION;
    }
    {
        const char *call = c->mycall[0] != '\0' ? c->mycall : "TEST-0";
        int paclen = c->paclen > 0 ? c->paclen : 256;
        int maxf = c->maxframe > 0 ? c->maxframe : 4;
        int t1 = c->t1_ms > 0 ? c->t1_ms : 2000;
        int t2 = c->t2_ms;
        int t3 = c->t3_ms > 0 ? c->t3_ms : 30000;
        int n2 = c->n2 > 0 ? c->n2 : 10;
        int bcnt = (c->bitfix_cnt == 1) ? 1 : 0;
        const char *san = c->bitfix_sanity ? "ASCII" : "none";
        int acb = c->tx_awgn_cb > 0 ? c->tx_awgn_cb : 325;
        int aseed = c->tx_awgn_seed > 0 ? c->tx_awgn_seed : 1;

        n = snprintf(out, (size_t)max,
                     "{\"build\":{\"version\":\"%s\",\"hash\":\"\","
                     "\"buildUTC\":\"%s\"},\"kiss\":{\"legacyMode\":%s,"
                     "\"logEvents\":%s,"
                     "\"txDelay\":%d,\"persist\":%d,\"slotTime\":%d,"
                     "\"txTail\":%d,\"fullDup\":%d},\"link\":{\"linkMode\":\"%s\","
                     "\"myCall\":\"%s\",\"paclen\":%d,\"maxframe\":%d,"
                     "\"t1_ms\":%d,\"t2_ms\":%d,\"t3_ms\":%d,\"n2\":%d},"
                     "\"modem\":{\"bitFixCnt\":%d,\"bitFixAlgo\":0,"
                     "\"bitFixSanity\":\"%s\"},"
                     "\"debug\":{\"txAwgnInject\":%s,\"txAwgnSNRdB\":%d.%02d,"
                     "\"txAwgnSeed\":%d}}",
                     version, GRIDDICK_BUILD_UTC,
                     c->legacy ? "true" : "false",
                     c->log_events ? "true" : "false", c->txdelay,
                     c->persist, c->slottime, c->txtail, c->fulldup,
                     c->linkmode ? "I" : "UI", call, paclen, maxf, t1, t2, t3,
                     n2, bcnt, san,
                     c->tx_awgn_on ? "true" : "false",
                     acb / 100, acb % 100, aseed);
    }
    if (n < 0 || n >= max) {
        return -1;
    }
    return n;
}
