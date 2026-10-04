/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * usb_descriptors.c - This file is part of Griddick TNC.
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

#include "pico/unique_id.h"
#include "pico/usb_reset.h"
#include "tusb.h"

#ifndef USBD_VID
#define USBD_VID 0x2E8A
#endif
#ifndef USBD_PID
#define USBD_PID 0x000A
#endif
#ifndef USBD_MANUFACTURER
#define USBD_MANUFACTURER "Raspberry Pi"
#endif
#ifndef USBD_PRODUCT
#define USBD_PRODUCT "Griddick KISS"
#endif

#define USBD_ITF_CDC       0
#define USBD_ITF_RPI_RESET 2
#define USBD_ITF_MAX       3
_Static_assert(USBD_ITF_RPI_RESET == PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF,
               "reset interface number must match MS OS 2.0 ITF");
#define USBD_CDC_EP_CMD    0x81
#define USBD_CDC_EP_OUT    0x02
#define USBD_CDC_EP_IN     0x82
#define USBD_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_RPI_RESET_DESC_LEN)

enum {
    USBD_STR_LANG = 0,
    USBD_STR_MANUF = 1,
    USBD_STR_PRODUCT = 2,
    USBD_STR_SERIAL = 3,
    USBD_STR_CDC = 4,
    USBD_STR_RPI_RESET = 5
};

static const tusb_desc_device_t usbd_desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
#if PICO_USB_RESET_SUPPORT_MS_OS_20_DESCRIPTOR
    .bcdUSB = 0x0210,
#else
    .bcdUSB = 0x0200,
#endif
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USBD_VID,
    .idProduct = USBD_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = USBD_STR_MANUF,
    .iProduct = USBD_STR_PRODUCT,
    .iSerialNumber = USBD_STR_SERIAL,
    .bNumConfigurations = 1,
};

static const uint8_t usbd_desc_cfg[USBD_DESC_LEN] = {
    TUD_CONFIG_DESCRIPTOR(1, USBD_ITF_MAX, 0, USBD_DESC_LEN, 0, 250),
    TUD_CDC_DESCRIPTOR(USBD_ITF_CDC, USBD_STR_CDC, USBD_CDC_EP_CMD, 8,
                       USBD_CDC_EP_OUT, USBD_CDC_EP_IN, 64),
    TUD_RPI_RESET_DESCRIPTOR(USBD_ITF_RPI_RESET, USBD_STR_RPI_RESET),
};

static char usbd_serial_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

static const char *const usbd_desc_str[] = {
    [USBD_STR_MANUF] = USBD_MANUFACTURER,
    [USBD_STR_PRODUCT] = USBD_PRODUCT,
    [USBD_STR_SERIAL] = usbd_serial_str,
    [USBD_STR_CDC] = "KISS",
    [USBD_STR_RPI_RESET] = "Reset",
};

const uint8_t *tud_descriptor_device_cb(void)
{
    return (const uint8_t *)&usbd_desc_device;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return usbd_desc_cfg;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    static uint16_t desc_str[32];
    uint8_t len;
    const char *str;

    (void)langid;
    if (!usbd_serial_str[0]) {
        pico_get_unique_board_id_string(usbd_serial_str, sizeof(usbd_serial_str));
    }
    if (index == 0) {
        desc_str[1] = 0x0409;
        len = 1;
    } else {
        if (index >= (sizeof(usbd_desc_str) / sizeof(usbd_desc_str[0])) ||
            usbd_desc_str[index] == NULL) {
            return NULL;
        }
        str = usbd_desc_str[index];
        for (len = 0; len < 31 && str[len]; len++) {
            desc_str[1 + len] = (uint16_t)(uint8_t)str[len];
        }
    }
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
    return desc_str;
}
