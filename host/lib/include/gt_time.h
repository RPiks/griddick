#ifndef GT_TIME_H
#define GT_TIME_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_time.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include <signal.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int gt_time_parse_log(const char *line, uint64_t *utc);

/* 1 = ack, 0 = timeout/stop, -1 = I/O. */
int gt_time_wait_ack(int fd, int timeout_ms, uint64_t *utc,
                     const volatile sig_atomic_t *stop);

/* Writes SETHW TIME. sync uses host clock; get queries TNC. */
int gt_time_sync(int fd, uint64_t *host_ms, uint64_t *tnc_ms,
                 const volatile sig_atomic_t *stop);
int gt_time_get(int fd, uint64_t *host_ms, uint64_t *tnc_ms,
                const volatile sig_atomic_t *stop);

#ifdef __cplusplus
}
#endif

#endif
