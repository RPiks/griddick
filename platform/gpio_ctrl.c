/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * gpio_ctrl.c - This file is part of Griddick TNC.
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

#include "gpio_ctrl.h"

#include "hardware/gpio.h"

#include <stddef.h>

bool gpio_ctrl_set_mode(uint8_t pin, uint8_t mode)
{
    if (pin > 29u) {
        return false;
    }

    gpio_init(pin);

    switch (mode) {
    case GPIO_CTRL_MODE_OUT:
        gpio_set_dir(pin, GPIO_OUT);
        gpio_disable_pulls(pin);
        break;
    case GPIO_CTRL_MODE_IN:
        gpio_set_dir(pin, GPIO_IN);
        gpio_disable_pulls(pin);
        break;
    case GPIO_CTRL_MODE_IN_PULLUP:
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
        break;
    case GPIO_CTRL_MODE_IN_PULLDOWN:
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_down(pin);
        break;
    default:
        return false;
    }

    return true;
}

bool gpio_ctrl_write(uint8_t pin, bool value)
{
    if (pin > 29u) {
        return false;
    }
    gpio_put(pin, value);
    return true;
}

bool gpio_ctrl_read(uint8_t pin, bool *out)
{
    if (pin > 29u || out == NULL) {
        return false;
    }
    *out = gpio_get(pin);
    return true;
}
