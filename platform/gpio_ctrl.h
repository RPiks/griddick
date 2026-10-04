#ifndef GPIO_CTRL_H
#define GPIO_CTRL_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * gpio_ctrl.h - This file is part of Griddick TNC.
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
 * @file gpio_ctrl.h
 * @brief Digital GPIO configure / read / write helpers.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPIO_CTRL_MODE_OUT        0u
#define GPIO_CTRL_MODE_IN         1u
#define GPIO_CTRL_MODE_IN_PULLUP  2u
#define GPIO_CTRL_MODE_IN_PULLDOWN 3u

/**
 * @brief Configure a pin as digital OUT or IN (with optional pull).
 * @param pin GPIO number 0..29.
 * @param mode GPIO_CTRL_MODE_*.
 * @return true on success.
 */
bool gpio_ctrl_set_mode(uint8_t pin, uint8_t mode);

/**
 * @brief Drive a pin high or low.
 * @param pin GPIO number 0..29.
 * @param value true = high.
 * @return true on success.
 */
bool gpio_ctrl_write(uint8_t pin, bool value);

/**
 * @brief Sample the current pin level.
 * @param pin GPIO number 0..29.
 * @param out Receives level.
 * @return true on success.
 */
bool gpio_ctrl_read(uint8_t pin, bool *out);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_CTRL_H */
