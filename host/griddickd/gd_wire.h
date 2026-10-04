#ifndef GD_WIRE_H
#define GD_WIRE_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gd_wire.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GD_WIRE_VER      0x01u
#define GD_WIRE_HDR_LEN  5
#define GD_WIRE_AUTH_LEN 2

void gd_wire_auth(const char *password, uint16_t sport, uint16_t dport,
                  const uint8_t *payload, int n, uint8_t out[2]);

/*
 * Pack ver|sport|dport|payload[|auth]. password NULL/empty → no trailer.
 * Returns Info length, or -1.
 */
int gd_wire_pack(uint8_t *out, int max,
                 uint16_t sport, uint16_t dport,
                 const uint8_t *payload, int n,
                 const char *password);

/*
 * Unpack. password NULL/empty → no trailer expected.
 * payload points into info.
 * Returns 0, -1 format, -2 auth mismatch.
 */
int gd_wire_unpack(const uint8_t *info, int info_len,
                   const char *password,
                   uint16_t *sport, uint16_t *dport,
                   const uint8_t **payload, int *n);

#ifdef __cplusplus
}
#endif

#endif
