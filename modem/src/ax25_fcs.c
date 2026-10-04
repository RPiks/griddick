/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * ax25_fcs.c - This file is part of Griddick TNC.
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
 * @file ax25_fcs.c
 * @brief AX.25 FCS: CRC-16/X.25 (poly 0x1021 reflected).
 */
#include "ax25_internal.h"

#ifndef __not_in_flash_func
#define __not_in_flash_func(fn) fn
#endif

/** @brief CRC-16/X.25 reflected (poly 0x8408), init/xor 0xFFFF. */
uint16_t __not_in_flash_func(ax25_fcs_calc)(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    size_t i;
    int b;

    if (data == NULL) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (b = 0; b < 8; b++) {
            if (crc & 1u) {
                crc = (uint16_t)((crc >> 1) ^ 0x8408u); /* reflected 0x1021 */
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return (uint16_t)(crc ^ 0xFFFFu);
}
