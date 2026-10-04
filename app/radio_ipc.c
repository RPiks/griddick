/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * radio_ipc.c - This file is part of Griddick TNC.
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

#include "radio_ipc.h"

#include "pico.h"

uint8_t s_txq[KISS_MAX_FRAME];
volatile int s_txq_n;
volatile int s_txq_ready;
volatile uint8_t s_txdelay = 30;
volatile uint8_t s_persist = 63;
volatile uint8_t s_slottime = 0;
volatile uint8_t s_txtail = 2;
volatile uint8_t s_fulldup = 0;
volatile int s_tx_done;
volatile int s_tx_fail;

volatile int s_c1_ready;
volatile int s_c1_fail;
volatile struct c1_snap s_snap;
volatile int s_qov;
volatile int s_dump;
volatile int s_dump_abort;
volatile int s_inject;
volatile int s_inject_abort;
volatile int s_inject_reinit;
volatile int s_adc_resume;

static struct iq_s s_iq[IQ_N];
static volatile uint16_t s_iq_wr;
static volatile uint16_t s_iq_rd;
static int16_t s_pcm[PCM_N];
static volatile uint16_t s_pcm_wr;
static volatile uint16_t s_pcm_rd;

void iq_reset(void)
{
    s_iq_wr = 0;
    s_iq_rd = 0;
    s_qov = 0;
}

void __not_in_flash_func(iq_push)(int32_t mr, int32_t mi, int32_t sr, int32_t si)
{
    uint16_t wr = s_iq_wr;
    uint16_t nwr = (uint16_t)((wr + 1u) % IQ_N);

    if (nwr == s_iq_rd) {
        s_qov = 1;
        return;
    }
    s_iq[wr].mr = mr;
    s_iq[wr].mi = mi;
    s_iq[wr].sr = sr;
    s_iq[wr].si = si;
    s_iq_wr = nwr;
}

int __not_in_flash_func(iq_pop)(struct iq_s *out)
{
    uint16_t rd = s_iq_rd;

    if (rd == s_iq_wr) {
        return 0;
    }
    *out = s_iq[rd];
    s_iq_rd = (uint16_t)((rd + 1u) % IQ_N);
    return 1;
}

void pcm_reset(void)
{
    s_pcm_wr = 0;
    s_pcm_rd = 0;
}

int __not_in_flash_func(pcm_push)(int16_t s)
{
    uint16_t wr = s_pcm_wr;
    uint16_t nwr = (uint16_t)((wr + 1u) % PCM_N);

    if (nwr == s_pcm_rd) {
        return 0;
    }
    s_pcm[wr] = s;
    s_pcm_wr = nwr;
    return 1;
}

int pcm_pop(int16_t *out)
{
    uint16_t rd = s_pcm_rd;

    if (out == NULL || rd == s_pcm_wr) {
        return 0;
    }
    *out = s_pcm[rd];
    s_pcm_rd = (uint16_t)((rd + 1u) % PCM_N);
    return 1;
}

int pcm_count(void)
{
    uint16_t wr = s_pcm_wr;
    uint16_t rd = s_pcm_rd;

    if (wr >= rd) {
        return (int)(wr - rd);
    }
    return (int)(PCM_N - rd + wr);
}
