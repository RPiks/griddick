#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gtcall.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

/*
 * gtcall — AX.25 v2.0 session: originate SABM, or --listen for SABM.
 * Line on stdin → I-frame. Peer I-info → stdout.
 */
#include "gt.h"

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop;
static int g_verbose;
static int g_echo;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void usage(FILE *fp)
{
    gt_print_notice(fp, "gtcall");
    fputs(
        "Usage: gtcall [options] --device PATH --dst CALL\n"
        "       gtcall [options] --device PATH --listen --dst CALL|ANY\n"
        "\n"
        "AX.25 v2.0 session. One line of stdin = one I.\n"
        "Originate: SABM to --dst (also accepts SABM from that peer).\n"
        "Listen: wait for SABM from --dst; ANY = no peer filter.\n"
        "Listen returns to wait after DISC. Ctrl-C or stdin EOF exits\n"
        "(sends DISC if the link is up).\n"
        "Do not run gtlog/gtdump on the same port.\n"
        "\n"
        "  gtcall -d /dev/ttyACM0 -s JBLADE -t CRAX-3\n"
        "  gtcall -d /dev/ttyACM0 --listen -s ZXBBS-0 -t ANY\n"
        "\n"
        "  -d, --device PATH   CDC device node (or positional PATH)\n"
        "  -w, --wait SEC      wait for PATH to appear (default 0)\n"
        "  -s, --src CALL[-N]  source (default link.myCall or TEST-0)\n"
        "  -t, --dst CALL[-N]  peer (required to originate; ANY with --listen)\n"
        "  -p, --path LIST     digipeaters, comma-separated\n"
        "      --listen        wait for SABM; do not send SABM\n"
        "      --echo          print our own lines as they are queued\n"
        "  -v, --verbose       state / control trace on stderr\n"
        "  -h, --help          this help\n"
        "  -L, --license       print the software license\n",
        fp);
}

static int is_any_call(const char *s)
{
    return s != NULL && strcasecmp(s, "ANY") == 0;
}

static void apply_cfg(struct gt_link_opts *lo, const struct gt_cfg *c,
                      int src_default, char *src_pr, size_t src_pr_sz)
{
    if (c->t1_ms > 0) {
        lo->t1_ms = c->t1_ms;
    }
    if (c->t2_ms > 0) {
        lo->t2_ms = c->t2_ms;
    }
    if (c->t3_ms > 0) {
        lo->t3_ms = c->t3_ms;
    }
    if (c->n2 > 0) {
        lo->n2 = c->n2;
    }
    if (c->paclen > 0) {
        lo->paclen = c->paclen;
    }
    if (c->maxframe > 0) {
        lo->maxframe = c->maxframe;
    }
    lo->txdelay = c->txdelay;
    lo->txtail = c->txtail;
    if (src_default && c->mycall[0] != '\0' &&
        gt_ax25_parse_call(c->mycall, &lo->src) == 0) {
        gt_ax25_fmt_call(src_pr, src_pr_sz, &lo->src);
    }
}

