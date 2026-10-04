/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * main.c - This file is part of Griddick TNC.
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
 * Griddick KISS UF2 — C2a: thin main, cores split.
 */
#include "radio_ipc.h"

#include "cpu_stat.h"
#include "log.h"
#include "sys_clock.h"
#include "wall_time.h"

#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "tusb.h"

int main(void)
{
    sys_clock_info_t clk;

    sys_clock_init(&clk);
    (void)clk;
    wall_time_init();
    log_init();
    cpu_stat_init();
    tusb_init();
    log_printf("Main", "Started");

    s_c1_ready = 0;
    s_c1_fail = 0;
    s_snap.seq = 0;
    multicore_launch_core1(core1_main);

    while (!s_c1_ready && !s_c1_fail) {
        tud_task();
    }
    if (s_c1_fail) {
        log_printf("Core", "c1 init failed");
    }
    rx_tract_init();
    core0_init();

    for (;;) {
        core0_poll();
    }
}
