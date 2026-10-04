/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * core0_usb.c - This file is part of Griddick TNC.
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
 * Core 0: USB CDC, KISS, log flush, time, drain I/Q into rx_tract.
 */
#include "radio_ipc.h"

#include "ax25_internal.h"
#include "cfg_flash.h"
#include "cpu_stat.h"
#include "kiss.h"
#include "kiss_adc.h"
#include "kiss_pwm.h"
#include "log.h"
#include "wall_time.h"

#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include <stdio.h>
#include <string.h>

static uint8_t s_wire[KISS_MAX_WIRE];
static uint8_t s_pcm_pay[KISS_PCM_SEQ + KISS_PCM_CHUNK * 2 + KISS_PCM_CRC];
static uint32_t s_usb_us;
static uint32_t s_c0_work_us;
static kiss_rx_t s_krx;
static absolute_time_t s_next_blink;
static int s_led_on;
static uint8_t s_dump_seq;
static int s_abort_logged;
static uint8_t s_inj_seq;
static int s_inj_have;
static int s_inject_stop;
static int s_inj_abort_logged;
static uint8_t s_legacy;
static uint8_t s_linkmode;
static char s_mycall[10] = "TEST-0";
static int s_paclen = 256;
static int s_maxframe = 4;
static int s_t1_ms = 2000;
static int s_t2_ms;
static int s_t3_ms = 30000;
static int s_n2 = 10;
static uint8_t s_log_events = 1;
static uint8_t s_bitfix_cnt;
static uint8_t s_bitfix_algo;
static uint8_t s_bitfix_sanity = 1;
static uint8_t s_tx_awgn_on;
static int s_tx_awgn_cb = 325;
static int s_tx_awgn_seed = 1;
static uint32_t s_uptime_s;

static void fmt_uptime(char *out, int max, uint32_t sec)
{
    unsigned d;
    unsigned h;
    unsigned m;
    unsigned s;
    int n;

    if (out == NULL || max < 4) {
        return;
    }
    d = (unsigned)(sec / 86400u);
    sec %= 86400u;
    h = (unsigned)(sec / 3600u);
    sec %= 3600u;
    m = (unsigned)(sec / 60u);
    s = (unsigned)(sec % 60u);
    if (d > 0u) {
        if (h > 0u && m > 0u) {
            n = snprintf(out, (size_t)max, "%ud%uh%um", d, h, m);
        } else if (h > 0u) {
            n = snprintf(out, (size_t)max, "%ud%uh", d, h);
        } else if (m > 0u) {
            n = snprintf(out, (size_t)max, "%ud%um", d, m);
        } else {
            n = snprintf(out, (size_t)max, "%ud", d);
        }
    } else if (h > 0u) {
        if (m > 0u) {
            n = snprintf(out, (size_t)max, "%uh%um%02us", h, m, s);
        } else {
            n = snprintf(out, (size_t)max, "%uh%02us", h, s);
        }
    } else if (m > 0u) {
        n = snprintf(out, (size_t)max, "%um%02us", m, s);
    } else {
        n = snprintf(out, (size_t)max, "%02us", s);
    }
    if (n < 0 || n >= max) {
        out[0] = '0';
        out[1] = '0';
        out[2] = 's';
        out[3] = '\0';
    }
}

static void cdc_write_all(const uint8_t *p, int n)
{
    while (n > 0) {
        uint32_t avail;
        uint32_t chunk;
        uint32_t w;

        tud_task();
        if (!tud_cdc_connected()) {
            return;
        }
        avail = tud_cdc_write_available();
        if (avail == 0u) {
            continue;
        }
        chunk = (uint32_t)n;
        if (chunk > avail) {
            chunk = avail;
        }
        w = tud_cdc_write(p, chunk);
        if (w == 0u) {
            continue;
        }
        p += w;
        n -= (int)w;
        tud_cdc_write_flush();
    }
}

