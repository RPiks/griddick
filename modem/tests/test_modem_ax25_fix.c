/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * test_modem_ax25_fix.c - This file is part of Griddick TNC.
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
 * F=1 raw-tone invert. Switch: --fix / --no-fix (default: both).
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

static int stuff_body(const uint8_t *frame, int frame_len, uint8_t *dbits,
                      int max_bits)
{
    int n = 0;
    int ones = 0;
    int i;
    int k;

    for (i = 0; i < frame_len; i++) {
        k = ax25_hdlc_stuff_byte(&dbits[n], max_bits - n, frame[i], &ones);
        if (k < 0) {
            return -1;
        }
        n += k;
    }
    return n;
}

static int nrzi_tones(const uint8_t *dbits, int nbits, int start_tone,
                      uint8_t *raw, int max_raw)
{
    int tone = start_tone & 1;
    int i;

    if (nbits + 1 > max_raw) {
        return -1;
    }
    raw[0] = (uint8_t)tone;
    for (i = 0; i < nbits; i++) {
        if ((dbits[i] & 1) == 0) {
            tone ^= 1;
        }
        raw[i + 1] = (uint8_t)tone;
    }
    return nbits + 1;
}

static int fcs_ok(const uint8_t *frame, size_t len)
{
    uint16_t calc;
    uint16_t rx;

    if (len < 3) {
        return 0;
    }
    calc = ax25_fcs_calc(frame, len - 2);
    rx = (uint16_t)frame[len - 2] | ((uint16_t)frame[len - 1] << 8);
    return calc == rx;
}

int main(int argc, char **argv)
{
    uint8_t frame[256];
    uint8_t dbits[2048];
    uint8_t raw[2048];
    uint8_t broken[2048];
    uint8_t out[256];
    int frame_len;
    int nbits;
    int nraw;
    int flip;
    size_t olen = 0;
    int do_fix = 1;
    int do_nofix = 1;
    const char *info = ">TEST PACKET";
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fix") == 0) {
            do_nofix = 0;
        } else if (strcmp(argv[i], "--no-fix") == 0) {
            do_fix = 0;
        } else if (strcmp(argv[i], "-h") == 0 ||
                   strcmp(argv[i], "--help") == 0) {
            fprintf(stderr,
                    "Usage: test_modem_ax25_fix [--fix|--no-fix]\n"
                    "  default: both. --fix = invert-single recovers.\n"
                    "  --no-fix = flipped raw stays bad FCS.\n");
            return 0;
        } else {
            return fail("bad arg");
        }
    }

    frame_len = build_ui_frame(frame, sizeof(frame), "APRS", "N0CALL",
                               (const uint8_t *)info, strlen(info));
    if (frame_len < AX25_FIX_MIN_FRAME) {
        return fail("build");
    }
    nbits = stuff_body(frame, frame_len, dbits, (int)sizeof(dbits));
    if (nbits < 8) {
        return fail("stuff");
    }
    nraw = nrzi_tones(dbits, nbits, 1, raw, (int)sizeof(raw));
    if (nraw < 2) {
        return fail("nrzi");
    }
    if (!ax25_fix_decode_raw(raw, nraw, -1, -1, -1, out, sizeof(out), &olen) ||
        (int)olen != frame_len || memcmp(out, frame, (size_t)frame_len) != 0) {
        return fail("clean decode");
    }

    flip = nraw / 2;
    if (flip < 1) {
        flip = 1;
    }
    memcpy(broken, raw, (size_t)nraw);
    broken[flip] ^= 1u;
    if (ax25_fix_decode_raw(broken, nraw, -1, -1, -1, out, sizeof(out),
                            &olen) &&
        fcs_ok(out, olen)) {
        return fail("flipped still good");
    }

    if (do_nofix) {
        if (ax25_fix_decode_raw(broken, nraw, -1, -1, -1, out, sizeof(out),
                                &olen) &&
            fcs_ok(out, olen)) {
            return fail("no-fix recovered");
        }
    }

    if (do_fix) {
        olen = 0;
        if (!ax25_fix_invert_single(broken, nraw, out, sizeof(out), &olen,
                                    AX25_FIX_SANITY_ASCII, 0, NULL, NULL)) {
            return fail("fix missed");
        }
        if ((int)olen != frame_len ||
            memcmp(out, frame, (size_t)frame_len) != 0) {
            return fail("fix bytes");
        }
        {
            uint8_t bininfo[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                  0x07};
            uint8_t bframe[256];
            uint8_t bdbits[2048];
            uint8_t braw[2048];
            int blen;
            int bb;
            int br;
            int bf;

            blen = build_ui_frame(bframe, sizeof(bframe), "APRS", "N0CALL",
                                  bininfo, sizeof(bininfo));
            bb = stuff_body(bframe, blen, bdbits, (int)sizeof(bdbits));
            br = nrzi_tones(bdbits, bb, 1, braw, (int)sizeof(braw));
            bf = br / 2;
            if (bf < 1) {
                bf = 1;
            }
            braw[bf] ^= 1u;
            olen = 0;
            if (ax25_fix_invert_single(braw, br, out, sizeof(out), &olen,
                                       AX25_FIX_SANITY_ASCII, 0, NULL, NULL)) {
                return fail("ascii accepted binary");
            }
            olen = 0;
            if (!ax25_fix_invert_single(braw, br, out, sizeof(out), &olen,
                                        AX25_FIX_SANITY_NONE, 0, NULL, NULL)) {
                return fail("none missed binary");
            }
        }
    }

    printf("test_modem_ax25_fix OK\n");
    return 0;
}
