#ifndef GT_AX25_H
#define GT_AX25_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_ax25.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GT_AX25_INFO_MAX  256
#define GT_AX25_DIGI_MAX  8
#define GT_AX25_ADDR_LEN  7

struct gt_ax25_addr {
    char call[7];
    int ssid;
};

int gt_ax25_parse_call(const char *in, struct gt_ax25_addr *out);
int gt_ax25_parse_path(const char *in, struct gt_ax25_addr *out, int *n_out);
void gt_ax25_fmt_call(char *out, size_t out_sz, const struct gt_ax25_addr *a);
void gt_ax25_decode_call(const uint8_t *a, char *out, size_t out_sz);

int gt_ax25_addr_eq(const struct gt_ax25_addr *a, const struct gt_ax25_addr *b);
void gt_ax25_load_addr(const uint8_t *raw, struct gt_ax25_addr *out);
/* dest C-bit: 1 = command, 0 = response. n < 7 → 0. */
int gt_ax25_is_cmd(const uint8_t *pay, int n);

int gt_ax25_build(uint8_t *out, int max,
                  const struct gt_ax25_addr *dst,
                  const struct gt_ax25_addr *src,
                  const struct gt_ax25_addr *digi, int n_digi,
                  uint8_t ctrl, uint8_t pid,
                  const uint8_t *info, int info_len);

/* command: dest C=1 src C=0. response: dest C=0 src C=1. with_pid: I/UI only. */
int gt_ax25_build_ex(uint8_t *out, int max,
                     const struct gt_ax25_addr *dst,
                     const struct gt_ax25_addr *src,
                     const struct gt_ax25_addr *digi, int n_digi,
                     uint8_t ctrl, uint8_t pid,
                     const uint8_t *info, int info_len,
                     int command, int with_pid);

int gt_ax25_parse_frame(const uint8_t *pay, int n,
                        struct gt_ax25_addr *dst,
                        struct gt_ax25_addr *src,
                        uint8_t *ctrl, uint8_t *pid, int *has_pid,
                        const uint8_t **info, int *info_len);

/*
 * Format one KISS DATA payload (Address..Info) as a dump line.
 * ts may be NULL to omit the leading timestamp and its trailing space.
 */
int gt_ax25_format_data(const uint8_t *pay, int n, int verbose,
                        const char *ts, char *line, int max);

#ifdef __cplusplus
}
#endif

#endif
