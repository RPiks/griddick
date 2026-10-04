#define _POSIX_C_SOURCE 200809L
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * griddickd.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

/*
 * griddickd — UDP over AX.25 UI via TUN + KISS TNC.
 *
 *   griddickd
 *   griddickd -c /opt/griddick/griddickd.conf
 */

#include "gd_conf.h"
#include "gd_tun.h"
#include "gd_wire.h"
#include "gt.h"

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define GD_UI_CTRL   0x03u
#define GD_UI_PID    0xF0u
#define GD_IP_TTL    64
#define GD_INFO_CAP  GT_AX25_INFO_MAX

static volatile sig_atomic_t g_stop;
static volatile sig_atomic_t g_reload;
static int g_log_stderr;
static const char *g_conf_path = GD_CONF_DEFAULT;
static struct gd_conf g_conf;
static uint16_t g_ip_id = 1;
static int g_info_max = GD_INFO_CAP;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void on_hup(int sig)
{
    (void)sig;
    g_reload = 1;
}

static void gd_log(int pri, const char *fmt, ...)
{
    va_list ap;
    char buf[256];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    syslog(pri, "%s", buf);
    if (g_log_stderr) {
        fprintf(stderr, "griddickd: %s\n", buf);
    }
}

static void usage(FILE *fp)
{
    gt_print_notice(fp, "griddickd");
    fputs(
        "Usage: griddickd [options]\n"
        "\n"
        "UDP-only TUN to AX.25 UI relay. Foreground. Owns the TNC port.\n"
        "\n"
        "  -c, --config FILE    config (default /opt/griddick/griddickd.conf)\n"
        "  -w, --wait SEC       wait for Device (overrides DeviceWait)\n"
        "  -h, --help           this help\n"
        "  -L, --license        print the software license\n"
        "\n"
        "SIGHUP reloads Peer/password/Call. Device/TunName/TunAddr need restart.\n",
        fp);
}

static uint16_t cksum_fold(uint32_t s)
{
    while (s >> 16) {
        s = (s & 0xffffu) + (s >> 16);
    }
    return (uint16_t)~s;
}

static uint16_t ip_cksum(const uint8_t *p, int n)
{
    uint32_t s = 0;
    int i;

    for (i = 0; i + 1 < n; i += 2) {
        s += (uint32_t)((p[i] << 8) | p[i + 1]);
    }
    if (i < n) {
        s += (uint32_t)(p[i] << 8);
    }
    return cksum_fold(s);
}

static uint16_t udp_cksum(uint32_t src, uint32_t dst,
                          const uint8_t *udp, int udp_len)
{
    uint32_t s = 0;
    int i;
    const uint8_t *a;

    a = (const uint8_t *)&src;
    s += (uint32_t)((a[0] << 8) | a[1]);
    s += (uint32_t)((a[2] << 8) | a[3]);
    a = (const uint8_t *)&dst;
    s += (uint32_t)((a[0] << 8) | a[1]);
    s += (uint32_t)((a[2] << 8) | a[3]);
    s += 17;
    s += (uint32_t)udp_len;
    for (i = 0; i + 1 < udp_len; i += 2) {
        s += (uint32_t)((udp[i] << 8) | udp[i + 1]);
    }
    if (i < udp_len) {
        s += (uint32_t)(udp[i] << 8);
    }
    return cksum_fold(s);
}

static int info_budget(const struct gd_peer *pr)
{
    int n = g_info_max - GD_WIRE_HDR_LEN;

    if (pr != NULL && pr->password[0] != '\0') {
        n -= GD_WIRE_AUTH_LEN;
    }
    return n < 0 ? 0 : n;
}

static int tun_mtu_for(int info_max)
{
    int pay = info_max - GD_WIRE_HDR_LEN - GD_WIRE_AUTH_LEN;

    if (pay < 0) {
        pay = 0;
    }
    return 20 + 8 + pay;
}

