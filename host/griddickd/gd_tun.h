#ifndef GD_TUN_H
#define GD_TUN_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gd_tun.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int gd_tun_open(const char *name, uint32_t addr, uint32_t mask, int mtu);
void gd_tun_close(int fd);

#ifdef __cplusplus
}
#endif

#endif
