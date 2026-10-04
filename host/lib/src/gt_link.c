/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_link.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#define _POSIX_C_SOURCE 200809L

#include "gt_link.h"
#include "gt_kiss.h"

#include <string.h>
#include <time.h>

#define GT_LINK_BAUD 1200

#define AX25_SABM    0x2Fu
#define AX25_SABM_P  0x3Fu
#define AX25_DISC    0x43u
#define AX25_DISC_P  0x53u
#define AX25_UA      0x63u
#define AX25_DM      0x0Fu
#define AX25_FRMR    0x87u

static uint8_t u_mask(uint8_t c)
{
    return (uint8_t)(c & 0xefu);
}

static int is_i(uint8_t c)
{
    return (c & 0x01u) == 0u;
}

static int is_s(uint8_t c)
{
    return (c & 0x03u) == 0x01u;
}

static int nr_of(uint8_t c)
{
    return (c >> 5) & 7;
}

static int ns_of(uint8_t c)
{
    return (c >> 1) & 7;
}

static int pf_of(uint8_t c)
{
    return (c & 0x10u) != 0;
}

static uint8_t ctrl_i(int ns, int nr, int pf)
{
    return (uint8_t)((nr << 5) | ((pf ? 1 : 0) << 4) | (ns << 1));
}

static uint8_t ctrl_rr(int nr, int pf)
{
    return (uint8_t)((nr << 5) | ((pf ? 1 : 0) << 4) | 0x01u);
}

static uint8_t ctrl_rej(int nr, int pf)
{
    return (uint8_t)((nr << 5) | ((pf ? 1 : 0) << 4) | 0x09u);
}

static int64_t now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int between(int a, int x, int b)
{
    x = (x - a) & 7;
    b = (b - a) & 7;
    return x <= b;
}

static int ours(const struct gt_link *L,
                const struct gt_ax25_addr *dst,
                const struct gt_ax25_addr *src)
{
    if (!gt_ax25_addr_eq(dst, &L->o.src)) {
        return 0;
    }
    if (L->o.accept_any && L->state == GT_LINK_IDLE) {
        return 1;
    }
    return gt_ax25_addr_eq(src, &L->o.dst);
}

static int k_win(const struct gt_link *L)
{
    int k = L->o.maxframe;

    if (k < 1) {
        k = 4;
    }
    if (k > GT_LINK_K_MAX) {
        k = GT_LINK_K_MAX;
    }
    return k;
}

static int t1_ms(const struct gt_link *L)
{
    return L->o.t1_ms > 0 ? L->o.t1_ms : 2000;
}

static int n2_max(const struct gt_link *L)
{
    return L->o.n2 > 0 ? L->o.n2 : 10;
}

static int64_t after_ptt(const struct gt_link *L)
{
    int64_t now = now_ms();

    return L->ptt_until > now ? L->ptt_until : now;
}

static void arm_t1(struct gt_link *L)
{
    L->t1_dl = after_ptt(L) + t1_ms(L);
    L->t1_on = 1;
}

static void arm_t3(struct gt_link *L)
{
    if (L->o.t3_ms > 0) {
        L->t3_dl = after_ptt(L) + L->o.t3_ms;
    }
}

/* Same flag count as kiss_tx_flag_count_10ms: ceil(n * 1.5), 1..128. */
static int flag_count_10ms(int n_10ms)
{
    int f;

    if (n_10ms < 0) {
        n_10ms = 0;
    }
    f = (n_10ms * 3 + 1) / 2;
    if (f < 1) {
        f = 1;
    }
    if (f > 128) {
        f = 128;
    }
    return f;
}

/*
 * Estimated PTT-up ms for one KISS DATA payload (Address..Info, no FCS).
 * Firmware: lead flags + stuffed (payload+FCS) + trail flags at 1200 baud.
 * Stuff slack is +1 bit per 5 data bits. +20 ms T/R margin.
 */
static int ptt_ms(const struct gt_link *L, int payload_n)
{
    int lead = flag_count_10ms(L->o.txdelay);
    int trail = flag_count_10ms(L->o.txtail);
    int body = payload_n + 2;
    int bits;

    if (body < 2) {
        body = 2;
    }
    bits = (lead + trail) * 8 + body * 8 + (body * 8) / 5;
    return bits * 1000 / GT_LINK_BAUD + 20;
}