static void log_flush(void)
{
    char line[LOG_LINE_MAX];
    int n;
    int n_wire;

    if (!tud_cdc_connected()) {
        return;
    }
    while ((n = log_pop(line, (int)sizeof(line))) > 0) {
        if (s_legacy && strstr(line, "[Cfg]") == NULL) {
            continue;
        }
        n_wire = kiss_encode(KISS_CMD_LOG, (const uint8_t *)line, n, s_wire,
                             (int)sizeof(s_wire));
        if (n_wire > 0) {
            cdc_write_all(s_wire, n_wire);
        }
    }
}

static void dump_send(int partial)
{
    int n;
    int i;
    int16_t s;
    uint16_t crc;
    int n_wire;

    if (s_legacy || s_dump_abort) {
        return;
    }
    for (;;) {
        n = pcm_count();
        if (n <= 0) {
            return;
        }
        if (n > KISS_PCM_CHUNK) {
            n = KISS_PCM_CHUNK;
        }
        if (n < KISS_PCM_CHUNK && !partial) {
            return;
        }
        s_pcm_pay[0] = s_dump_seq++;
        for (i = 0; i < n; i++) {
            if (!pcm_pop(&s)) {
                n = i;
                break;
            }
            s_pcm_pay[1 + i * 2] = (uint8_t)((uint16_t)s & 0xffu);
            s_pcm_pay[2 + i * 2] = (uint8_t)(((uint16_t)s >> 8) & 0xffu);
        }
        if (n <= 0) {
            return;
        }
        crc = kiss_crc16(s_pcm_pay, 1 + n * 2);
        s_pcm_pay[1 + n * 2] = (uint8_t)(crc & 0xffu);
        s_pcm_pay[2 + n * 2] = (uint8_t)((crc >> 8) & 0xffu);
        n_wire = kiss_encode(KISS_CMD_PCM, s_pcm_pay, 1 + n * 2 + 2, s_wire,
                             (int)sizeof(s_wire));
        if (n_wire > 0) {
            cdc_write_all(s_wire, n_wire);
        }
        if (n < KISS_PCM_CHUNK) {
            return;
        }
    }
}

static void dump_on(void)
{
    if (s_inject || s_inject_abort) {
        return;
    }
    s_dump = 0;
    s_dump_abort = 0;
    s_abort_logged = 0;
    pcm_reset();
    iq_reset();
    s_dump_seq = 0;
    log_printf("Dump", "on");
    log_flush();
    s_dump = 1;
}

static void dump_off(void)
{
    s_dump = 0;
    dump_send(1);
    s_dump_abort = 0;
    s_abort_logged = 0;
}

static void inject_on(void)
{
    if (s_dump || s_dump_abort) {
        return;
    }
    s_inject = 0;
    s_inject_abort = 0;
    s_inj_abort_logged = 0;
    s_inject_stop = 0;
    s_inj_have = 0;
    s_inj_seq = 0;
    pcm_reset();
    iq_reset();
    rx_tract_init();
    rx_tract_set_fix((int)s_bitfix_cnt, (int)s_bitfix_sanity);
    s_inject_reinit = 1;
    log_printf("Inject", "on");
    log_printf("Fix", "arm cnt=%d san=%d", (int)s_bitfix_cnt,
               (int)s_bitfix_sanity);
    log_flush();
    s_inject = 1;
}

static void inject_off(void)
{
    s_inject_stop = 1;
}

static void inject_pcm(const uint8_t *pay, int n_data)
{
    int body;
    uint16_t got;
    uint16_t expect;
    uint8_t seq;
    int ns;
    int i;

    if (!s_inject || s_inject_abort || pay == NULL || n_data < 3) {
        return;
    }
    body = n_data - 2;
    if ((body & 1) != 1 || body < 1) {
        return;
    }
    seq = pay[0];
    expect = kiss_crc16(pay, body);
    got = (uint16_t)pay[body] | ((uint16_t)pay[body + 1] << 8);
    ns = (body - 1) / 2;
    if (got != expect || (s_inj_have && seq != s_inj_seq)) {
        s_inject = 0;
        s_inject_abort = 1;
        return;
    }
    for (i = 0; i < ns; i++) {
        int16_t s = (int16_t)((uint16_t)pay[1 + i * 2] |
                              ((uint16_t)pay[2 + i * 2] << 8));

        if (!pcm_push(s)) {
            s_inject = 0;
            s_inject_abort = 1;
            return;
        }
    }
    s_inj_have = 1;
    s_inj_seq = (uint8_t)(seq + 1u);
}