static int parse_udp4(const uint8_t *pkt, int n,
                      uint32_t *src, uint32_t *dst,
                      uint16_t *sport, uint16_t *dport,
                      const uint8_t **payload, int *plen)
{
    int ihl;
    int tot;
    int frag;
    int udp_off;
    int ulen;

    if (n < 28) {
        return -1;
    }
    if ((pkt[0] >> 4) != 4) {
        return -1;
    }
    ihl = (pkt[0] & 0x0f) * 4;
    if (ihl != 20) {
        return -1;
    }
    tot = (pkt[2] << 8) | pkt[3];
    if (tot < 28 || tot > n) {
        return -1;
    }
    frag = ((pkt[6] << 8) | pkt[7]) & 0x3fff;
    if (frag != 0) {
        return -1;
    }
    if (pkt[9] != 17) {
        return -1;
    }
    memcpy(src, pkt + 12, 4);
    memcpy(dst, pkt + 16, 4);
    udp_off = ihl;
    ulen = (pkt[udp_off + 4] << 8) | pkt[udp_off + 5];
    if (ulen < 8 || udp_off + ulen > tot) {
        return -1;
    }
    *sport = (uint16_t)((pkt[udp_off] << 8) | pkt[udp_off + 1]);
    *dport = (uint16_t)((pkt[udp_off + 2] << 8) | pkt[udp_off + 3]);
    *payload = pkt + udp_off + 8;
    *plen = ulen - 8;
    return 0;
}

static int build_udp4(uint8_t *out, int max,
                      uint32_t src, uint32_t dst,
                      uint16_t sport, uint16_t dport,
                      const uint8_t *payload, int plen)
{
    int tot = 20 + 8 + plen;
    uint16_t c;

    if (plen < 0 || tot > max) {
        return -1;
    }
    memset(out, 0, (size_t)tot);
    out[0] = 0x45;
    out[2] = (uint8_t)(tot >> 8);
    out[3] = (uint8_t)(tot & 0xff);
    out[4] = (uint8_t)(g_ip_id >> 8);
    out[5] = (uint8_t)(g_ip_id & 0xff);
    g_ip_id++;
    out[8] = GD_IP_TTL;
    out[9] = 17;
    memcpy(out + 12, &src, 4);
    memcpy(out + 16, &dst, 4);
    c = ip_cksum(out, 20);
    out[10] = (uint8_t)(c >> 8);
    out[11] = (uint8_t)(c & 0xff);
    out[20] = (uint8_t)(sport >> 8);
    out[21] = (uint8_t)(sport & 0xff);
    out[22] = (uint8_t)(dport >> 8);
    out[23] = (uint8_t)(dport & 0xff);
    out[24] = (uint8_t)((8 + plen) >> 8);
    out[25] = (uint8_t)((8 + plen) & 0xff);
    if (plen > 0) {
        memcpy(out + 28, payload, (size_t)plen);
    }
    c = udp_cksum(src, dst, out + 20, 8 + plen);
    if (c == 0) {
        c = 0xffff;
    }
    out[26] = (uint8_t)(c >> 8);
    out[27] = (uint8_t)(c & 0xff);
    return tot;
}

