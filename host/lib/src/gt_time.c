/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_time.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gt_time.h"
#include "gt_kiss.h"
#include "gt_util.h"

#include <stdlib.h>
#include <string.h>

int gt_time_parse_log(const char *line, uint64_t *utc)
{
    const char *p = strstr(line, "[Time]");
    const char *u;

    if (p == NULL) {
        return 0;
    }
    u = strstr(p, "utc=");
    if (u == NULL) {
        return 0;
    }
    *utc = strtoull(u + 4, NULL, 10);
    return 1;
}

static int match_utc(const char *line, void *ctx)
{
    return gt_time_parse_log(line, (uint64_t *)ctx);
}

int gt_time_wait_ack(int fd, int timeout_ms, uint64_t *utc,
                     const volatile sig_atomic_t *stop)
{
    return gt_kiss_wait_log(fd, timeout_ms, stop, match_utc, utc, NULL, 0);
}

int gt_time_sync(int fd, uint64_t *host_ms, uint64_t *tnc_ms,
                 const volatile sig_atomic_t *stop)
{
    uint64_t now = gt_utc_ms();
    uint8_t payload[9];
    uint64_t ack = 0;
    int i;

    payload[0] = KISS_HW_TIME;
    for (i = 0; i < 8; i++) {
        payload[1 + i] = (uint8_t)(now >> (8 * i));
    }
    if (gt_kiss_write(fd, KISS_CMD_SETHW, payload, 9) != 0) {
        return -1;
    }
    (void)gt_kiss_drain(fd);
    if (gt_time_wait_ack(fd, 2000, &ack, stop) != 1) {
        return 0;
    }
    if (host_ms != NULL) {
        *host_ms = now;
    }
    if (tnc_ms != NULL) {
        *tnc_ms = ack;
    }
    return 1;
}

int gt_time_get(int fd, uint64_t *host_ms, uint64_t *tnc_ms,
                const volatile sig_atomic_t *stop)
{
    uint8_t payload[1];
    uint64_t tnc;

    payload[0] = KISS_HW_TIME;
    if (gt_kiss_write(fd, KISS_CMD_SETHW, payload, 1) != 0) {
        return -1;
    }
    if (gt_time_wait_ack(fd, 2000, &tnc, stop) != 1) {
        return 0;
    }
    if (tnc_ms != NULL) {
        *tnc_ms = tnc;
    }
    if (host_ms != NULL) {
        *host_ms = gt_utc_ms();
    }
    return 1;
}
