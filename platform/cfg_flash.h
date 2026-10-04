#ifndef CFG_FLASH_H
#define CFG_FLASH_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * cfg_flash.h - This file is part of Griddick TNC.
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
 * @file cfg_flash.h
 * @brief Packed settings journal: 128 × 256-byte pages, CRC-32.
 *
 * Page v2 (little-endian):
 *   0   magic 'G','T','C','F'
 *   4   ver u8 (2)
 *   5   txdelay persist slottime txtail fulldup legacy log_events linkmode
 *   13  mycall[10]
 *   23  paclen u16
 *   25  maxframe u8
 *   26  n2 u8
 *   27  t1_ms u16
 *   29  t2_ms u16
 *   31  t3_ms u32
 *   35  seq u32
 *   39  bitfix_cnt u8 (0|1)
 *   40  bitfix_algo u8 (0)
 *   41  bitfix_sanity u8 (0 none, 1 ASCII)
 *   252 crc32 of bytes 0..251 (ISO-HDLC)
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_FLASH_PAGE_SIZE   256
#define CFG_FLASH_SECTOR_SIZE 4096
#define CFG_FLASH_PAGE_COUNT  128
#define CFG_FLASH_BYTES \
    (CFG_FLASH_PAGE_COUNT * CFG_FLASH_PAGE_SIZE)

#define CFG_FLASH_VER 2

typedef struct {
    uint8_t txdelay;
    uint8_t persist;
    uint8_t slottime;
    uint8_t txtail;
    uint8_t fulldup;
    uint8_t legacy;
    uint8_t log_events;
    uint8_t linkmode;
    char mycall[10];
    uint16_t paclen;
    uint8_t maxframe;
    uint16_t t1_ms;
    uint16_t t2_ms;
    uint32_t t3_ms;
    uint8_t n2;
    uint32_t seq;
    uint8_t bitfix_cnt;
    uint8_t bitfix_algo;
    uint8_t bitfix_sanity;
} cfg_flash_cfg_t;

uint32_t cfg_flash_crc32(const uint8_t *p, int n);

/* Fill 256-byte page. seq taken from @p c. Returns 0 or −1. */
int cfg_flash_pack(uint8_t page[CFG_FLASH_PAGE_SIZE],
                   const cfg_flash_cfg_t *c);

/*
 * Valid magic + ver==1 + CRC → copy out, return 0.
 * Bad page → −1.
 */
int cfg_flash_unpack(const uint8_t page[CFG_FLASH_PAGE_SIZE],
                     cfg_flash_cfg_t *c);

/*
 * Highest seq among valid pages.
 * Returns 1 if found, 0 if none, −1 if args bad.
 */
int cfg_flash_scan(const uint8_t *mem, unsigned bytes, cfg_flash_cfg_t *c);

/*
 * Next page index, seq to write, and sector to erase first (−1 = none).
 * Never names the sector that holds the current latest record.
 * Returns 0 or −1.
 */
int cfg_flash_plan(const uint8_t *mem, unsigned bytes,
                   unsigned *page_idx, uint32_t *seq, int *erase_sec);

/*
 * Append next page (seq = last+1, or 1 if empty).
 * Erases the target 4 KiB sector when the next page is not blank,
 * never the sector that holds the current latest record.
 * After program, unpacks the page and checks CRC.
 * Returns 0 or −1.
 */
int cfg_flash_append(uint8_t *mem, unsigned bytes, const cfg_flash_cfg_t *in);

#ifdef __cplusplus
}
#endif

#endif /* CFG_FLASH_H */
