#ifndef GT_APRS_H
#define GT_APRS_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_aprs.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GT_APRS_DEST_DEFAULT "APZ001"
#define GT_APRS_PATH_DEFAULT "WIDE1-1,WIDE2-1"

/* -2 = bad symbol, -1/-3 = other. */
int gt_aprs_build_pos(uint8_t *out, int max, const char *ll,
                      const char *sym, const char *comment);
int gt_aprs_build_status(uint8_t *out, int max, const char *text);
int gt_aprs_build_msg(uint8_t *out, int max, const char *to, const char *text);
/* 0 = not APRS info. >0 = bytes written. */
int gt_aprs_format_rx(const uint8_t *info, int n, char *out, int max);

#ifdef __cplusplus
}
#endif

#endif