static void wait_ptt(struct gt_link *L)
{
    for (;;) {
        int64_t now = now_ms();
        int left;
        struct timespec ts;

        if (now >= L->ptt_until) {
            return;
        }
        left = (int)(L->ptt_until - now);
        if (left > 50) {
            left = 50;
        }
        if (left < 1) {
            return;
        }
        ts.tv_sec = left / 1000;
        ts.tv_nsec = (long)(left % 1000) * 1000000L;
        (void)nanosleep(&ts, NULL);
    }
}

static void cancel_t2(struct gt_link *L)
{
    L->t2_on = 0;
}

static int send_frame(struct gt_link *L, uint8_t ctrl, int with_pid,
                      uint8_t pid, const uint8_t *info, int info_len,
                      int command);
static int on_frame(struct gt_link *L, const uint8_t *pay, int n,
                    struct gt_link_ev *ev);
static int on_timer(struct gt_link *L, struct gt_link_ev *ev);

static int send_rr_rej(struct gt_link *L, int rej, int pf, int command)
{
    uint8_t c = rej ? ctrl_rej(L->vr, pf) : ctrl_rr(L->vr, pf);

    return send_frame(L, c, 0, 0, NULL, 0, command);
}

/* Delay RR/REJ when t2_ms > 0. UA is not delayed. ACK/F-reply = response. */
static int ack_later(struct gt_link *L, int rej, int pf)
{
    if (L->o.t2_ms <= 0) {
        return send_rr_rej(L, rej, pf, 0);
    }
    L->t2_on = 1;
    L->t2_rej = rej ? 1 : 0;
    L->t2_pf = pf ? 1 : 0;
    L->t2_dl = after_ptt(L) + L->o.t2_ms;
    return 0;
}

static int flush_t2(struct gt_link *L)
{
    int rej;
    int pf;

    if (!L->t2_on) {
        return 0;
    }
    rej = L->t2_rej;
    pf = L->t2_pf;
    cancel_t2(L);
    return send_rr_rej(L, rej, pf, 0);
}

static int send_frame(struct gt_link *L, uint8_t ctrl, int with_pid,
                      uint8_t pid, const uint8_t *info, int info_len,
                      int command)
{
    uint8_t pay[2 * GT_AX25_ADDR_LEN + GT_AX25_DIGI_MAX * GT_AX25_ADDR_LEN +
                2 + GT_AX25_INFO_MAX];
    int n;

    n = gt_ax25_build_ex(pay, (int)sizeof(pay), &L->o.dst, &L->o.src, L->o.digi,
                         L->o.n_digi, ctrl, pid, info, info_len, command,
                         with_pid);
    if (n < 0) {
        return -1;
    }
    /* Pico has one TX slot; a second DATA while PTT is up is dropped. */
    wait_ptt(L);
    if (gt_kiss_write(L->fd, KISS_CMD_DATA, pay, n) != 0) {
        L->io_err = 1;
        return -1;
    }
    (void)gt_kiss_drain(L->fd);
    L->ptt_until = now_ms() + ptt_ms(L, n);
    return 0;
}

static int send_i(struct gt_link *L, const struct gt_link_slot *s, int pf)
{
    return send_frame(L, ctrl_i(s->ns, L->vr, pf), 1, s->pid, s->info, s->len,
                      1);
}

static void go_dead(struct gt_link *L)
{
    L->state = GT_LINK_DEAD;
    L->t1_on = 0;
    cancel_t2(L);
    L->n_out = 0;
}

static void go_up(struct gt_link *L)
{
    L->state = GT_LINK_CONNECTED;
    L->vs = 0;
    L->va = 0;
    L->vr = 0;
    L->tries = 0;
    L->t1_on = 0;
    cancel_t2(L);
    L->n_out = 0;
    L->up_seen = 1;
    arm_t3(L);
}