static int handle_tun(int tun_fd, int tnc_fd)
{
    uint8_t pkt[2048];
    ssize_t nr;
    uint32_t src;
    uint32_t dst;
    uint16_t sport;
    uint16_t dport;
    const uint8_t *pl;
    int plen;
    struct gd_peer *pr;
    uint8_t info[GD_INFO_CAP];
    uint8_t ax[2 * GT_AX25_ADDR_LEN + 2 + GD_INFO_CAP];
    int info_len;
    int ax_len;
    const char *pw;

    nr = read(tun_fd, pkt, sizeof(pkt));
    if (nr <= 0) {
        return nr == 0 ? 0 : -1;
    }
    if (parse_udp4(pkt, (int)nr, &src, &dst, &sport, &dport, &pl, &plen) != 0) {
        return 0;
    }
    if (src != g_conf.tun_ip) {
        return 0;
    }
    pr = gd_conf_peer_by_ip(&g_conf, dst);
    if (pr == NULL) {
        return 0;
    }
    if (plen > info_budget(pr)) {
        return 0;
    }
    pw = pr->password[0] != '\0' ? pr->password : NULL;
    info_len = gd_wire_pack(info, g_info_max, sport, dport, pl, plen, pw);
    if (info_len < 0) {
        return 0;
    }
    ax_len = gt_ax25_build(ax, (int)sizeof(ax),
                           &pr->call, &g_conf.local_call, NULL, 0,
                           GD_UI_CTRL, GD_UI_PID, info, info_len);
    if (ax_len < 0) {
        return 0;
    }
    if (gt_kiss_write(tnc_fd, KISS_CMD_DATA, ax, ax_len) != 0) {
        gd_log(LOG_ERR, "kiss write failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static void handle_kiss(int tun_fd, const kiss_rx_t *rx)
{
    struct gt_ax25_addr dst;
    struct gt_ax25_addr src;
    uint8_t ctrl;
    uint8_t pid;
    int has_pid;
    const uint8_t *info;
    int info_len;
    struct gd_peer *pr;
    uint16_t sport;
    uint16_t dport;
    const uint8_t *pl;
    int plen;
    uint8_t pkt[20 + 8 + GD_INFO_CAP];
    int n;
    const char *pw;

    if ((rx->buf[0] & KISS_CMD_MASK) != KISS_CMD_DATA || rx->frame_len < 2) {
        return;
    }
    if (gt_ax25_parse_frame(rx->buf + 1, rx->frame_len - 1,
                            &dst, &src, &ctrl, &pid, &has_pid,
                            &info, &info_len) != 0) {
        return;
    }
    if ((ctrl & 0xefu) != GD_UI_CTRL || !has_pid || pid != GD_UI_PID) {
        return;
    }
    if (!gt_ax25_addr_eq(&dst, &g_conf.local_call)) {
        return;
    }
    pr = gd_conf_peer_by_call(&g_conf, &src);
    if (pr == NULL) {
        return;
    }
    pw = pr->password[0] != '\0' ? pr->password : NULL;
    {
        int ur;
        char from[16];

        ur = gd_wire_unpack(info, info_len, pw, &sport, &dport, &pl, &plen);
        if (ur == -2) {
            gt_ax25_fmt_call(from, sizeof(from), &src);
            gd_log(LOG_WARNING, "auth mismatch from %s", from);
            return;
        }
        if (ur != 0) {
            return;
        }
    }
    n = build_udp4(pkt, (int)sizeof(pkt), pr->ip, g_conf.tun_ip,
                   sport, dport, pl, plen);
    if (n < 0) {
        return;
    }
    if (write(tun_fd, pkt, (size_t)n) != n) {
        gd_log(LOG_ERR, "tun write failed: %s", strerror(errno));
    }
}

static int reload_conf(void)
{
    struct gd_conf neu;
    char err[128];

    if (gd_conf_load(g_conf_path, &neu, err, (int)sizeof(err)) != 0) {
        gd_log(LOG_ERR, "reload failed: %s", err);
        return -1;
    }
    if (strcmp(neu.device, g_conf.device) != 0 ||
        strcmp(neu.tun_name, g_conf.tun_name) != 0 ||
        neu.tun_ip != g_conf.tun_ip ||
        neu.tun_mask != g_conf.tun_mask) {
        gd_log(LOG_WARNING,
               "reload: Device/TunName/TunAddr change ignored until restart");
        snprintf(neu.device, sizeof(neu.device), "%s", g_conf.device);
        snprintf(neu.tun_name, sizeof(neu.tun_name), "%s", g_conf.tun_name);
        neu.tun_ip = g_conf.tun_ip;
        neu.tun_mask = g_conf.tun_mask;
        neu.tun_prefix = g_conf.tun_prefix;
    }
    g_conf = neu;
    gd_log(LOG_INFO, "reloaded %d peer(s)", g_conf.n_peer);
    return 0;
}

static int open_tnc(int wait_sec, struct gt_cfg *tcfg)
{
    int fd;
    char local[16];
    char remote[16];

    if (gt_tty_wait(g_conf.device, wait_sec) != 0) {
        gd_log(LOG_ERR, "device %s not present", g_conf.device);
        return -1;
    }
    fd = gt_tty_open(g_conf.device);
    if (fd < 0) {
        gd_log(LOG_ERR, "open %s: %s", g_conf.device, strerror(errno));
        return -1;
    }
    if (gt_cfg_get(fd, tcfg) != 1) {
        gd_log(LOG_ERR, "cannot read TNC cfg");
        gt_tty_close(fd);
        return -1;
    }
    if (tcfg->have_legacy && tcfg->legacy) {
        gd_log(LOG_ERR, "legacyMode is on; refuse start");
        gt_tty_close(fd);
        return -1;
    }
    if (tcfg->have_mycall && tcfg->mycall[0] != '\0') {
        struct gt_ax25_addr mc;

        if (gt_ax25_parse_call(tcfg->mycall, &mc) == 0 &&
            !gt_ax25_addr_eq(&mc, &g_conf.local_call)) {
            gt_ax25_fmt_call(local, sizeof(local), &g_conf.local_call);
            gt_ax25_fmt_call(remote, sizeof(remote), &mc);
            gd_log(LOG_WARNING, "TNC myCall %s != Call %s", remote, local);
        }
    }
    if (tcfg->have_paclen && tcfg->paclen > 0 && tcfg->paclen < GD_INFO_CAP) {
        g_info_max = tcfg->paclen;
    } else {
        g_info_max = GD_INFO_CAP;
    }
    return fd;
}

int main(int argc, char **argv)
{
    int opt;
    int wait_override = -1;
    int tnc_fd = -1;
    int tun_fd = -1;
    int rc = 1;
    char err[128];
    char callbuf[16];
    struct gt_cfg tcfg;
    struct sigaction sa;
    kiss_rx_t rx;
    static const struct option longopts[] = {
        {"config", required_argument, NULL, 'c'},
        {"wait", required_argument, NULL, 'w'},
        {"help", no_argument, NULL, 'h'},
        {"license", no_argument, NULL, 'L'},
        {NULL, 0, NULL, 0},
    };

    while ((opt = getopt_long(argc, argv, "c:w:hL", longopts, NULL)) != -1) {
        char *end;

        switch (opt) {
        case 'c':
            g_conf_path = optarg;
            break;
        case 'w':
            wait_override = (int)strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || wait_override < 0) {
                fprintf(stderr, "griddickd: invalid --wait '%s'\n", optarg);
                return 1;
            }
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'L':
            gt_print_license(stdout, "griddickd");
            return 0;
        default:
            usage(stderr);
            return 1;
        }
    }
    if (optind != argc) {
        usage(stderr);
        return 1;
    }

    g_log_stderr = isatty(STDERR_FILENO);
    openlog("griddickd", LOG_PID | LOG_NDELAY, LOG_DAEMON);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = on_hup;
    sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    if (gd_conf_load(g_conf_path, &g_conf, err, (int)sizeof(err)) != 0) {
        gd_log(LOG_ERR, "config %s: %s", g_conf_path, err);
        goto done;
    }
    if (wait_override >= 0) {
        g_conf.device_wait = wait_override;
    }

    tnc_fd = open_tnc(g_conf.device_wait, &tcfg);
    if (tnc_fd < 0) {
        goto done;
    }
    tun_fd = gd_tun_open(g_conf.tun_name, g_conf.tun_ip, g_conf.tun_mask,
                         tun_mtu_for(g_info_max));
    if (tun_fd < 0) {
        gd_log(LOG_ERR, "tun %s: %s", g_conf.tun_name, strerror(errno));
        goto done;
    }

    gt_ax25_fmt_call(callbuf, sizeof(callbuf), &g_conf.local_call);
    gd_log(LOG_INFO, "started device=%s tun=%s call=%s peers=%d mtu=%d",
           g_conf.device, g_conf.tun_name, callbuf, g_conf.n_peer,
           tun_mtu_for(g_info_max));

    kiss_rx_init(&rx);
    while (!g_stop) {
        struct pollfd pfd[2];
        int pr;

        if (g_reload) {
            g_reload = 0;
            (void)reload_conf();
        }
        pfd[0].fd = tun_fd;
        pfd[0].events = POLLIN;
        pfd[0].revents = 0;
        pfd[1].fd = tnc_fd;
        pfd[1].events = POLLIN;
        pfd[1].revents = 0;
        pr = poll(pfd, 2, 500);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            gd_log(LOG_ERR, "poll: %s", strerror(errno));
            goto done;
        }
        if (pfd[0].revents & POLLIN) {
            if (handle_tun(tun_fd, tnc_fd) < 0) {
                goto done;
            }
        }
        if (pfd[1].revents & (POLLERR | POLLHUP)) {
            gd_log(LOG_ERR, "TNC port closed");
            goto done;
        }
        if (pfd[1].revents & POLLIN) {
            while (!g_stop && gt_kiss_read_frame(tnc_fd, &rx, 0, &g_stop) == 1) {
                handle_kiss(tun_fd, &rx);
                kiss_rx_init(&rx);
            }
        }
    }
    rc = 0;
    gd_log(LOG_INFO, "stopped");

done:
    gd_tun_close(tun_fd);
    gt_tty_close(tnc_fd);
    closelog();
    return rc;
}
