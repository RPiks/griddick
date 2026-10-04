#ifndef GT_LINK_H
#define GT_LINK_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_link.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gt_ax25.h"
#include "kiss.h"

#include <signal.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GT_LINK_K_MAX 7

enum gt_link_state {
    GT_LINK_IDLE = 0,
    GT_LINK_SABM,
    GT_LINK_CONNECTED,
    GT_LINK_DISC,
    GT_LINK_DEAD
};

enum gt_link_ev_type {
    GT_LINK_EV_NONE = 0,
    GT_LINK_EV_UP,
    GT_LINK_EV_DOWN,
    GT_LINK_EV_INFO,
    GT_LINK_EV_UI
};

struct gt_link_opts {
    struct gt_ax25_addr src;
    struct gt_ax25_addr dst;
    struct gt_ax25_addr digi[GT_AX25_DIGI_MAX];
    int n_digi;
    int t1_ms;
    int t2_ms; /* RR/REJ hold-off after I; 0 = send at once */
    int t3_ms;
    int n2;
    int paclen;
    int maxframe;
    int accept_any; /* IDLE: any peer SABM to src; then lock dst */
    int txdelay; /* KISS 10 ms ticks; PTT lead flags */
    int txtail;  /* KISS 10 ms ticks; PTT trail flags */
};

struct gt_link_ev {
    int type;
    uint8_t info[GT_AX25_INFO_MAX];
    int info_len;
};

struct gt_link_slot {
    uint8_t info[GT_AX25_INFO_MAX];
    int len;
    uint8_t pid;
    int ns;
};

struct gt_link {
    int fd;
    struct gt_link_opts o;
    int state;
    int vs;
    int va;
    int vr;
    int tries;
    int64_t t1_dl;
    int64_t t2_dl;
    int64_t t3_dl;
    int t1_on;
    int t2_on;
    int t2_rej;
    int t2_pf;
    int io_err;
    int up_seen;
    int64_t ptt_until; /* CLOCK_MONOTONIC ms; next TX after this */
    struct gt_link_slot out[GT_LINK_K_MAX];
    int n_out;
    kiss_rx_t rx;
};

void gt_link_init(struct gt_link *L, int fd, const struct gt_link_opts *o);
int gt_link_start(struct gt_link *L);
int gt_link_queue(struct gt_link *L, uint8_t pid,
                  const uint8_t *info, int info_len);
/* Drive the machine without reading fd. GUI owns KISS RX. */
int gt_link_feed(struct gt_link *L, const uint8_t *ax25, int n,
                 struct gt_link_ev *ev);
int gt_link_tick(struct gt_link *L, struct gt_link_ev *ev);
int gt_link_poll(struct gt_link *L, int timeout_ms,
                 const volatile sig_atomic_t *stop, struct gt_link_ev *ev);
int gt_link_disconnect(struct gt_link *L);
int gt_link_state(const struct gt_link *L);

int gt_link_send(int fd, const struct gt_link_opts *o,
                 uint8_t pid, const uint8_t *info, int info_len,
                 const volatile sig_atomic_t *stop);

#ifdef __cplusplus
}
#endif

#endif