static const uint8_t *cfg_journal(void)
{
    return (const uint8_t *)(XIP_BASE + PICO_FLASH_SIZE_BYTES -
                             CFG_FLASH_BYTES);
}

static void cfg_apply_flash(const cfg_flash_cfg_t *in)
{
    s_txdelay = in->txdelay;
    s_persist = in->persist;
    s_slottime = in->slottime;
    s_txtail = in->txtail;
    s_fulldup = in->fulldup ? 1u : 0u;
    s_legacy = in->legacy ? 1u : 0u;
    s_log_events = in->log_events ? 1u : 0u;
    log_set_events((int)s_log_events);
    s_linkmode = in->linkmode ? 1u : 0u;
    memcpy(s_mycall, in->mycall, sizeof(s_mycall));
    s_paclen = (int)in->paclen;
    s_maxframe = (int)in->maxframe;
    s_t1_ms = (int)in->t1_ms;
    s_t2_ms = (int)in->t2_ms;
    s_t3_ms = (int)in->t3_ms;
    s_n2 = (int)in->n2;
    s_bitfix_cnt = (in->bitfix_cnt == 1u) ? 1u : 0u;
    s_bitfix_algo = 0;
    s_bitfix_sanity = (in->bitfix_sanity == 1u) ? 1u : 0u;
    rx_tract_set_fix((int)s_bitfix_cnt, (int)s_bitfix_sanity);
}

static void cfg_from_ram(cfg_flash_cfg_t *out)
{
    memset(out, 0, sizeof(*out));
    out->txdelay = s_txdelay;
    out->persist = s_persist;
    out->slottime = s_slottime;
    out->txtail = s_txtail;
    out->fulldup = s_fulldup;
    out->legacy = s_legacy;
    out->log_events = s_log_events;
    out->linkmode = s_linkmode;
    memcpy(out->mycall, s_mycall, sizeof(out->mycall));
    out->paclen = (uint16_t)s_paclen;
    out->maxframe = (uint8_t)s_maxframe;
    out->t1_ms = (uint16_t)s_t1_ms;
    out->t2_ms = (uint16_t)s_t2_ms;
    out->t3_ms = (uint32_t)s_t3_ms;
    out->n2 = (uint8_t)s_n2;
    out->bitfix_cnt = s_bitfix_cnt;
    out->bitfix_algo = s_bitfix_algo;
    out->bitfix_sanity = s_bitfix_sanity;
}