static int tx_window(struct gt_link *L)
{
    int i;
    int outstanding = (L->vs - L->va) & 7;

    for (i = 0; i < L->n_out && outstanding < k_win(L); i++) {
        if (L->out[i].ns >= 0) {
            continue;
        }
        L->out[i].ns = L->vs;
        cancel_t2(L);
        if (send_i(L, &L->out[i], 1) != 0) {
            return -1;
        }
        L->vs = (L->vs + 1) & 7;
        outstanding++;
        L->tries = 0;
        arm_t1(L);
        arm_t3(L);
    }
    return 0;
}

static void ack_nr(struct gt_link *L, int nr)
{
    int i;
    int w = 0;

    if (!between(L->va, nr, L->vs)) {
        return;
    }
    for (i = 0; i < L->n_out; i++) {
        if (L->out[i].ns >= 0 && between(L->va, (L->out[i].ns + 1) & 7, nr)) {
            continue;
        }
        L->out[w++] = L->out[i];
    }
    L->n_out = w;
    L->va = nr;
    if (L->va == L->vs) {
        L->t1_on = 0;
        L->tries = 0;
    }
}

static int rexmit(struct gt_link *L)
{
    int i;

    for (i = 0; i < L->n_out; i++) {
        if (L->out[i].ns == L->va) {
            cancel_t2(L);
            if (send_i(L, &L->out[i], 1) != 0) {
                return -1;
            }
            arm_t1(L);
            return 0;
        }
    }
    return 0;
}

void gt_link_init(struct gt_link *L, int fd, const struct gt_link_opts *o)
{
    memset(L, 0, sizeof(*L));
    L->fd = fd;
    L->o = *o;
    if (L->o.paclen <= 0 || L->o.paclen > GT_AX25_INFO_MAX) {
        L->o.paclen = GT_AX25_INFO_MAX;
    }
    kiss_rx_init(&L->rx);
    L->state = GT_LINK_IDLE;
}

int gt_link_start(struct gt_link *L)
{
    L->tries = 0;
    L->state = GT_LINK_SABM;
    if (send_frame(L, AX25_SABM_P, 0, 0, NULL, 0, 1) != 0) {
        return -1;
    }
    L->tries = 1;
    arm_t1(L);
    return 0;
}

int gt_link_queue(struct gt_link *L, uint8_t pid,
                  const uint8_t *info, int info_len)
{
    struct gt_link_slot *s;

    if (L->state != GT_LINK_CONNECTED) {
        return 0;
    }
    if (info_len < 0 || info_len > L->o.paclen) {
        return -2;
    }
    if (L->n_out >= GT_LINK_K_MAX) {
        return 0;
    }
    s = &L->out[L->n_out++];
    s->len = info_len;
    s->pid = pid;
    s->ns = -1;
    if (info_len > 0 && info != NULL) {
        memcpy(s->info, info, (size_t)info_len);
    }
    return tx_window(L) == 0 ? 1 : -1;
}

int gt_link_disconnect(struct gt_link *L)
{
    if (L->state == GT_LINK_DEAD || L->state == GT_LINK_IDLE) {
        return 0;
    }
    L->state = GT_LINK_DISC;
    L->tries = 0;
    cancel_t2(L);
    if (send_frame(L, AX25_DISC_P, 0, 0, NULL, 0, 1) != 0) {
        return -1;
    }
    L->tries = 1;
    arm_t1(L);
    return 0;
}

int gt_link_state(const struct gt_link *L)
{
    return L->state;
}

int gt_link_feed(struct gt_link *L, const uint8_t *ax25, int n,
                 struct gt_link_ev *ev)
{
    if (ev != NULL) {
        ev->type = GT_LINK_EV_NONE;
        ev->info_len = 0;
    }
    if (L->io_err) {
        return -1;
    }
    if (ax25 == NULL || n < 14) {
        return 0;
    }
    return on_frame(L, ax25, n, ev);
}

int gt_link_tick(struct gt_link *L, struct gt_link_ev *ev)
{
    if (ev != NULL) {
        ev->type = GT_LINK_EV_NONE;
        ev->info_len = 0;
    }
    if (L->io_err) {
        return -1;
    }
    return on_timer(L, ev);
}

