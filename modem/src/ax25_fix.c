/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * ax25_fix.c - This file is part of Griddick TNC.
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

/**
 * @file ax25_fix.c
 * @brief Direwolf F=1: invert one pre-NRZI tone, re-destuff, re-check FCS.
 *
 * raw[0] is the last opening-flag tone (NRZI prev). raw[1..] are body tones.
 * Closing-flag tones must already be chopped. bitFixAlgo 0 only.
 */
#include "ax25_internal.h"

#include <string.h>

#ifndef __not_in_flash_func
#define __not_in_flash_func(fn) fn
#endif

static int raw_tone(const uint8_t *raw, int i, int invert_i, int invert_j,
                    int invert_k)
{
    int t = raw[i] & 1;

    if (i == invert_i || i == invert_j || i == invert_k) {
        t ^= 1;
    }
    return t;
}

int __not_in_flash_func(ax25_fix_decode_raw)(const uint8_t *raw, int nraw, int invert_i, int invert_j,
                        int invert_k, uint8_t *out, size_t max_out, size_t *len)
{
    int prev;
    int i;
    int ones = 0;
    int olen = 0;
    uint8_t oacc = 0;
    size_t n = 0;

    if (raw == NULL || out == NULL || len == NULL || nraw < 2) {
        return 0;
    }
    *len = 0;
    prev = raw_tone(raw, 0, invert_i, invert_j, invert_k);
    for (i = 1; i < nraw; i++) {
        int tone = raw_tone(raw, i, invert_i, invert_j, invert_k);
        int dbit = (tone == prev) ? 1 : 0;

        prev = tone;
        if (ones == 5) {
            if (dbit == 0) {
                ones = 0;
                continue;
            }
            return 0;
        }
        if (dbit) {
            ones++;
            if (ones >= 7) {
                return 0;
            }
        } else {
            ones = 0;
        }
        oacc = (uint8_t)(oacc >> 1);
        if (dbit) {
            oacc = (uint8_t)(oacc | 0x80u);
        }
        olen++;
        if (olen >= 8) {
            if (n >= max_out || n >= AX25_HDLC_MAX_FRAME) {
                return 0;
            }
            out[n++] = oacc;
            olen = 0;
            oacc = 0;
        }
    }
    if (olen != 0 || n < (size_t)AX25_FIX_MIN_FRAME) {
        return 0;
    }
    *len = n;
    return 1;
}

static int info_off(const uint8_t *buf, size_t body)
{
    size_t off = 0;
    int ext = 0;

    if (buf == NULL || body < 14u) {
        return -1;
    }
    while (off + 7u <= body) {
        ext = buf[off + 6u] & 1;
        off += 7u;
        if (ext) {
            break;
        }
    }
    if (!ext || off < 14u) {
        return -1;
    }
    if (off >= body) {
        return (int)off;
    }
    {
        uint8_t ctrl = buf[off++];

        if ((ctrl & 0x01u) == 0u) {
            /* I frame: no PID */
        } else if ((ctrl & 0x03u) == 0x01u) {
            /* S frame */
        } else if ((ctrl & 0xEFu) == 0x03u && off < body) {
            off++;
        }
    }
    return (int)off;
}

int ax25_fix_sanity_ascii(const uint8_t *buf, size_t len)
{
    size_t body;
    int off;
    size_t i;

    if (buf == NULL || len < (size_t)AX25_FIX_MIN_FRAME) {
        return 0;
    }
    body = len - 2u;
    off = info_off(buf, body);
    if (off < 0) {
        return 0;
    }
    for (i = (size_t)off; i < body; i++) {
        if (buf[i] < 0x20u || buf[i] > 0x7Eu) {
            return 0;
        }
    }
    return 1;
}

int __not_in_flash_func(ax25_fix_invert_single)(const uint8_t *raw, int nraw,
                                               uint8_t *out, size_t max_out,
                                               size_t *len, int sanity,
                                               uint32_t budget_us,
                                               uint32_t (*now_us)(void),
                                               int *io_pos)
{
    static uint8_t tmp[AX25_HDLC_MAX_FRAME];
    size_t n = 0;
    int i = 0;
    uint32_t t0 = 0;

    if (len != NULL) {
        *len = 0;
    }
    if (raw == NULL || out == NULL || len == NULL) {
        return 0;
    }
    if (nraw < 2 || nraw > AX25_FIX_MAX_RAW) {
        return 0;
    }
    if (io_pos != NULL) {
        i = *io_pos;
        if (i < 0) {
            i = 0;
        }
    }
    if (now_us != NULL && budget_us > 0u) {
        t0 = now_us();
    }
    for (; i < nraw; i++) {
        uint16_t calc;
        uint16_t rx;

        if (now_us != NULL && budget_us > 0u) {
            if ((uint32_t)(now_us() - t0) >= budget_us) {
                if (io_pos != NULL) {
                    *io_pos = i;
                }
                return 0;
            }
        }
        if (!ax25_fix_decode_raw(raw, nraw, i, -1, -1, tmp, sizeof(tmp), &n)) {
            continue;
        }
        calc = ax25_fcs_calc(tmp, n - 2u);
        rx = (uint16_t)tmp[n - 2u] | ((uint16_t)tmp[n - 1u] << 8);
        if (calc != rx) {
            continue;
        }
        if (sanity == AX25_FIX_SANITY_ASCII &&
            !ax25_fix_sanity_ascii(tmp, n)) {
            continue;
        }
        if (n > max_out) {
            if (io_pos != NULL) {
                *io_pos = nraw;
            }
            return 0;
        }
        memcpy(out, tmp, n);
        *len = n;
        if (io_pos != NULL) {
            *io_pos = i + 1;
        }
        return 1;
    }
    if (io_pos != NULL) {
        *io_pos = nraw;
    }
    return 0;
}
