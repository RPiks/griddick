#ifndef GT_KISS_H
#define GT_KISS_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_kiss.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "kiss.h"

#include <signal.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int gt_kiss_write(int fd, uint8_t cmd, const uint8_t *data, int n_data);
int gt_kiss_drain(int fd);

/*
 * Read wire bytes until one KISS frame is complete.
 * timeout_ms < 0 waits forever, 0 polls without blocking.
 * stop may be NULL.
 * Returns 1 if rx->buf[0 .. frame_len-1] holds a frame,
 * 0 on timeout or *stop, -1 on I/O error.
 */
int gt_kiss_read_frame(int fd, kiss_rx_t *rx, int timeout_ms,
                       const volatile sig_atomic_t *stop);

/*
 * Feed already-read bytes into rx. Returns 1 when a frame completes;
 * *consumed is set to bytes used (including the closing FEND).
 */
int gt_kiss_feed(kiss_rx_t *rx, const uint8_t *buf, int n, int *consumed);

/* 1 = LOG line written to out (NUL-terminated), 0 = not a LOG frame. */
int gt_kiss_log_line(const kiss_rx_t *rx, char *out, int max);

typedef int (*gt_kiss_log_match_fn)(const char *line, void *ctx);

/* 1 = match, 0 = timeout/stop, -1 = I/O. */
int gt_kiss_wait_log(int fd, int timeout_ms, const volatile sig_atomic_t *stop,
                     gt_kiss_log_match_fn match, void *ctx,
                     char *line, int max);

/* 1 = [Tx] done, 0 = drop / timeout / stop, -1 = I/O. */
int gt_kiss_wait_tx(int fd, int timeout_ms, const volatile sig_atomic_t *stop);

#ifdef __cplusplus
}
#endif

#endif