static int on_frame(struct gt_link *L, const uint8_t *pay, int n,
                    struct gt_link_ev *ev)
{
    struct gt_ax25_addr dst;
    struct gt_ax25_addr src;
    uint8_t ctrl;
    uint8_t pid;
    int has_pid;
    const uint8_t *ibody;
    int ilen;
    uint8_t um;

    if (gt_ax25_parse_frame(pay, n, &dst, &src, &ctrl, &pid, &has_pid,
                            &ibody, &ilen) != 0) {
        return 0;
    }
    if ((ctrl & 0xefu) == 0x03u) {
        if (ev != NULL && ilen > 0) {
            ev->type = GT_LINK_EV_UI;
            ev->info_len = ilen < GT_AX25_INFO_MAX ? ilen : GT_AX25_INFO_MAX;
            memcpy(ev->info, ibody, (size_t)ev->info_len);
        }
        return ev != NULL && ev->type == GT_LINK_EV_UI;
    }
    if (!ours(L, &dst, &src)) {
        return 0;
    }
    um = u_mask(ctrl);
    if (um == AX25_FRMR) {
        if (L->state == GT_LINK_IDLE) {
            return 0;
        }
        go_dead(L);
        if (ev != NULL) {
            ev->type = GT_LINK_EV_DOWN;
        }
        return ev != NULL;
    }
    if (um == AX25_SABM) {
        if (L->state == GT_LINK_SABM || L->state == GT_LINK_IDLE ||
            L->state == GT_LINK_CONNECTED) {
            if (L->o.accept_any && L->state == GT_LINK_IDLE) {
                L->o.dst = src;
            }
            if (send_frame(L, AX25_UA | 0x10u, 0, 0, NULL, 0, 0) != 0) {
                return -1;
            }
            if (L->state != GT_LINK_CONNECTED) {
                go_up(L);
                if (ev != NULL) {
                    ev->type = GT_LINK_EV_UP;
                }
                return ev != NULL;
            }
            arm_t3(L);
        } else {
            (void)send_frame(L, AX25_DM | 0x10u, 0, 0, NULL, 0, 0);
        }
        return 0;
    }
    if (um == AX25_UA) {
        if (L->state == GT_LINK_SABM) {
            go_up(L);
            if (ev != NULL) {
                ev->type = GT_LINK_EV_UP;
            }
            return ev != NULL;
        }
        if (L->state == GT_LINK_DISC) {
            go_dead(L);
            if (ev != NULL) {
                ev->type = GT_LINK_EV_DOWN;
            }
            return ev != NULL;
        }
        return 0;
    }
    if (um == AX25_DM) {
        if (L->state == GT_LINK_IDLE) {
            return 0;
        }
        go_dead(L);
        if (ev != NULL) {
            ev->type = GT_LINK_EV_DOWN;
        }
        return ev != NULL;
    }
    if (um == AX25_DISC) {
        if (L->state == GT_LINK_IDLE) {
            return 0;
        }
        (void)send_frame(L, AX25_UA | 0x10u, 0, 0, NULL, 0, 0);
        go_dead(L);
        if (ev != NULL) {
            ev->type = GT_LINK_EV_DOWN;
        }
        return ev != NULL;
    }
    if (L->state != GT_LINK_CONNECTED) {
        return 0;
    }
    if (is_i(ctrl) || is_s(ctrl)) {
        ack_nr(L, nr_of(ctrl));
        (void)tx_window(L);
        arm_t3(L);
    }
    if (is_i(ctrl)) {
        if (ns_of(ctrl) == L->vr) {
            L->vr = (L->vr + 1) & 7;
            if (ack_later(L, 0, pf_of(ctrl)) != 0) {
                return -1;
            }
            if (ev != NULL && ilen >= 0) {
                ev->type = GT_LINK_EV_INFO;
                ev->info_len = ilen < GT_AX25_INFO_MAX ? ilen : GT_AX25_INFO_MAX;
                if (ev->info_len > 0) {
                    memcpy(ev->info, ibody, (size_t)ev->info_len);
                }
                return 1;
            }
        } else {
            if (ack_later(L, 1, pf_of(ctrl)) != 0) {
                return -1;
            }
        }
        return 0;
    }
    /* Reply F only to a Command+P. Response+F is the answer, not a poll. */
    if (is_s(ctrl) && pf_of(ctrl) && gt_ax25_is_cmd(pay, n)) {
        if (ack_later(L, 0, 1) != 0) {
            return -1;
        }
    }
    return 0;
}

