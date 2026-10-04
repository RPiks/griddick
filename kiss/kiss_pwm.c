/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * kiss_pwm.c - This file is part of Griddick TNC.
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

#include "kiss_pwm.h"
#include "awgn_pn.h"
#include "gpio_ctrl.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"

#define PTT_PIN 18u
#define HOLD_M  10u
#define TX_PEAK 262
#define AWGN_SIGMA 46300

static uint s_slice;
static uint s_chan;
static volatile uint16_t s_level = KISS_TX_MID;
static volatile uint8_t s_hold;
static volatile uint8_t s_busy;
static volatile uint8_t s_done;
static kiss_tx_t *s_tx;
static volatile int s_awgn_on;
static volatile int s_awgn_cb = 325;
static uint32_t s_awgn_st = 1;
static uint32_t s_awgn_seed = 1;

static int awgn_lin_q8(int cb)
{
    static const uint16_t t[49] = {
        256, 263, 271, 279, 287, 296, 304, 313, 322, 332, 341, 351, 362,
        372, 383, 394, 406, 418, 430, 442, 455, 469, 482, 496, 511, 526,
        541, 557, 573, 590, 607, 625, 643, 662, 681, 701, 722, 743, 764,
        787, 810, 833, 858, 883, 908, 935, 962, 990, 1019
    };
    int i;

    if (cb < 0) {
        cb = 0;
    }
    if (cb > 1200) {
        cb = 1200;
    }
    i = cb / 25;
    return (int)t[i];
}

static uint16_t __not_in_flash_func(awgn_on_lvl)(uint16_t lvl)
{
    int32_t nrms;
    int32_t n;
    int32_t y;
    int lin;

    if (!s_awgn_on) {
        return lvl;
    }
    lin = awgn_lin_q8(s_awgn_cb);
    nrms = ((int32_t)TX_PEAK * 181) / lin;
    if (nrms < 1) {
        nrms = 1;
    }
    n = (awgn_raw(&s_awgn_st) * nrms) / AWGN_SIGMA;
    y = (int32_t)lvl + n;
    if (y < 0) {
        y = 0;
    } else if (y > (int32_t)KISS_TX_WRAP) {
        y = (int32_t)KISS_TX_WRAP;
    }
    return (uint16_t)y;
}

void kiss_pwm_set_awgn(int on, int snr_cb, uint32_t seed)
{
    s_awgn_on = on ? 1 : 0;
    if (snr_cb >= 200 && snr_cb <= 1200) {
        s_awgn_cb = snr_cb;
    }
    if (seed == 0u) {
        seed = 1u;
    }
    s_awgn_seed = seed;
    s_awgn_st = seed;
}

static void __not_in_flash_func(kiss_pwm_irq)(void)
{
    uint32_t mask = pwm_get_irq_status_mask();

    if (!(mask & (1u << s_slice))) {
        return;
    }
    pwm_clear_irq(s_slice);

    if (s_hold == 0u) {
        uint16_t lvl;

        if (s_tx != NULL && kiss_tx_next(s_tx, &lvl)) {
            s_level = awgn_on_lvl(lvl);
        } else {
            s_level = KISS_TX_MID;
            s_done = 1;
            pwm_set_irq_enabled(s_slice, false);
        }
    }
    pwm_set_chan_level(s_slice, s_chan, s_level);
    s_hold++;
    if (s_hold >= HOLD_M) {
        s_hold = 0;
    }
}

void kiss_pwm_init(void)
{
    pwm_config cfg;

    gpio_ctrl_set_mode((uint8_t)PTT_PIN, GPIO_CTRL_MODE_OUT);
    gpio_ctrl_write((uint8_t)PTT_PIN, false);

    gpio_set_function(0, GPIO_FUNC_PWM);
    s_slice = pwm_gpio_to_slice_num(0);
    s_chan = pwm_gpio_to_channel(0);
    s_level = KISS_TX_MID;
    s_hold = 0;
    s_busy = 0;
    s_done = 0;
    s_tx = NULL;

    cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&cfg, 1);
    pwm_config_set_wrap(&cfg, (uint16_t)KISS_TX_WRAP);
    pwm_init(s_slice, &cfg, true);
    pwm_set_chan_level(s_slice, s_chan, KISS_TX_MID);

    irq_set_exclusive_handler(PWM_IRQ_WRAP, kiss_pwm_irq);
    irq_set_priority(PWM_IRQ_WRAP, 0x40);
    irq_set_enabled(PWM_IRQ_WRAP, true);
}

void kiss_pwm_start(kiss_tx_t *tx)
{
    if (tx == NULL || s_busy) {
        return;
    }
    s_tx = tx;
    s_hold = 0;
    s_level = KISS_TX_MID;
    s_done = 0;
    s_busy = 1;
    gpio_ctrl_write((uint8_t)PTT_PIN, true);
    pwm_clear_irq(s_slice);
    pwm_set_irq_enabled(s_slice, true);
}

void kiss_pwm_service(void)
{
    if (!s_done) {
        return;
    }
    s_done = 0;
    s_busy = 0;
    gpio_ctrl_write((uint8_t)PTT_PIN, false);
    s_level = KISS_TX_MID;
    pwm_set_chan_level(s_slice, s_chan, KISS_TX_MID);
    s_tx = NULL;
}

int kiss_pwm_busy(void)
{
    return s_busy ? 1 : 0;
}
