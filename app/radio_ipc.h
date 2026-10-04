#ifndef RADIO_IPC_H
#define RADIO_IPC_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * radio_ipc.h - This file is part of Griddick TNC.
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

#include <stdint.h>

#define ADC_DRAIN_MAX 64
#define IQ_N          2048
#define PCM_N         2048

struct iq_s {
    int32_t mr;
    int32_t mi;
    int32_t sr;
    int32_t si;
};

struct c1_snap {
    uint32_t seq;
    uint32_t adc;
    uint32_t feed_us;
    uint32_t isr_avg;
    uint32_t isr_max;
    int ov;
    int qov;
    uint32_t rms;
};

extern uint8_t s_txq[KISS_MAX_FRAME];
extern volatile int s_txq_n;
extern volatile int s_txq_ready;
extern volatile uint8_t s_txdelay;
extern volatile uint8_t s_persist;
extern volatile uint8_t s_slottime;
extern volatile uint8_t s_txtail;
extern volatile uint8_t s_fulldup;
extern volatile int s_tx_done;
extern volatile int s_tx_fail;

extern volatile int s_c1_ready;
extern volatile int s_c1_fail;
extern volatile struct c1_snap s_snap;
extern volatile int s_qov;
extern volatile int s_dump;
extern volatile int s_dump_abort;
extern volatile int s_inject;
extern volatile int s_inject_abort;
extern volatile int s_inject_reinit;
extern volatile int s_adc_resume;

void iq_push(int32_t mr, int32_t mi, int32_t sr, int32_t si);
int iq_pop(struct iq_s *out);
void iq_reset(void);

void pcm_reset(void);
int pcm_push(int16_t s);
int pcm_pop(int16_t *out);
int pcm_count(void);

void core1_main(void);
void core0_init(void);
void core0_poll(void);

void rx_tract_init(void);
void rx_tract_set_fix(int cnt, int sanity);
void rx_tract_feed(const struct iq_s *iq);
int rx_tract_take_frame(uint8_t *out, int max, int *n);
int rx_tract_fix_slice(void);
void rx_tract_stat_take(uint32_t *st, uint32_t *dcd, uint32_t *fr);

#endif
