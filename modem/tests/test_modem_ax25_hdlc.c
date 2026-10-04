/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * test_modem_ax25_hdlc.c - This file is part of Griddick TNC.
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

/*
 * HDLC + FCS unit tests (bit-level, no AFSK).
 */
#include "ax25_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static void put_call(uint8_t *dst, const char *callsign, int ext)
{
    char call[7];
    int ssid = 0;
    int i, j = 0;

    memset(call, ' ', 6);
    call[6] = '\0';
    for (i = 0; callsign[i] && callsign[i] != '-' && j < 6; i++) {
        call[j++] = callsign[i];
    }
    if (callsign[i] == '-') {
        ssid = atoi(&callsign[i + 1]);
    }
    for (i = 0; i < 6; i++) {
        dst[i] = (uint8_t)(call[i] << 1);
    }
    dst[6] = (uint8_t)(0x60 | ((ssid & 0x0f) << 1) | (ext & 1));
}

static int build_ui_frame(uint8_t *out, size_t max, const char *dst,
                          const char *src, const uint8_t *info, size_t info_len)
{
    uint8_t body[256];
    size_t n = 0;
    uint16_t fcs;

    put_call(&body[n], dst, 0);
    n += 7;
    put_call(&body[n], src, 1);
    n += 7;
    body[n++] = 0x03;
    body[n++] = 0xf0;
    if (n + info_len + 2 > sizeof(body) || n + info_len + 2 > max) {
        return -1;
    }
    memcpy(&body[n], info, info_len);
    n += info_len;
    fcs = ax25_fcs_calc(body, n);
    body[n++] = (uint8_t)(fcs & 0xff);
    body[n++] = (uint8_t)((fcs >> 8) & 0xff);
    memcpy(out, body, n);
    return (int)n;
}

static int bits_from_frame(const uint8_t *frame, int frame_len,
                           uint8_t *bits, int max_bits, int lead_flags)
{
    int n = 0;
    int ones = 0;
    int f, i, k;

    for (f = 0; f < lead_flags; f++) {
        k = ax25_hdlc_append_flag(&bits[n], max_bits - n);
        if (k < 0) {
            return -1;
        }
        n += k;
        ones = 0;
    }
    for (i = 0; i < frame_len; i++) {
        k = ax25_hdlc_stuff_byte(&bits[n], max_bits - n, frame[i], &ones);
        if (k < 0) {
            return -1;
        }
        n += k;
    }
    k = ax25_hdlc_append_flag(&bits[n], max_bits - n);
    if (k < 0) {
        return -1;
    }
    n += k;
    return n;
}

int main(void)
{
    uint8_t frame[256];
    uint8_t bits[4096];
    int frame_len;
    int nbits;
    int i;
    ax25_hdlc_rx_t hx;
    const char *info = ">TEST PACKET";
    int got = 0;

    frame_len = build_ui_frame(frame, sizeof(frame), "APRS", "N0CALL",
                               (const uint8_t *)info, strlen(info));
    if (frame_len < 16) {
        return fail("build_ui_frame");
    }
    if (ax25_fcs_calc(frame, (size_t)frame_len - 2) !=
        (uint16_t)(frame[frame_len - 2] | (frame[frame_len - 1] << 8))) {
        return fail("fcs self");
    }

    nbits = bits_from_frame(frame, frame_len, bits, (int)sizeof(bits), 4);
    /* Extra closing flags used to re-emit the same body (Pico txtail). */
    {
        int k = ax25_hdlc_append_flag(&bits[nbits], (int)sizeof(bits) - nbits);
        int k2;

        if (k < 0) {
            return fail("trail flag");
        }
        nbits += k;
        k2 = ax25_hdlc_append_flag(&bits[nbits], (int)sizeof(bits) - nbits);
        if (k2 < 0) {
            return fail("trail flag2");
        }
        nbits += k2;
    }
    if (nbits < 0) {
        return fail("stuff");
    }

    ax25_hdlc_rx_init(&hx);
    hx.olen = 0;
    for (i = 0; i < nbits; i++) {
        if (ax25_hdlc_rx_bit(&hx, bits[i])) {
            if (hx.len != (size_t)frame_len) {
                fprintf(stderr, "len %zu want %d\n", hx.len, frame_len);
                return fail("frame len");
            }
            if (memcmp(hx.buf, frame, (size_t)frame_len) != 0) {
                return fail("frame bytes");
            }
            got++;
            hx.len = 0;
        }
    }
    if (got != 1) {
        fprintf(stderr, "got %d want 1\n", got);
        return fail("frame count");
    }

    printf("test_modem_ax25_hdlc OK\n");
    return 0;
}
