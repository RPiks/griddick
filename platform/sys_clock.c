/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * sys_clock.c - This file is part of Griddick TNC.
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

#include "sys_clock.h"

#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <stdint.h>
#include <string.h>

int64_t modem_monotonic_ms(void)
{
    return (int64_t)to_ms_since_boot(get_absolute_time());
}

bool sys_clock_init(sys_clock_info_t *info)
{
    sys_clock_info_t local;
    memset(&local, 0, sizeof(local));
    local.target_hz = SYS_CLOCK_TARGET_HZ;
    local.vco_hz = SYS_CLOCK_VCO_HZ;
    local.postdiv1 = SYS_CLOCK_PD1;
    local.postdiv2 = SYS_CLOCK_PD2;

    set_sys_clock_pll(SYS_CLOCK_VCO_HZ, SYS_CLOCK_PD1, SYS_CLOCK_PD2);
    local.measured_hz = clock_get_hz(clk_sys);
    local.exact = (local.measured_hz == SYS_CLOCK_TARGET_HZ);

    if (info) {
        *info = local;
    }
    return true;
}