static int cfg_flash_commit(void)
{
    cfg_flash_cfg_t rec;
    cfg_flash_cfg_t got;
    uint8_t page[CFG_FLASH_PAGE_SIZE];
    unsigned idx;
    uint32_t seq;
    int erase_sec;
    uint32_t base;
    uint32_t irq;

    cfg_from_ram(&rec);
    if (cfg_flash_plan(cfg_journal(), CFG_FLASH_BYTES, &idx, &seq,
                       &erase_sec) != 0) {
        return -1;
    }
    rec.seq = seq;
    if (cfg_flash_pack(page, &rec) != 0) {
        return -1;
    }
    base = PICO_FLASH_SIZE_BYTES - CFG_FLASH_BYTES;
    multicore_lockout_start_blocking();
    kiss_adc_stop();
    irq = save_and_disable_interrupts();
    if (erase_sec >= 0) {
        flash_range_erase(base + (uint32_t)erase_sec * CFG_FLASH_SECTOR_SIZE,
                          CFG_FLASH_SECTOR_SIZE);
    }
    flash_range_program(base + idx * CFG_FLASH_PAGE_SIZE, page,
                        CFG_FLASH_PAGE_SIZE);
    restore_interrupts(irq);
    kiss_adc_start();
    multicore_lockout_end_blocking();
    if (cfg_flash_unpack(cfg_journal() + idx * CFG_FLASH_PAGE_SIZE, &got) !=
            0 ||
        got.seq != rec.seq || got.txdelay != rec.txdelay ||
        got.persist != rec.persist || got.slottime != rec.slottime ||
        got.txtail != rec.txtail || got.fulldup != rec.fulldup ||
        got.legacy != rec.legacy || got.log_events != rec.log_events ||
        got.linkmode != rec.linkmode || got.paclen != rec.paclen ||
        got.maxframe != rec.maxframe || got.n2 != rec.n2 ||
        got.t1_ms != rec.t1_ms || got.t2_ms != rec.t2_ms ||
        got.t3_ms != rec.t3_ms || got.bitfix_cnt != rec.bitfix_cnt ||
        got.bitfix_algo != rec.bitfix_algo ||
        got.bitfix_sanity != rec.bitfix_sanity ||
        memcmp(got.mycall, rec.mycall, 10) != 0) {
        return -1;
    }
    return 0;
}

void core0_init(void)
{
    cfg_flash_cfg_t rec;

    kiss_rx_init(&s_krx);
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    s_next_blink = make_timeout_time_ms(250);
    s_led_on = 0;
    if (cfg_flash_scan(cfg_journal(), CFG_FLASH_BYTES, &rec) == 1) {
        cfg_apply_flash(&rec);
    } else if (cfg_flash_commit() != 0) {
        log_printf("Cfg", "flash");
    }
    rx_tract_set_fix((int)s_bitfix_cnt, (int)s_bitfix_sanity);
}

