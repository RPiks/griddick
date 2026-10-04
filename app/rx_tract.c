/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * rx_tract.c - This file is part of Griddick TNC.
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
 * Core 0 RX tract: baud FIR + hypot + AGC + DPLL + HDLC (after I/Q pop).
 * Same function for ADC and inject. HDLC follows host kzii: every strobe.
 */
#include "radio_ipc.h"

#include "ax25_internal.h"
#include "pixdsp_agc_i16.h"
#include "pixdsp_dpll.h"
#include "pixdsp_hessian_baud.h"
#include "pixdsp_hypot_i32.h"
#include "pixdsp_i32.h"

#include "log.h"

#include "pico.h"
#include "pico/time.h"

#include <string.h>

static pixdsp_hessian_baud_t s_bx_im;
static pixdsp_hessian_baud_t s_bx_qm;
static pixdsp_hessian_baud_t s_bx_is;
static pixdsp_hessian_baud_t s_bx_qs;
static pixdsp_agc_i16_t s_agc_m;
static pixdsp_agc_i16_t s_agc_s;
#define RX_FR_Q 8

static pixdsp_dpll_t s_dpll;
static ax25_hdlc_rx_t s_hdlc;
static int s_prev_tone;
static uint32_t s_strobe_n;
static uint32_t s_frm_n;
static uint32_t s_dcd_n;
static uint8_t s_pend[RX_FR_Q][AX25_HDLC_MAX_FRAME];
static size_t s_pend_n[RX_FR_Q];
static int s_pend_rd;
static int s_pend_wr;
static int s_pend_cnt;
static int s_fix_cnt;
static int s_fix_sanity;
#define RX_RAW_PACK ((AX25_FIX_MAX_RAW + 7) / 8)
static uint8_t s_raw_pack[RX_RAW_PACK];
static int s_raw_n;
static uint8_t s_pend_raw[RX_FR_Q][RX_RAW_PACK];
static int s_pend_raw_n[RX_FR_Q];
static uint8_t s_fix_raw[AX25_FIX_MAX_RAW];
static uint8_t s_fix_out[AX25_HDLC_MAX_FRAME];
static size_t s_fix_out_n;
static int s_fix_pos;
static int s_fix_ready;
static int s_fix_hit;
static uint32_t s_fix_tried;
static uint32_t s_fix_repaired;
static uint32_t s_fix_end;

static uint32_t fix_now_us(void)
{
    return time_us_32();
}

static void raw_reset(void)
{
    s_raw_n = 0;
}

static void raw_append(int bit)
{
    int i;

    if (s_raw_n >= AX25_FIX_MAX_RAW) {
        return;
    }
    i = s_raw_n >> 3;
    if (bit & 1) {
        s_raw_pack[i] = (uint8_t)(s_raw_pack[i] | (uint8_t)(1u << (s_raw_n & 7)));
    } else {
        s_raw_pack[i] = (uint8_t)(s_raw_pack[i] & (uint8_t)~(1u << (s_raw_n & 7)));
    }
    s_raw_n++;
}

static void raw_chop8(void)
{
    if (s_raw_n >= 8) {
        s_raw_n -= 8;
    } else {
        s_raw_n = 0;
    }
}

static void raw_unpack(const uint8_t *pack, int nbits, uint8_t *out)
{
    int i;

    for (i = 0; i < nbits; i++) {
        out[i] = (uint8_t)((pack[i >> 3] >> (i & 7)) & 1u);
    }
}

static int32_t __not_in_flash_func(box_arm)(pixdsp_hessian_baud_t *st,
                                           int32_t q30)
{
    int32_t y;

    if (q30 > PIXDSP_I32_ONE) {
        q30 = PIXDSP_I32_ONE;
    } else if (q30 < -PIXDSP_I32_ONE) {
        q30 = -PIXDSP_I32_ONE;
    }
    y = ((1 << 14) + q30) >> 15;
    return pixdsp_hessian_baud_process(st, y);
}

