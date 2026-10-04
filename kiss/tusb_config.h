#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * tusb_config.h - This file is part of Griddick TNC.
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

#define CFG_TUD_ENABLED       1
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_CDC     1
#define CFG_TUD_MSC     0
#define CFG_TUD_HID     0
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

#define CFG_TUD_CDC_RX_BUFSIZE 4096
#define CFG_TUD_CDC_TX_BUFSIZE 4096
#define CFG_TUD_CDC_EP_BUFSIZE 64

#endif /* _TUSB_CONFIG_H_ */