void core0_poll(void)
{
    struct iq_s iq;
    uint32_t elapsed_us;
    uint32_t loops;
    uint64_t t0;
    uint8_t frame[KISS_MAX_FRAME];
    int frame_n;
    int dumping;
    int injecting;
    int lab;

    cpu_stat_mark_loop();
    dumping = s_dump && !s_dump_abort;
    injecting = s_inject && !s_inject_abort;
    lab = dumping || injecting || s_dump_abort || s_inject_abort;

    t0 = time_us_64();
    if (!dumping && !s_dump_abort) {
        while (iq_pop(&iq)) {
            rx_tract_feed(&iq);
        }
    } else {
        while (iq_pop(&iq)) {
        }
    }
    s_c0_work_us += (uint32_t)(time_us_64() - t0);

    while (!dumping && !s_dump_abort &&
           rx_tract_take_frame(frame, (int)sizeof(frame), &frame_n)) {
        int fcs_ok = 0;

        if (frame_n >= 3) {
            uint16_t calc = ax25_fcs_calc(frame, (size_t)frame_n - 2u);
            uint16_t rx = (uint16_t)frame[frame_n - 2] |
                          ((uint16_t)frame[frame_n - 1] << 8);

            fcs_ok = (calc == rx);
        }
        log_printf("Ax25", "len=%u fcs=%d", (unsigned)frame_n, fcs_ok);
        if (fcs_ok && frame_n >= 3) {
            int n_wire = kiss_encode(KISS_CMD_DATA, frame, frame_n - 2, s_wire,
                                     (int)sizeof(s_wire));

            if (n_wire > 0) {
                cdc_write_all(s_wire, n_wire);
            }
        }
    }
    if (s_tx_done) {
        s_tx_done = 0;
        log_printf("Tx", "done");
    }
    if (s_tx_fail) {
        s_tx_fail = 0;
        log_printf("Tx", "start failed");
    }

    t0 = time_us_64();
    tud_task();
    if (!dumping && !s_dump_abort) {
        (void)rx_tract_fix_slice();
    }
    if (dumping) {
        dump_send(0);
    }
    if (s_dump_abort && !s_abort_logged) {
        s_abort_logged = 1;
        log_printf("Dump", "abort");
    }
    if (s_inject_abort && !s_inj_abort_logged) {
        s_inj_abort_logged = 1;
        log_printf("Inject", "abort");
    }
    if (s_inject_stop && pcm_count() == 0) {
        uint32_t st = 0;
        uint32_t dcd = 0;
        uint32_t fr = 0;

        rx_tract_stat_take(&st, &dcd, &fr);
        s_inject = 0;
        s_inject_stop = 0;
        s_inject_abort = 0;
        s_adc_resume = 1;
        log_printf("Inject", "off qov=%d st=%lu dcd=%lu fr=%lu",
                   s_qov, (unsigned long)st, (unsigned long)dcd,
                   (unsigned long)fr);
    }
    if (!dumping) {
        log_flush();
    }
    {
        int nb = 0;

        while (nb < (injecting ? 2048 : 32) && tud_cdc_available()) {
            uint8_t b;
            uint32_t n = tud_cdc_read(&b, 1);

            if (n == 0u) {
                break;
            }
            nb++;
            if (!kiss_rx_byte(&s_krx, b)) {
                continue;
            }
            {
                uint8_t cmd = (uint8_t)(s_krx.buf[0] & KISS_CMD_MASK);
                int n_data = s_krx.frame_len - 1;

                if (cmd == KISS_CMD_PCM && n_data > 0) {
                    if (!s_legacy) {
                        inject_pcm(s_krx.buf + 1, n_data);
                    }
                } else if (cmd == KISS_CMD_DATA && n_data > 0) {
                    if (s_dump || s_dump_abort || s_inject || s_inject_abort) {
                        /* lab owns the tract; no TX */
                    } else if (s_txq_ready) {
                        log_printf("Kiss", "drop len=%d", n_data);
                    } else {
                        memcpy(s_txq, s_krx.buf + 1, (size_t)n_data);
                        s_txq_n = n_data;
                        s_txq_ready = 1;
                        log_printf("Kiss", "queued len=%d", n_data);
                    }
                } else if (cmd == KISS_CMD_SETHW && n_data >= 1) {
                    uint8_t hw = s_krx.buf[1];

                    if (s_legacy && hw != KISS_HW_CFG) {
                        /* TAPR SETHW except cfg is off */
                    } else if (hw == KISS_HW_TIME) {
                        if (n_data >= 9) {
                            uint64_t utc = 0;
                            int i;

                            for (i = 0; i < 8; i++) {
                                utc |= (uint64_t)s_krx.buf[2 + i] << (8 * i);
                            }
                            wall_time_set_utc_ms(utc);
                            log_printf("Time", "set utc=%llu",
                                       (unsigned long long)utc);
                        } else {
                            log_printf("Time", "utc=%llu",
                                       (unsigned long long)wall_time_utc_ms());
                        }
                    } else if (hw == KISS_HW_DUMP_ON) {
                        dump_on();
                        dumping = s_dump && !s_dump_abort;
                    } else if (hw == KISS_HW_DUMP_OFF) {
                        dump_off();
                        dumping = 0;
                        log_printf("Dump", "off");
                    } else if (hw == KISS_HW_INJECT_ON) {
                        inject_on();
                        injecting = s_inject && !s_inject_abort;
                    } else if (hw == KISS_HW_INJECT_OFF) {
                        inject_off();
                    } else if (hw == KISS_HW_CFG) {
                        char js[LOG_LINE_MAX];
                        kiss_cfg_t cur;
                        int jn;
                        int wn;
                        int reply = 1;

                        if (n_data > 1 && s_txq_ready) {
                            log_printf("Cfg", "tx");
                            reply = 0;
                        } else if (n_data > 1) {
                            int off = 0;
                            int jlen = 0;
                            kiss_cfg_t in;
                            int ur;

                            ur = kiss_cfg_unwrap(
                                (const char *)(s_krx.buf + 2), n_data - 1,
                                &off, &jlen);
                            if (ur != 1 ||
                                kiss_cfg_from_json(
                                    (const char *)(s_krx.buf + 2 + off),
                                    jlen, &in) != 0) {
                                log_printf("Cfg", "crc");
                                reply = 0;
                            } else {
                                if (in.have_txdelay) {
                                    s_txdelay = (uint8_t)in.txdelay;
                                }
                                if (in.have_persist) {
                                    s_persist = (uint8_t)in.persist;
                                }
                                if (in.have_slottime) {
                                    s_slottime = (uint8_t)in.slottime;
                                }
                                if (in.have_txtail) {
                                    s_txtail = (uint8_t)in.txtail;
                                }
                                if (in.have_fulldup) {
                                    s_fulldup = in.fulldup ? 1u : 0u;
                                }
                                if (in.have_legacy) {
                                    s_legacy = in.legacy ? 1u : 0u;
                                    if (s_legacy) {
                                        dump_off();
                                        inject_off();
                                        dumping = 0;
                                        injecting = 0;
                                    }
                                }
                                if (in.have_log_events) {
                                    s_log_events = in.log_events ? 1u : 0u;
                                    log_set_events((int)s_log_events);
                                }
                                if (in.have_linkmode) {
                                    s_linkmode = in.linkmode ? 1u : 0u;
                                }
                                if (in.have_mycall) {
                                    memcpy(s_mycall, in.mycall,
                                           sizeof(s_mycall));
                                }
                                if (in.have_paclen) {
                                    s_paclen = in.paclen;
                                }
                                if (in.have_maxframe) {
                                    s_maxframe = in.maxframe;
                                }
                                if (in.have_t1) {
                                    s_t1_ms = in.t1_ms;
                                }
                                if (in.have_t2) {
                                    s_t2_ms = in.t2_ms;
                                }
                                if (in.have_t3) {
                                    s_t3_ms = in.t3_ms;
                                }
                                if (in.have_n2) {
                                    s_n2 = in.n2;
                                }
                                if (in.have_bitfix_cnt) {
                                    s_bitfix_cnt = in.bitfix_cnt ? 1u : 0u;
                                }
                                if (in.have_bitfix_algo) {
                                    s_bitfix_algo = 0;
                                }
                                if (in.have_bitfix_sanity) {
                                    s_bitfix_sanity =
                                        in.bitfix_sanity ? 1u : 0u;
                                }
                                if (in.have_bitfix_cnt ||
                                    in.have_bitfix_sanity) {
                                    rx_tract_set_fix((int)s_bitfix_cnt,
                                                     (int)s_bitfix_sanity);
                                }
                                if (in.have_tx_awgn_on) {
                                    s_tx_awgn_on = in.tx_awgn_on ? 1u : 0u;
                                }
                                if (in.have_tx_awgn_cb) {
                                    s_tx_awgn_cb = in.tx_awgn_cb;
                                }
                                if (in.have_tx_awgn_seed) {
                                    s_tx_awgn_seed = in.tx_awgn_seed;
                                }
                                if (in.have_tx_awgn_on || in.have_tx_awgn_cb ||
                                    in.have_tx_awgn_seed) {
                                    kiss_pwm_set_awgn((int)s_tx_awgn_on,
                                                      s_tx_awgn_cb,
                                                      (uint32_t)s_tx_awgn_seed);
                                }
                                if (cfg_flash_commit() != 0) {
                                    log_printf("Cfg", "flash");
                                }
                            }
                        }
                        if (reply) {
                            memset(&cur, 0, sizeof(cur));
                            cur.txdelay = s_txdelay;
                            cur.persist = s_persist;
                            cur.slottime = s_slottime;
                            cur.txtail = s_txtail;
                            cur.fulldup = s_fulldup;
                            cur.legacy = s_legacy;
                            cur.log_events = s_log_events;
                            cur.linkmode = s_linkmode;
                            memcpy(cur.mycall, s_mycall, sizeof(cur.mycall));
                            cur.paclen = s_paclen;
                            cur.maxframe = s_maxframe;
                            cur.t1_ms = s_t1_ms;
                            cur.t2_ms = s_t2_ms;
                            cur.t3_ms = s_t3_ms;
                            cur.n2 = s_n2;
                            cur.bitfix_cnt = s_bitfix_cnt;
                            cur.bitfix_algo = s_bitfix_algo;
                            cur.bitfix_sanity = s_bitfix_sanity;
                            cur.tx_awgn_on = s_tx_awgn_on;
                            cur.tx_awgn_cb = s_tx_awgn_cb;
                            cur.tx_awgn_seed = s_tx_awgn_seed;
                            jn = kiss_cfg_to_json(&cur, NULL, js,
                                                  (int)sizeof(js) - 8);
                            if (jn > 0) {
                                wn = kiss_cfg_wrap(js, jn, (int)sizeof(js));
                                if (wn > 0) {
                                    log_printf("Cfg", "%s", js);
                                }
                            }
                        }
                    }
                } else if (n_data >= 1) {
                    uint8_t v = s_krx.buf[1];

                    if (cmd == KISS_CMD_TXDELAY) {
                        s_txdelay = v;
                    } else if (cmd == KISS_CMD_PERSIST) {
                        s_persist = v;
                    } else if (cmd == KISS_CMD_SLOTTIME) {
                        s_slottime = v;
                    } else if (cmd == KISS_CMD_TXTAIL) {
                        s_txtail = v;
                    } else if (cmd == KISS_CMD_FULLDUP) {
                        s_fulldup = v ? 1u : 0u;
                    }
                    if (!lab) {
                        log_printf("Kiss", "cmd=0x%02x val=%u", cmd,
                                   (unsigned)v);
                    }
                } else if (!lab) {
                    log_printf("Kiss", "cmd=0x%02x n=%d", cmd,
                               n_data < 0 ? 0 : n_data);
                }
            }
        }
    }
    s_usb_us += (uint32_t)(time_us_64() - t0);
    if (!lab) {
        sleep_us(1000);
    }

    if (!lab && cpu_stat_take(&elapsed_us, &loops)) {
        uint32_t used;
        uint32_t used_pct;
        uint32_t adc = 0;
        uint32_t feed_us = 0;
        int ov = 0;
        int qov = 0;
        uint32_t rms = 0;
        uint32_t c1_used;

        if (s_snap.seq != 0u) {
            adc = s_snap.adc;
            feed_us = s_snap.feed_us;
            ov = s_snap.ov;
            qov = s_snap.qov;
            rms = s_snap.rms;
        }
        rx_tract_stat_take(NULL, NULL, NULL);
        used = s_usb_us + s_c0_work_us;
        if (used > elapsed_us) {
            used = elapsed_us;
        }
        used_pct = (elapsed_us > 0u) ? (used * 100u / elapsed_us) : 0u;
        c1_used = (feed_us >= 1000000u) ? 100u : (feed_us * 100u / 1000000u);
        {
            char up[20];

            s_uptime_s = (uint32_t)(time_us_64() / 1000000ull);
            fmt_uptime(up, (int)sizeof(up), s_uptime_s);
            log_printf("Runtime",
                       "CoreLoad:%lu/%lu, Uptime:%s, ADCrate:%lu, OVF:%d, QOV:%d, RMS:%lu",
                       (unsigned long)used_pct, (unsigned long)c1_used, up,
                       (unsigned long)adc, ov, qov, (unsigned long)rms);
        }
        s_usb_us = 0;
        s_c0_work_us = 0;
    }

    if (!s_snap.ov && !s_snap.qov && !s_dump_abort && !s_inject_abort &&
        time_reached(s_next_blink)) {
        s_led_on = !s_led_on;
        gpio_put(PICO_DEFAULT_LED_PIN, s_led_on);
        s_next_blink = delayed_by_ms(s_next_blink,
                                     tud_cdc_connected() ? 250 : 1000);
    }
    if (s_snap.ov || s_snap.qov || s_dump_abort || s_inject_abort) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
    }
}
