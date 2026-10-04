/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * cfg_flash.c - This file is part of Griddick TNC.
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

#include "cfg_flash.h"

#include <string.h>

#define MAGIC0 'G'
#define MAGIC1 'T'
#define MAGIC2 'C'
#define MAGIC3 'F'
#define CRC_OFF 252

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint32_t cfg_flash_crc32(const uint8_t *p, int n)
{
    uint32_t crc = 0xFFFFFFFFu;
    int i;
    int b;

    if (p == NULL || n <= 0) {
        return crc ^ 0xFFFFFFFFu;
    }
    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (b = 0; b < 8; b++) {
            if (crc & 1u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

int cfg_flash_pack(uint8_t page[CFG_FLASH_PAGE_SIZE], const cfg_flash_cfg_t *c)
{
    uint32_t crc;

    if (page == NULL || c == NULL) {
        return -1;
    }
    memset(page, 0, CFG_FLASH_PAGE_SIZE);
    page[0] = MAGIC0;
    page[1] = MAGIC1;
    page[2] = MAGIC2;
    page[3] = MAGIC3;
    page[4] = CFG_FLASH_VER;
    page[5] = c->txdelay;
    page[6] = c->persist;
    page[7] = c->slottime;
    page[8] = c->txtail;
    page[9] = c->fulldup;
    page[10] = c->legacy;
    page[11] = c->log_events;
    page[12] = c->linkmode;
    memcpy(page + 13, c->mycall, 10);
    wr16(page + 23, c->paclen);
    page[25] = c->maxframe;
    page[26] = c->n2;
    wr16(page + 27, c->t1_ms);
    wr16(page + 29, c->t2_ms);
    wr32(page + 31, c->t3_ms);
    wr32(page + 35, c->seq);
    page[39] = c->bitfix_cnt;
    page[40] = c->bitfix_algo;
    page[41] = c->bitfix_sanity;
    crc = cfg_flash_crc32(page, CRC_OFF);
    wr32(page + CRC_OFF, crc);
    return 0;
}

int cfg_flash_unpack(const uint8_t page[CFG_FLASH_PAGE_SIZE],
                     cfg_flash_cfg_t *c)
{
    uint32_t crc;

    if (page == NULL || c == NULL) {
        return -1;
    }
    if (page[0] != MAGIC0 || page[1] != MAGIC1 || page[2] != MAGIC2 ||
        page[3] != MAGIC3) {
        return -1;
    }
    if (page[4] != CFG_FLASH_VER) {
        return -1;
    }
    crc = rd32(page + CRC_OFF);
    if (crc != cfg_flash_crc32(page, CRC_OFF)) {
        return -1;
    }
    memset(c, 0, sizeof(*c));
    c->txdelay = page[5];
    c->persist = page[6];
    c->slottime = page[7];
    c->txtail = page[8];
    c->fulldup = page[9];
    c->legacy = page[10];
    c->log_events = page[11];
    c->linkmode = page[12];
    memcpy(c->mycall, page + 13, 10);
    c->paclen = rd16(page + 23);
    c->maxframe = page[25];
    c->n2 = page[26];
    c->t1_ms = rd16(page + 27);
    c->t2_ms = rd16(page + 29);
    c->t3_ms = rd32(page + 31);
    c->seq = rd32(page + 35);
    c->bitfix_cnt = page[39];
    c->bitfix_algo = page[40];
    c->bitfix_sanity = page[41];
    return 0;
}

static int page_blank(const uint8_t *p)
{
    int i;

    for (i = 0; i < CFG_FLASH_PAGE_SIZE; i++) {
        if (p[i] != 0xFFu) {
            return 0;
        }
    }
    return 1;
}

static int scan_idx(const uint8_t *mem, unsigned bytes, cfg_flash_cfg_t *c,
                    int *idx)
{
    unsigned np;
    unsigned i;
    int found = 0;
    cfg_flash_cfg_t best;
    cfg_flash_cfg_t cur;
    int best_i = -1;

    if (mem == NULL || bytes != CFG_FLASH_BYTES) {
        return -1;
    }
    np = CFG_FLASH_PAGE_COUNT;
    memset(&best, 0, sizeof(best));
    for (i = 0; i < np; i++) {
        if (cfg_flash_unpack(mem + i * CFG_FLASH_PAGE_SIZE, &cur) != 0) {
            continue;
        }
        if (!found || cur.seq > best.seq) {
            best = cur;
            best_i = (int)i;
            found = 1;
        }
    }
    if (!found) {
        return 0;
    }
    if (c != NULL) {
        *c = best;
    }
    if (idx != NULL) {
        *idx = best_i;
    }
    return 1;
}

int cfg_flash_scan(const uint8_t *mem, unsigned bytes, cfg_flash_cfg_t *c)
{
    return scan_idx(mem, bytes, c, NULL);
}

int cfg_flash_plan(const uint8_t *mem, unsigned bytes,
                   unsigned *page_idx, uint32_t *seq, int *erase_sec)
{
    cfg_flash_cfg_t last;
    int last_i = -1;
    int have;
    unsigned next;
    unsigned next_sec;

    if (mem == NULL || bytes != CFG_FLASH_BYTES || page_idx == NULL ||
        seq == NULL || erase_sec == NULL) {
        return -1;
    }
    have = scan_idx(mem, bytes, &last, &last_i);
    if (have < 0) {
        return -1;
    }
    if (have == 0) {
        next = 0;
        *seq = 1;
    } else {
        next = (unsigned)((last_i + 1) % CFG_FLASH_PAGE_COUNT);
        *seq = last.seq + 1u;
        if (*seq == 0u) {
            *seq = 1u;
        }
    }
    *page_idx = next;
    next_sec = (next * CFG_FLASH_PAGE_SIZE) / CFG_FLASH_SECTOR_SIZE;
    if (page_blank(mem + next * CFG_FLASH_PAGE_SIZE)) {
        *erase_sec = -1;
        return 0;
    }
    if (have) {
        unsigned last_sec;

        last_sec = ((unsigned)last_i * CFG_FLASH_PAGE_SIZE) /
                   CFG_FLASH_SECTOR_SIZE;
        if (next_sec == last_sec) {
            return -1;
        }
    }
    *erase_sec = (int)next_sec;
    return 0;
}

static int rec_same(const cfg_flash_cfg_t *a, const cfg_flash_cfg_t *b)
{
    return a->seq == b->seq && a->txdelay == b->txdelay &&
           a->persist == b->persist && a->slottime == b->slottime &&
           a->txtail == b->txtail && a->fulldup == b->fulldup &&
           a->legacy == b->legacy && a->log_events == b->log_events &&
           a->linkmode == b->linkmode && a->paclen == b->paclen &&
           a->maxframe == b->maxframe && a->n2 == b->n2 &&
           a->t1_ms == b->t1_ms && a->t2_ms == b->t2_ms &&
           a->t3_ms == b->t3_ms && a->bitfix_cnt == b->bitfix_cnt &&
           a->bitfix_algo == b->bitfix_algo &&
           a->bitfix_sanity == b->bitfix_sanity &&
           memcmp(a->mycall, b->mycall, 10) == 0;
}

static void erase_sector(uint8_t *mem, unsigned sector)
{
    memset(mem + sector * CFG_FLASH_SECTOR_SIZE, 0xFF, CFG_FLASH_SECTOR_SIZE);
}

int cfg_flash_append(uint8_t *mem, unsigned bytes, const cfg_flash_cfg_t *in)
{
    cfg_flash_cfg_t rec;
    cfg_flash_cfg_t got;
    unsigned next;
    uint32_t seq;
    int erase_sec;
    uint8_t *page;

    if (mem == NULL || in == NULL || bytes != CFG_FLASH_BYTES) {
        return -1;
    }
    if (cfg_flash_plan(mem, bytes, &next, &seq, &erase_sec) != 0) {
        return -1;
    }
    if (erase_sec >= 0) {
        erase_sector(mem, (unsigned)erase_sec);
    }
    rec = *in;
    rec.seq = seq;
    page = mem + next * CFG_FLASH_PAGE_SIZE;
    if (cfg_flash_pack(page, &rec) != 0) {
        return -1;
    }
    if (cfg_flash_unpack(page, &got) != 0 || !rec_same(&got, &rec)) {
        return -1;
    }
    return 0;
}
