/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_kiss.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#define _POSIX_C_SOURCE 200809L

#include "gt_kiss.h"

#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

int gt_kiss_write(int fd, uint8_t cmd, const uint8_t *data, int n_data)
{
    uint8_t wire[KISS_MAX_WIRE];
    int n;

    n = kiss_encode(cmd, data, n_data, wire, (int)sizeof(wire));
    if (n < 0) {
        return -1;
    }
    if (write(fd, wire, (size_t)n) != n) {
        return -1;
    }
    return 0;
}

int gt_kiss_drain(int fd)
{
    return tcdrain(fd);
}

int gt_kiss_feed(kiss_rx_t *rx, const uint8_t *buf, int n, int *consumed)
{
    int i;

    if (consumed != NULL) {
        *consumed = 0;
    }
    for (i = 0; i < n; i++) {
        if (kiss_rx_byte(rx, buf[i])) {
            if (consumed != NULL) {
                *consumed = i + 1;
            }
            return 1;
        }
    }
    if (consumed != NULL) {
        *consumed = n;
    }
    return 0;
}

int gt_kiss_log_line(const kiss_rx_t *rx, char *out, int max)
{
    int n;

    if (rx == NULL || rx->frame_len < 2) {
        return 0;
    }
    if ((rx->buf[0] & KISS_CMD_MASK) != KISS_CMD_LOG) {
        return 0;
    }
    if (out == NULL || max < 2) {
        return 0;
    }
    n = rx->frame_len - 1;
    if (n >= max) {
        n = max - 1;
    }
    memcpy(out, rx->buf + 1, (size_t)n);
    out[n] = '\0';
    return 1;
}

int gt_kiss_read_frame(int fd, kiss_rx_t *rx, int timeout_ms,
                       const volatile sig_atomic_t *stop)
{
    int left = timeout_ms;
    int forever = (timeout_ms < 0);

    /* timeout 0 = non-blocking drain. The old `while (left > 0)` skipped
     * the read, so gtcall (select then poll 0) never ingested SABM. */
    for (;;) {
        fd_set rfds;
        struct timeval tv;
        unsigned char buf[4096];
        ssize_t n;
        int sl;
        int slice;

        if (stop != NULL && *stop) {
            return 0;
        }
        if (forever) {
            slice = 100;
        } else if (left > 100) {
            slice = 100;
        } else if (left > 0) {
            slice = left;
        } else {
            slice = 0;
        }
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = slice * 1000;
        sl = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (sl < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (sl == 0) {
            if (forever) {
                continue;
            }
            if (left <= 0) {
                return 0;
            }
            left -= slice;
            if (left <= 0) {
                return 0;
            }
            continue;
        }
        n = read(fd, buf, 1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        if (kiss_rx_byte(rx, buf[0])) {
            return 1;
        }
    }
    return 0;
}

int gt_kiss_wait_log(int fd, int timeout_ms, const volatile sig_atomic_t *stop,
                     gt_kiss_log_match_fn match, void *ctx,
                     char *line, int max)
{
    kiss_rx_t rx;
    int left = timeout_ms;
    char local[KISS_MAX_FRAME];
    char *dst = (line != NULL) ? line : local;
    int dst_max = (line != NULL) ? max : (int)sizeof(local);

    kiss_rx_init(&rx);
    while (left > 0) {
        int slice = (left > 100) ? 100 : left;
        int r;

        if (stop != NULL && *stop) {
            return 0;
        }
        r = gt_kiss_read_frame(fd, &rx, slice, stop);
        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            left -= slice;
            continue;
        }
        if (gt_kiss_log_line(&rx, dst, dst_max) &&
            match != NULL && match(dst, ctx)) {
            return 1;
        }
    }
    return 0;
}

static int match_tx(const char *line, void *ctx)
{
    int *kind = (int *)ctx;

    if (strstr(line, "[Tx]") != NULL && strstr(line, "done") != NULL) {
        *kind = 1;
        return 1;
    }
    if (strstr(line, "[Kiss]") != NULL && strstr(line, "drop") != NULL) {
        *kind = 0;
        return 1;
    }
    return 0;
}

int gt_kiss_wait_tx(int fd, int timeout_ms, const volatile sig_atomic_t *stop)
{
    int kind = 0;
    int r;

    r = gt_kiss_wait_log(fd, timeout_ms, stop, match_tx, &kind, NULL, 0);
    if (r < 0) {
        return -1;
    }
    if (r == 0) {
        return 0;
    }
    return kind;
}
