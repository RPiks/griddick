#ifndef SYS_CLOCK_H
#define SYS_CLOCK_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * sys_clock.h - This file is part of Griddick TNC.
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

/**
 * @file sys_clock.h
 * @brief Configure clk_sys from 12 MHz XOSC. Default 120 MHz; kiss may override.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SYS_CLOCK_TARGET_HZ
#define SYS_CLOCK_TARGET_HZ  120000000u
#endif
#ifndef SYS_CLOCK_VCO_HZ
#define SYS_CLOCK_VCO_HZ     1200000000u
#endif
#ifndef SYS_CLOCK_PD1
#define SYS_CLOCK_PD1        5u
#endif
#ifndef SYS_CLOCK_PD2
#define SYS_CLOCK_PD2        2u
#endif

typedef struct {
    uint32_t target_hz;
    uint32_t measured_hz;
    uint32_t vco_hz;
    uint32_t postdiv1;
    uint32_t postdiv2;
    bool exact;
} sys_clock_info_t;

/**
 * @brief Configure clk_sys via PLL.
 * @param info Optional measured/target info out.
 * @return true if a PLL config was applied.
 */
bool sys_clock_init(sys_clock_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* SYS_CLOCK_H */
