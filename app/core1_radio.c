/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * core1_radio.c - This file is part of Griddick TNC.
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
 * Core 1: ADC + IFIR + mix → I/Q ring; simplex TX PWM/PTT.
 */
#include "radio_ipc.h"

#include "kiss_adc.h"
#include "kiss_pwm.h"
#include "kiss_tx.h"
#include "pixdsp_dds_i32.h"
#include "pixdsp_ifir_i32.h"

#include "pico/multicore.h"
#include "pico/stdlib.h"

#include <string.h>

static pixdsp_ifir_i32_t s_ifir;
static pixdsp_dds_i32_t s_mix_m;
static pixdsp_dds_i32_t s_mix_s;
static kiss_tx_t s_ktx;
static int s_tx_run;

static int mix_init(void)
{
    pixdsp_dds_params_t qm;
    pixdsp_dds_params_t qs;

    memset(&qm, 0, sizeof(qm));
    memset(&qs, 0, sizeof(qs));
    qm.sample_rate_hz = 48000.0f;
    qm.lo_freq_hz = 1200.0f;
    qm.mode = PIXDSP_DDS_MIX_DOWN;
    qs.sample_rate_hz = 48000.0f;
    qs.lo_freq_hz = 2200.0f;
    qs.mode = PIXDSP_DDS_MIX_DOWN;
    if (pixdsp_ifir_i32_init_bell202_p1(&s_ifir) != 0) {
        return -1;
    }
    if (pixdsp_dds_i32_init(&s_mix_m, &qm) != 0 ||
        pixdsp_dds_i32_init(&s_mix_s, &qs) != 0) {
        return -1;
    }
    return 0;
}

static void __not_in_flash_func(feed_one)(int32_t q30)
{
    int32_t x;
    int32_t zm_re;
    int32_t zm_im;
    int32_t zs_re;
    int32_t zs_im;

    x = pixdsp_ifir_i32_process(&s_ifir, q30 >> 17) << 2;
    pixdsp_dds_i32_process(&s_mix_m, x, &zm_re, &zm_im);
    pixdsp_dds_i32_process(&s_mix_s, x, &zs_re, &zs_im);
    iq_push(zm_re, zm_im, zs_re, zs_im);
}

static uint32_t isqrt_u64(uint64_t x)
{
    uint64_t op = x;
    uint64_t res = 0;
    uint64_t one = (uint64_t)1 << 62;

    while (one > op) {
        one >>= 2;
    }
    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return (uint32_t)res;
}

static void core1_publish(uint32_t adc, uint32_t feed_us, uint32_t rms)
{
    uint32_t isr_n;
    uint32_t isr_us;
    uint32_t isr_max;
    uint32_t seq;

    kiss_adc_stat_take(&isr_n, &isr_us, &isr_max);
    s_snap.adc = adc;
    s_snap.feed_us = feed_us;
    s_snap.isr_avg = (isr_n > 0u) ? (isr_us / isr_n) : 0u;
    s_snap.isr_max = isr_max;
    s_snap.ov = kiss_adc_ring_ovfl();
    s_snap.qov = s_qov;
    s_snap.rms = rms;
    seq = s_snap.seq + 1u;
    s_snap.seq = seq;
}

void core1_main(void)
{
    uint32_t adc_acc = 0;
    uint32_t feed_acc = 0;
    uint64_t sum_sq = 0;
    uint64_t period_start;

    multicore_lockout_victim_init();
    if (mix_init() != 0) {
        s_c1_fail = 1;
        return;
    }
    iq_reset();
    kiss_tx_init(&s_ktx);
    kiss_pwm_init();
    kiss_adc_init();
    kiss_adc_start();
    period_start = time_us_64();
    s_c1_ready = 1;

    for (;;) {
        int32_t q30;
        int drained = 0;
        uint64_t t0;
        uint64_t now;

        if (s_txq_ready && !s_tx_run && !kiss_pwm_busy() && !s_dump &&
            !s_dump_abort && !s_inject && !s_inject_abort) {
            int lead;
            int trail;
            int waits;

            if (!s_fulldup) {
                static uint32_t rng = 1u;

                for (waits = 0; waits < 16; waits++) {
                    rng = rng * 1103515245u + 12345u;
                    if (((rng >> 16) & 0xffu) <= s_persist) {
                        break;
                    }
                    sleep_us((uint32_t)s_slottime * 10000u);
                }
            }
            lead = kiss_tx_flag_count_10ms(s_txdelay);
            trail = kiss_tx_flag_count_10ms(s_txtail);
            kiss_adc_stop();
            if (kiss_tx_start(&s_ktx, s_txq, s_txq_n, lead, trail) != 0) {
                s_tx_fail = 1;
                s_txq_ready = 0;
                kiss_adc_start();
            } else {
                kiss_pwm_start(&s_ktx);
                s_tx_run = 1;
            }
        }
        if (s_tx_run) {
            kiss_pwm_service();
            if (!kiss_pwm_busy()) {
                s_tx_run = 0;
                s_txq_ready = 0;
                s_tx_done = 1;
                kiss_adc_start();
            }
            now = time_us_64();
            if ((uint32_t)(now - period_start) >= 1000000u) {
                core1_publish(adc_acc, feed_acc, 0);
                adc_acc = 0;
                feed_acc = 0;
                sum_sq = 0;
                period_start = now;
            }
            continue;
        }

        if (s_inject_reinit) {
            kiss_adc_stop();
            (void)mix_init();
            s_inject_reinit = 0;
        }
        if (s_adc_resume && !s_inject && !s_dump) {
            kiss_adc_start();
            s_adc_resume = 0;
        }

        t0 = time_us_64();
        if (s_inject && !s_inject_abort) {
            int16_t s16v;

            while (drained < ADC_DRAIN_MAX && pcm_pop(&s16v)) {
                q30 = (int32_t)s16v << 15;
                sum_sq += (uint64_t)((int64_t)s16v * (int64_t)s16v);
                feed_one(q30);
                drained++;
            }
        } else {
            while (drained < ADC_DRAIN_MAX && kiss_adc_pop(&q30)) {
                int32_t s16 = q30 >> 15;

                sum_sq += (uint64_t)((int64_t)s16 * (int64_t)s16);
                if (s_dump) {
                    if (s16 > 32767) {
                        s16 = 32767;
                    } else if (s16 < -32768) {
                        s16 = -32768;
                    }
                    if (!pcm_push((int16_t)s16)) {
                        s_dump = 0;
                        s_dump_abort = 1;
                    }
                } else if (!s_dump_abort) {
                    feed_one(q30);
                }
                drained++;
            }
        }
        if (drained > 0) {
            feed_acc += (uint32_t)(time_us_64() - t0);
            adc_acc += (uint32_t)drained;
        }

        now = time_us_64();
        if ((uint32_t)(now - period_start) >= 1000000u) {
            uint32_t rms = 0;

            if (adc_acc > 0u) {
                rms = isqrt_u64(sum_sq / (uint64_t)adc_acc);
            }
            core1_publish(adc_acc, feed_acc, rms);
            adc_acc = 0;
            feed_acc = 0;
            sum_sq = 0;
            period_start = now;
        }
    }
}
