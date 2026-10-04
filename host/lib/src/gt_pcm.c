/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_pcm.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gt_pcm.h"
#include "gt_kiss.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static void put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)((v >> 8) & 0xffu);
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)((v >> 8) & 0xffu);
    p[2] = (uint8_t)((v >> 16) & 0xffu);
    p[3] = (uint8_t)((v >> 24) & 0xffu);
}

static uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

void gt_wav_hdr_48k_mono(uint8_t *h, uint32_t n_samp)
{
    uint32_t data = n_samp * 2u;

    memcpy(h, "RIFF", 4);
    put_le32(h + 4, 36u + data);
    memcpy(h + 8, "WAVEfmt ", 8);
    put_le32(h + 16, 16);
    put_le16(h + 20, 1);
    put_le16(h + 22, 1);
    put_le32(h + 24, GT_PCM_RATE_HZ);
    put_le32(h + 28, GT_PCM_RATE_HZ * 2u);
    put_le16(h + 32, 2);
    put_le16(h + 34, 16);
    memcpy(h + 36, "data", 4);
    put_le32(h + 40, data);
}

int gt_wav_open_48k_mono(const char *path, int *out_fd, uint32_t *n_samp)
{
    int fd;
    uint8_t hdr[12];
    uint32_t data_bytes = 0;
    int got_fmt = 0;
    int got_data = 0;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    if (read(fd, hdr, 12) != 12 || memcmp(hdr, "RIFF", 4) != 0 ||
        memcmp(hdr + 8, "WAVE", 4) != 0) {
        close(fd);
        return -1;
    }
    while (!got_data) {
        uint8_t ch[8];
        uint32_t sz;
        off_t skip;

        if (read(fd, ch, 8) != 8) {
            close(fd);
            return -1;
        }
        sz = rd_le32(ch + 4);
        if (memcmp(ch, "fmt ", 4) == 0) {
            uint8_t fmt[16];

            if (sz < 16 || read(fd, fmt, 16) != 16) {
                close(fd);
                return -1;
            }
            if (rd_le16(fmt) != 1 || rd_le16(fmt + 2) != 1 ||
                rd_le32(fmt + 4) != GT_PCM_RATE_HZ || rd_le16(fmt + 14) != 16) {
                close(fd);
                return -1;
            }
            skip = (off_t)sz - 16;
            if (skip > 0 && lseek(fd, skip, SEEK_CUR) < 0) {
                close(fd);
                return -1;
            }
            got_fmt = 1;
        } else if (memcmp(ch, "data", 4) == 0) {
            if (!got_fmt) {
                close(fd);
                return -1;
            }
            data_bytes = sz;
            got_data = 1;
        } else {
            if (lseek(fd, (off_t)sz, SEEK_CUR) < 0) {
                close(fd);
                return -1;
            }
        }
        if ((sz & 1u) && !got_data) {
            if (lseek(fd, 1, SEEK_CUR) < 0) {
                close(fd);
                return -1;
            }
        }
    }
    if ((data_bytes & 1u) != 0) {
        close(fd);
        return -1;
    }
    *out_fd = fd;
    *n_samp = data_bytes / 2u;
    return 0;
}

int gt_pcm_pack(uint8_t *pay, int max, uint8_t seq, const int16_t *s, int ns)
{
    uint16_t crc;
    int i;
    int n = 1 + ns * 2 + 2;

    if (ns < 0 || n > max) {
        return -1;
    }
    pay[0] = seq;
    for (i = 0; i < ns; i++) {
        pay[1 + i * 2] = (uint8_t)((uint16_t)s[i] & 0xffu);
        pay[2 + i * 2] = (uint8_t)(((uint16_t)s[i] >> 8) & 0xffu);
    }
    crc = kiss_crc16(pay, 1 + ns * 2);
    pay[1 + ns * 2] = (uint8_t)(crc & 0xffu);
    pay[2 + ns * 2] = (uint8_t)((crc >> 8) & 0xffu);
    return n;
}

int gt_pcm_write(int fd, uint8_t seq, const int16_t *s, int ns)
{
    uint8_t pay[1 + KISS_PCM_CHUNK * 2 + 2];
    int n;

    n = gt_pcm_pack(pay, (int)sizeof(pay), seq, s, ns);
    if (n < 0) {
        return -1;
    }
    return gt_kiss_write(fd, KISS_CMD_PCM, pay, n);
}

int gt_pcm_parse(const uint8_t *frame, int len,
                 uint8_t *seq, const uint8_t **samples, int *n_samp,
                 int *crc_ok)
{
    int n_data;
    int body;
    uint16_t got;
    uint16_t expect;

    if (len < 4 || (frame[0] & KISS_CMD_MASK) != KISS_CMD_PCM) {
        return 0;
    }
    n_data = len - 1;
    if (n_data < 3) {
        return 0;
    }
    body = n_data - 2;
    if ((body & 1) != 1 || body < 1) {
        return 0;
    }
    *seq = frame[1];
    expect = kiss_crc16(frame + 1, body);
    got = (uint16_t)frame[1 + body] | ((uint16_t)frame[2 + body] << 8);
    *crc_ok = (got == expect);
    *samples = frame + 2;
    *n_samp = (body - 1) / 2;
    return 1;
}
