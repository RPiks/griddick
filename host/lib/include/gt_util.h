#ifndef GT_UTIL_H
#define GT_UTIL_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_util.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t gt_utc_ms(void);
void gt_stamp(char *out, size_t out_sz);

int gt_parse_hex_byte(const char *s, uint8_t *out);
int gt_parse_hex_blob(const char *s, uint8_t *out, int max);

/* Returns byte count, -1 I/O, -2 too large. path "-" = stdin. */
int gt_load_file(const char *path, uint8_t *out, int max);
int gt_load_text(const char *path, char *out, int max);

#ifdef __cplusplus
}
#endif

#endif