static int on_timer(struct gt_link *L, struct gt_link_ev *ev)
{
    int64_t now = now_ms();

    if (L->t2_on && now >= L->t2_dl) {
        if (flush_t2(L) != 0) {
            return -1;
        }
    }
    if (L->t1_on && now >= L->t1_dl) {
        L->t1_on = 0;
        if (L->tries >= n2_max(L)) {
            go_dead(L);
            if (ev != NULL) {
                ev->type = GT_LINK_EV_DOWN;
            }
            return ev != NULL;
        }
        L->tries++;
        if (L->state == GT_LINK_SABM) {
            if (send_frame(L, AX25_SABM_P, 0, 0, NULL, 0, 1) != 0) {
                return -1;
            }
            arm_t1(L);
        } else if (L->state == GT_LINK_DISC) {
            if (send_frame(L, AX25_DISC_P, 0, 0, NULL, 0, 1) != 0) {
                return -1;
            }
            arm_t1(L);
        } else if (L->state == GT_LINK_CONNECTED) {
            if (rexmit(L) != 0) {
                return -1;
            }
            if (!L->t1_on) {
                if (send_frame(L, ctrl_rr(L->vr, 1), 0, 0, NULL, 0, 1) != 0) {
                    return -1;
                }
                arm_t1(L);
            }
        }
    }
    if (L->state == GT_LINK_CONNECTED && L->o.t3_ms > 0 && now >= L->t3_dl &&
        !L->t1_on) {
        if (send_frame(L, ctrl_rr(L->vr, 1), 0, 0, NULL, 0, 1) != 0) {
            return -1;
        }
        L->tries = 1;
        arm_t1(L);
        arm_t3(L);
    }
    return 0;
}

int gt_link_poll(struct gt_link *L, int timeout_ms,
                 const volatile sig_atomic_t *stop, struct gt_link_ev *ev)
{
    int r;
    int tr;

    if (ev != NULL) {
        ev->type = GT_LINK_EV_NONE;
        ev->info_len = 0;
    }
    if (L->io_err) {
        return -1;
    }
    tr = gt_link_tick(L, ev);
    if (tr < 0) {
        return -1;
    }
    if (tr > 0) {
        return 1;
    }
    r = gt_kiss_read_frame(L->fd, &L->rx, timeout_ms, stop);
    if (r < 0) {
        L->io_err = 1;
        return -1;
    }
    if (r == 0) {
        tr = gt_link_tick(L, ev);
        if (tr < 0) {
            return -1;
        }
        return tr > 0 ? 1 : 0;
    }
    if ((L->rx.buf[0] & KISS_CMD_MASK) != KISS_CMD_DATA ||
        L->rx.frame_len < 15) {
        return 0;
    }
    return gt_link_feed(L, L->rx.buf + 1, L->rx.frame_len - 1, ev);
}

int gt_link_send(int fd, const struct gt_link_opts *o,
                 uint8_t pid, const uint8_t *info, int info_len,
                 const volatile sig_atomic_t *stop)
{
    struct gt_link L;
    struct gt_link_ev ev;
    int queued = 0;
    int acked = 0;

    gt_link_init(&L, fd, o);
    if (gt_link_start(&L) != 0) {
        return -1;
    }
    for (;;) {
        if (stop != NULL && *stop) {
            break;
        }
        if (gt_link_poll(&L, 100, stop, &ev) < 0) {
            return -1;
        }
        if (ev.type == GT_LINK_EV_UP && !queued) {
            if (gt_link_queue(&L, pid, info, info_len) < 0) {
                (void)gt_link_disconnect(&L);
                return -1;
            }
            queued = 1;
        }
        if (queued && L.n_out == 0 && L.va == L.vs) {
            acked = 1;
            (void)gt_link_disconnect(&L);
        }
        if (L.state == GT_LINK_DEAD || L.state == GT_LINK_IDLE) {
            break;
        }
    }
    return acked ? 1 : 0;
}
