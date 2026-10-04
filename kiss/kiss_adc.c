/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * kiss_adc.c - This file is part of Griddick TNC.
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

#include "kiss_adc.h"

#include "pixdsp_polydec.h"

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/time.h"

#define ADC_CH        2u
#define ADC_GPIO      28u
#define ADC_CLKDIV    199.0f /* 48e6 / 200 = 240 kHz */
#define ADC_M         5u
#define Q_BUF         256

static int s_dma_chan;
static uint16_t s_raw[ADC_M];
static pixdsp_polydec_t s_poly;
static int32_t s_q[Q_BUF];
static volatile uint16_t s_wr;
static volatile uint16_t s_rd;
static volatile int s_run;
static volatile int s_ovfl;
static volatile uint32_t s_isr_n;
static volatile uint32_t s_isr_us;
static volatile uint32_t s_isr_max;

static void adc_restart(void)
{
    dma_channel_set_write_addr(s_dma_chan, s_raw, false);
    dma_channel_set_trans_count(s_dma_chan, ADC_M, true);
}

static void __not_in_flash_func(kiss_adc_dma_irq)(void)
{
    uint64_t t0;
    uint32_t dt;

    if (!dma_channel_get_irq0_status(s_dma_chan)) {
        return;
    }
    dma_channel_acknowledge_irq0(s_dma_chan);
    t0 = time_us_64();
    if (s_run) {
        int32_t acc;
        uint16_t nwr;

        acc = pixdsp_polydec_process_m5(&s_poly, s_raw);
        nwr = (uint16_t)((s_wr + 1u) % Q_BUF);
        if (nwr != s_rd) {
            s_q[s_wr] = acc;
            s_wr = nwr;
        } else {
            s_ovfl = 1;
        }
        adc_restart();
    }
    dt = (uint32_t)(time_us_64() - t0);
    s_isr_n++;
    s_isr_us += dt;
    if (dt > s_isr_max) {
        s_isr_max = dt;
    }
}

void kiss_adc_init(void)
{
    dma_channel_config cfg;

    clock_configure(clk_adc, 0,
                    CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48000000, 48000000);

    s_wr = 0;
    s_rd = 0;
    s_run = 0;
    s_isr_n = 0;
    s_isr_us = 0;
    s_isr_max = 0;
    pixdsp_polydec_reset(&s_poly);

    adc_init();
    adc_gpio_init(ADC_GPIO);
    adc_select_input(ADC_CH);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(ADC_CLKDIV);

    s_dma_chan = dma_claim_unused_channel(true);
    cfg = dma_channel_get_default_config(s_dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);
    dma_channel_configure(s_dma_chan, &cfg, s_raw, &adc_hw->fifo, ADC_M, false);
    dma_channel_set_irq0_enabled(s_dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, kiss_adc_dma_irq);
    irq_set_enabled(DMA_IRQ_0, true);
}

void kiss_adc_start(void)
{
    adc_fifo_drain();
    pixdsp_polydec_reset(&s_poly);
    s_wr = 0;
    s_rd = 0;
    s_run = 1;
    adc_restart();
    adc_run(true);
}

void kiss_adc_stop(void)
{
    s_run = 0;
    adc_run(false);
    dma_channel_abort(s_dma_chan);
    adc_fifo_drain();
    s_wr = 0;
    s_rd = 0;
}

int kiss_adc_pop(int32_t *q30)
{
    uint16_t rd;

    if (q30 == NULL || s_rd == s_wr) {
        return 0;
    }
    rd = s_rd;
    *q30 = s_q[rd];
    s_rd = (uint16_t)((rd + 1u) % Q_BUF);
    return 1;
}

int kiss_adc_ring_ovfl(void)
{
    return s_ovfl;
}

void kiss_adc_stat_take(uint32_t *n, uint32_t *us, uint32_t *max_us)
{
    if (n != NULL) {
        *n = s_isr_n;
    }
    if (us != NULL) {
        *us = s_isr_us;
    }
    if (max_us != NULL) {
        *max_us = s_isr_max;
    }
    s_isr_n = 0;
    s_isr_us = 0;
    s_isr_max = 0;
}