int main(int argc, char **argv)
{
    const char *dev = NULL;
    const char *src_s = "TEST-0";
    const char *dst_s = NULL;
    const char *path_s = NULL;
    int src_default = 1;
    int wait_sec = 0;
    int listen = 0;
    int accept_any = 0;
    int fd;
    int opt;
    int n_digi = 0;
    int up = 0;
    struct gt_link_opts lo;
    struct gt_link L;
    struct gt_cfg cfg;
    struct gt_ax25_addr digi[GT_AX25_DIGI_MAX];
    char src_pr[16];
    char dst_pr[16];
    static const struct option longopts[] = {
        {"device", required_argument, NULL, 'd'},
        {"wait", required_argument, NULL, 'w'},
        {"src", required_argument, NULL, 's'},
        {"dst", required_argument, NULL, 't'},
        {"path", required_argument, NULL, 'p'},
        {"listen", no_argument, NULL, 2},
        {"echo", no_argument, NULL, 1},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {"license", no_argument, NULL, 'L'},
        {NULL, 0, NULL, 0},
    };

    while ((opt = getopt_long(argc, argv, "d:w:s:t:p:vhL", longopts, NULL)) !=
           -1) {
        char *end;

        switch (opt) {
        case 'd':
            dev = optarg;
            break;
        case 'w':
            wait_sec = (int)strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || wait_sec < 0) {
                fprintf(stderr, "gtcall: invalid --wait '%s'\n", optarg);
                return 2;
            }
            break;
        case 's':
            src_s = optarg;
            src_default = 0;
            break;
        case 't':
            dst_s = optarg;
            break;
        case 'p':
            path_s = optarg;
            break;
        case 2:
            listen = 1;
            break;
        case 1:
            g_echo = 1;
            break;
        case 'v':
            g_verbose = 1;
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'L':
            gt_print_license(stdout, "gtcall");
            return 0;
        default:
            usage(stderr);
            return 2;
        }
    }
    if (dev == NULL && optind < argc) {
        dev = argv[optind++];
    }
    if (dst_s == NULL && optind < argc) {
        dst_s = argv[optind++];
    }
    if (dev == NULL || optind != argc) {
        usage(stderr);
        return 2;
    }
    if (!listen && dst_s == NULL) {
        usage(stderr);
        return 2;
    }
    if (!listen && is_any_call(dst_s)) {
        fprintf(stderr, "gtcall: --dst ANY requires --listen\n");
        return 2;
    }
    accept_any = listen && (dst_s == NULL || is_any_call(dst_s));

    memset(&lo, 0, sizeof(lo));
    if (gt_ax25_parse_call(src_s, &lo.src) != 0) {
        fprintf(stderr, "gtcall: bad --src '%s'\n", src_s);
        return 2;
    }
    if (!accept_any) {
        if (gt_ax25_parse_call(dst_s, &lo.dst) != 0) {
            fprintf(stderr, "gtcall: bad --dst '%s'\n", dst_s);
            return 2;
        }
    }
    lo.accept_any = accept_any;
    if (gt_ax25_parse_path(path_s, digi, &n_digi) != 0) {
        fprintf(stderr, "gtcall: bad --path '%s'\n", path_s);
        return 2;
    }
    memcpy(lo.digi, digi, sizeof(lo.digi));
    lo.n_digi = n_digi;
    lo.t1_ms = 2000;
    lo.t3_ms = 30000;
    lo.n2 = 10;
    lo.paclen = GT_AX25_INFO_MAX;
    lo.maxframe = 4;
    lo.txdelay = 30;
    lo.txtail = 2;
    gt_ax25_fmt_call(src_pr, sizeof(src_pr), &lo.src);
    if (accept_any) {
        snprintf(dst_pr, sizeof(dst_pr), "ANY");
    } else {
        gt_ax25_fmt_call(dst_pr, sizeof(dst_pr), &lo.dst);
    }

    if (gt_tty_wait(dev, wait_sec) != 0) {
        fprintf(stderr, "gtcall: %s did not appear\n", dev);
        return 1;
    }
    fd = gt_tty_open(dev);
    if (fd < 0) {
        fprintf(stderr, "gtcall: open %s: %s\n", dev, strerror(errno));
        return 1;
    }
    if (gt_cfg_get(fd, &cfg) == 1) {
        apply_cfg(&lo, &cfg, src_default, src_pr, sizeof(src_pr));
    }

    signal(SIGINT, on_stop);
    signal(SIGTERM, on_stop);

    gt_link_init(&L, fd, &lo);
    if (!listen) {
        if (gt_link_start(&L) != 0) {
            fprintf(stderr, "gtcall: start: %s\n", strerror(errno));
            gt_tty_close(fd);
            return 1;
        }
        if (g_verbose) {
            fprintf(stderr, "gtcall: sabm %s>%s t1=%d t2=%d n2=%d\n", src_pr,
                    dst_pr, lo.t1_ms, lo.t2_ms, lo.n2);
        }
    } else if (g_verbose) {
        fprintf(stderr, "gtcall: listen %s from %s t1=%d t2=%d n2=%d\n",
                src_pr, dst_pr, lo.t1_ms, lo.t2_ms, lo.n2);
    }

    while (!g_stop) {
        if (gt_link_state(&L) == GT_LINK_DEAD) {
            if (!listen) {
                break;
            }
            gt_link_init(&L, fd, &lo);
            if (g_verbose) {
                fprintf(stderr, "gtcall: listen %s from %s\n", src_pr,
                        accept_any ? "ANY" : dst_pr);
            }
        }
        fd_set rfds;
        struct timeval tv;
        int nf;
        struct gt_link_ev ev;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        if (!feof(stdin)) {
            FD_SET(0, &rfds);
        }
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        nf = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (nf < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "gtcall: select: %s\n", strerror(errno));
            break;
        }
        if (nf > 0 && FD_ISSET(0, &rfds)) {
            char *line = NULL;
            size_t cap = 0;
            ssize_t n = getline(&line, &cap, stdin);

            if (n < 0) {
                g_stop = 1;
                free(line);
            } else {
                while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
                    line[--n] = '\0';
                }
                if (gt_link_state(&L) == GT_LINK_CONNECTED) {
                    int q = gt_link_queue(&L, 0xF0, (const uint8_t *)line,
                                          (int)n);

                    if (q == 1 && g_echo) {
                        printf("> %s\n", line);
                        fflush(stdout);
                    } else if (q == 0) {
                        fprintf(stderr, "gtcall: window full\n");
                    } else if (q == -2) {
                        fprintf(stderr, "gtcall: line longer than paclen\n");
                    }
                } else if (g_verbose) {
                    fprintf(stderr, "gtcall: not connected, line dropped\n");
                }
                free(line);
            }
        }
        if (gt_link_poll(&L, (nf > 0 && FD_ISSET(fd, &rfds)) ? 0 : 50,
                         &g_stop, &ev) < 0) {
            fprintf(stderr, "gtcall: i/o: %s\n", strerror(errno));
            gt_tty_close(fd);
            return 1;
        }
        if (ev.type == GT_LINK_EV_UP) {
            up = 1;
            gt_ax25_fmt_call(dst_pr, sizeof(dst_pr), &L.o.dst);
            fprintf(stderr, "gtcall: connected %s>%s\n", src_pr, dst_pr);
        } else if (ev.type == GT_LINK_EV_INFO) {
            fwrite(ev.info, 1, (size_t)ev.info_len, stdout);
            fputc('\n', stdout);
            fflush(stdout);
        } else if (ev.type == GT_LINK_EV_UI && g_verbose) {
            fprintf(stderr, "gtcall: ui len=%d\n", ev.info_len);
        } else if (ev.type == GT_LINK_EV_DOWN) {
            fprintf(stderr, "gtcall: disconnected\n");
            if (!listen) {
                break;
            }
        }
    }

    if (gt_link_state(&L) == GT_LINK_CONNECTED ||
        gt_link_state(&L) == GT_LINK_SABM) {
        (void)gt_link_disconnect(&L);
        while (gt_link_state(&L) == GT_LINK_DISC && !g_stop) {
            struct gt_link_ev ev;

            if (gt_link_poll(&L, 100, &g_stop, &ev) < 0) {
                break;
            }
        }
    }
    gt_tty_close(fd);
    if (listen) {
        return 0;
    }
    if (!up) {
        if (g_verbose) {
            fprintf(stderr, "gtcall: no connect\n");
        }
        return 1;
    }
    return 0;
}
