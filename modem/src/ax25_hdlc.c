/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * ax25_hdlc.c - This file is part of Griddick TNC.
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
 * @file ax25_hdlc.c
 * @brief HDLC flag hunt / unstuff (Direwolf-compatible pattern detector).
 *
 * Input bits are post-NRZI data bits (dbit). Octets are LSB-first on the wire.
 */
#include "ax25_internal.h"

#include <string.h>

/** @brief Idle hunter: olen = −1, empty body. */
void ax25_hdlc_rx_init(ax25_hdlc_rx_t *h)
{
    if (h == NULL) {
        return;
    }
    memset(h, 0, sizeof(*h));
    h->olen = -1;
}

/** @brief Write 8 LSB-first flag bits (0x7E). @return 8, or −1. */
int ax25_hdlc_append_flag(uint8_t *bit_out, int max_bits)
{
    static const uint8_t flag_bits[8] = {0, 1, 1, 1, 1, 1, 1, 0};
    int i;

    if (bit_out == NULL || max_bits < 8) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        bit_out[i] = flag_bits[i];
    }
    return 8;
}

/**
 * @brief Stuff one octet LSB-first; insert 0 after five consecutive 1s.
 * @return Bits written, or −1.
 */
int ax25_hdlc_stuff_byte(uint8_t *bit_out, int max_bits, uint8_t byte, int *ones)
{
    int n = 0;
    int i;
    int o;

    if (bit_out == NULL || ones == NULL || max_bits < 8) {
        return -1;
    }
    o = *ones;
    for (i = 0; i < 8; i++) {
        int bit = (byte >> i) & 1;
        if (n >= max_bits) {
            return -1;
        }
        bit_out[n++] = (uint8_t)bit;
        if (bit) {
            o++;
            if (o == 5) {
                if (n >= max_bits) {
                    return -1;
                }
                bit_out[n++] = 0;
                o = 0;
            }
        } else {
            o = 0;
        }
    }
    *ones = o;
    return n;
}

/**
 * @brief One post-NRZI bit: flag / abort / destuff / octet assemble.
 * @return 1 when h->buf holds Address..FCS.
 */
int ax25_hdlc_rx_bit(ax25_hdlc_rx_t *h, int dbit)
{
    int emit = 0;

    if (h == NULL) {
        return 0;
    }
    dbit &= 1;

    h->pat_det = (uint8_t)(h->pat_det >> 1);
    if (dbit) {
        h->pat_det = (uint8_t)(h->pat_det | 0x80u);
    }

    if (h->pat_det == AX25_HDLC_FLAG) {
        /* Closing/opening flag. olen==7 → flag fragment in oacc only.
         * Extra trailing flags also arrive with olen==7; require a new
         * octet since the last flag or the same body is emitted again. */
        if (h->olen == 7 && h->len >= 3 && h->got_octet) {
            emit = 1;
        }
        h->got_octet = 0;
        if (!h->data_seen && !emit) {
            h->flags_leading++;
        }
        if (emit) {
            h->data_seen = 1;
            h->in_frame = 1;
            h->olen = 0;
            return 1;
        }
        h->len = 0;
        h->in_frame = 1;
        h->olen = 0;
        h->oacc = 0;
        return 0;
    }

    if (h->pat_det == 0xfeu) {
        h->olen = -1;
        h->len = 0;
        h->in_frame = 0;
        return 0;
    }

    if ((h->pat_det & 0xfcu) == 0x7cu) {
        /* stuffed 0 after five 1s — discard */
        return 0;
    }

    if (h->olen < 0) {
        return 0;
    }

    h->oacc = (uint8_t)(h->oacc >> 1);
    if (dbit) {
        h->oacc = (uint8_t)(h->oacc | 0x80u);
    }
    h->olen++;
    if (h->olen >= 8) {
        if (h->len < AX25_HDLC_MAX_FRAME) {
            h->buf[h->len++] = h->oacc;
            h->data_seen = 1;
            h->got_octet = 1;
        }
        h->olen = 0;
    }
    return 0;
}