void rx_tract_init(void)
{
    pixdsp_dpll_params_t dp;

    pixdsp_hessian_baud_reset(&s_bx_im);
    pixdsp_hessian_baud_reset(&s_bx_qm);
    pixdsp_hessian_baud_reset(&s_bx_is);
    pixdsp_hessian_baud_reset(&s_bx_qs);
    (void)pixdsp_agc_i16_init(&s_agc_m);
    (void)pixdsp_agc_i16_init(&s_agc_s);
    memset(&dp, 0, sizeof(dp));
    dp.sample_rate_hz = 48000.0f;
    dp.baud = 1200.0f;
    dp.inertia = PIXDSP_DPLL_INERTIA_LOCKED;
    dp.searching_inertia = PIXDSP_DPLL_INERTIA_SEARCHING;
    (void)pixdsp_dpll_init(&s_dpll, &dp);
    ax25_hdlc_rx_init(&s_hdlc);
    s_prev_tone = 1;
    s_strobe_n = 0;
    s_frm_n = 0;
    s_dcd_n = 0;
    s_pend_rd = 0;
    s_pend_wr = 0;
    s_pend_cnt = 0;
    s_fix_cnt = 0;
    s_fix_sanity = AX25_FIX_SANITY_ASCII;
    s_fix_pos = -1;
    s_fix_ready = 0;
    s_fix_hit = 0;
    s_fix_out_n = 0;
    raw_reset();
}

void rx_tract_set_fix(int cnt, int sanity)
{
    s_fix_cnt = (cnt == 1) ? 1 : 0;
    s_fix_sanity = (sanity == AX25_FIX_SANITY_ASCII) ? AX25_FIX_SANITY_ASCII
                                                    : AX25_FIX_SANITY_NONE;
    if (!s_fix_cnt) {
        raw_reset();
        s_fix_pos = -1;
        s_fix_ready = 0;
        s_fix_hit = 0;
    }
}

void rx_tract_feed(const struct iq_s *iq)
{
    int32_t im;
    int32_t qm;
    int32_t is;
    int32_t qs;
    int16_t nm;
    int16_t ns;
    int32_t mm;
    int32_t ms;
    int32_t y_q15;
    int strobe;
    int bit;

    im = box_arm(&s_bx_im, iq->mr);
    qm = box_arm(&s_bx_qm, iq->mi);
    is = box_arm(&s_bx_is, iq->sr);
    qs = box_arm(&s_bx_qs, iq->si);
    mm = pixdsp_hypot_i32(im, qm);
    ms = pixdsp_hypot_i32(is, qs);
    (void)pixdsp_agc_i16_process(&s_agc_m, mm, &nm);
    (void)pixdsp_agc_i16_process(&s_agc_s, ms, &ns);
    y_q15 = (int32_t)nm - (int32_t)ns;
    (void)pixdsp_dpll_process(&s_dpll, y_q15, &strobe, &bit);
    (void)bit;
    if (pixdsp_dpll_dcd(&s_dpll)) {
        s_dcd_n++;
    }
    if (strobe) {
        int tone = (y_q15 > 0) ? 0 : 1;
        int dbit = (tone == s_prev_tone) ? 1 : 0;

        s_strobe_n++;
        s_prev_tone = tone;
        if (s_fix_cnt) {
            raw_append(tone);
        }
        if (ax25_hdlc_rx_bit(&s_hdlc, dbit)) {
            if (s_hdlc.len >= AX25_FIX_MIN_FRAME &&
                s_hdlc.len <= AX25_HDLC_MAX_FRAME) {
                s_frm_n++;
                if (s_pend_cnt < RX_FR_Q) {
                    memcpy(s_pend[s_pend_wr], s_hdlc.buf, s_hdlc.len);
                    s_pend_n[s_pend_wr] = s_hdlc.len;
                    if (s_fix_cnt) {
                        raw_chop8();
                        memcpy(s_pend_raw[s_pend_wr], s_raw_pack, RX_RAW_PACK);
                        s_pend_raw_n[s_pend_wr] = s_raw_n;
                    } else {
                        s_pend_raw_n[s_pend_wr] = 0;
                    }
                    s_pend_wr = (s_pend_wr + 1) % RX_FR_Q;
                    s_pend_cnt++;
                }
            }
            s_hdlc.len = 0;
            if (s_fix_cnt) {
                raw_reset();
                raw_append(tone);
            }
        } else if (s_fix_cnt && s_hdlc.pat_det == AX25_HDLC_FLAG) {
            raw_reset();
            raw_append(tone);
        }
    }
}

