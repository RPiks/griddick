#ifndef GD_CONF_H
#define GD_CONF_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gd_conf.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gt_ax25.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GD_CONF_DEFAULT  "/opt/griddick/griddickd.conf"
#define GD_PEER_MAX      64
#define GD_PASS_MAX      64
#define GD_PATH_MAX      128
#define GD_WAIT_DEFAULT  30

struct gd_peer {
    struct gt_ax25_addr call;
    uint32_t ip; /* network byte order */
    char password[GD_PASS_MAX + 1];
};

struct gd_conf {
    char device[GD_PATH_MAX];
    char tun_name[16];
    struct gt_ax25_addr local_call;
    uint32_t tun_ip;
    uint32_t tun_mask;
    int tun_prefix;
    int device_wait;
    int n_peer;
    struct gd_peer peer[GD_PEER_MAX];
};

int gd_conf_load(const char *path, struct gd_conf *c, char *err, int err_max);

struct gd_peer *gd_conf_peer_by_ip(struct gd_conf *c, uint32_t ip);
struct gd_peer *gd_conf_peer_by_call(struct gd_conf *c,
                                     const struct gt_ax25_addr *call);

#ifdef __cplusplus
}
#endif

#endif