static int pend_fcs_ok(void)
{
    size_t len = s_pend_n[s_pend_rd];
    uint16_t calc;
    uint16_t rx;

    if (len < 3u) {
        return 0;
    }
    calc = ax25_fcs_calc(s_pend[s_pend_rd], len - 2u);
    rx = (uint16_t)s_pend[s_pend_rd][len - 2u] |
         ((uint16_t)s_pend[s_pend_rd][len - 1u] << 8);
    return calc == rx;
}

static void pend_pop(void)
{
    s_pend_rd = (s_pend_rd + 1) % RX_FR_Q;
    s_pend_cnt--;
    s_fix_pos = -1;
    s_fix_ready = 0;
    s_fix_hit = 0;
    s_fix_out_n = 0;
}

int rx_tract_take_frame(uint8_t *out, int max, int *n)
{
    size_t len;
    int need_fix;

    if (s_pend_cnt <= 0 || out == NULL || n == NULL) {
        return 0;
    }
    len = s_pend_n[s_pend_rd];
    if ((int)len > max) {
        pend_pop();
        return 0;
    }
    need_fix = (s_fix_cnt && s_pend_raw_n[s_pend_rd] >= 2 && !pend_fcs_ok());
    if (need_fix && s_pend_cnt > 1) {
        log_printf("Fix", "skip npend=%d nraw=%d pos=%d",
                   s_pend_cnt, s_pend_raw_n[s_pend_rd], s_fix_pos);
        memcpy(out, s_pend[s_pend_rd], len);
        *n = (int)len;
        pend_pop();
        return 1;
    }
    if (need_fix && !s_fix_ready) {
        return 0;
    }
    if (need_fix && s_fix_ready && s_fix_hit && s_fix_out_n <= (size_t)max) {
        memcpy(out, s_fix_out, s_fix_out_n);
        *n = (int)s_fix_out_n;
        pend_pop();
        return 1;
    }
    memcpy(out, s_pend[s_pend_rd], len);
    *n = (int)len;
    pend_pop();
    return 1;
}

int rx_tract_fix_slice(void)
{
    int nraw;
    uint32_t budget;
    uint32_t (*now)(void);
    size_t flen = 0;
    int hit;

    if (!s_fix_cnt || s_pend_cnt <= 0 || s_fix_ready) {
        return 0;
    }
    if (s_pend_raw_n[s_pend_rd] < 2 || pend_fcs_ok()) {
        return 0;
    }
    nraw = s_pend_raw_n[s_pend_rd];
    if (s_fix_pos < 0) {
        raw_unpack(s_pend_raw[s_pend_rd], nraw, s_fix_raw);
        s_fix_pos = 0;
        s_fix_tried++;
    }
    budget = AX25_FIX_SLICE_US;
    now = fix_now_us;
    if (s_inject) {
        budget = 0;
        now = NULL;
    }
    hit = ax25_fix_invert_single(s_fix_raw, nraw, s_fix_out, sizeof(s_fix_out),
                                 &flen, s_fix_sanity, budget, now, &s_fix_pos);
    if (hit) {
        s_fix_out_n = flen;
        s_fix_hit = 1;
        s_fix_ready = 1;
        s_fix_repaired++;
        log_printf("Fix", "hit=1 nraw=%d pos=%d tried=%lu repaired=%lu end=%lu",
                   nraw, s_fix_pos, (unsigned long)s_fix_tried,
                   (unsigned long)s_fix_repaired, (unsigned long)s_fix_end);
        return 1;
    }
    if (s_fix_pos >= nraw) {
        s_fix_hit = 0;
        s_fix_ready = 1;
        s_fix_end++;
        log_printf("Fix", "hit=0 nraw=%d pos=%d tried=%lu repaired=%lu end=%lu",
                   nraw, s_fix_pos, (unsigned long)s_fix_tried,
                   (unsigned long)s_fix_repaired, (unsigned long)s_fix_end);
        return 2;
    }
    return 0;
}

void rx_tract_stat_take(uint32_t *st, uint32_t *dcd, uint32_t *fr)
{
    if (st != NULL) {
        *st = s_strobe_n;
    }
    if (dcd != NULL) {
        *dcd = s_dcd_n;
    }
    if (fr != NULL) {
        *fr = s_frm_n;
    }
    s_strobe_n = 0;
    s_dcd_n = 0;
    s_frm_n = 0;
}
