#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gttx.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

/*
 * gttx — send one AX.25 frame as KISS DATA (Address..Info, no FCS).
 *
 *   gttx -d /dev/ttyACM0
 *   gttx -d /dev/ttyACM0 GRIDDICK
 *   gttx -d /dev/ttyACM0 --text hello
 *   gttx -d /dev/ttyACM0 -s R2BDY-1 -t TEST-0 --text 'hello guys'
 */
#include "gt.h"

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop;
static int g_verbose;

#define PAYLOAD_MAX  512

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void usage(FILE *fp)
{
    gt_print_notice(fp, "gttx");
    fputs(
        "Usage: gttx [options] --device PATH [INFO]\n"
        "\n"
        "Send one AX.25 frame as KISS DATA (Address..Info, no FCS).\n"
        "Default is UI TEST-0>TEST-0 PID=F0 INFO=GRIDDICK.\n"
        "Do not run gtlog on the same port.\n"
        "\n"
        "  -d, --device PATH     CDC device node (or positional PATH)\n"
        "  -w, --wait SEC        wait for PATH to appear (default 0)\n"
        "  -s, --src CALL[-N]    source (default TEST-0)\n"
        "  -t, --dst CALL[-N]    destination (default TEST-0)\n"
        "  -p, --path LIST       digipeaters, comma-separated\n"
        "      --pid HEX         PID byte (default F0)\n"
        "      --ctrl HEX        control byte (default 03 = UI)\n"
        "      --text STR        ASCII info field\n"
        "      --hex HEX         info as hex (spaces or colons ok)\n"
        "      --file PATH       info from file ('-' = stdin)\n"
        "      --raw-hex HEX     raw Address..Info (no builder)\n"
        "      --raw-file PATH   raw Address..Info from file\n"
        "      --aprs-pos LAT,LON  uncompressed APRS position (decimal degrees)\n"
        "      --aprs-status STR APRS status ('>' + STR)\n"
        "      --aprs-msg CALL   APRS message to CALL; body is --text\n"
        "      --sym XY          APRS symbol table+code (default /-)\n"
        "      --comment STR     APRS position comment\n"
        "  -n, --count N         send N times (default 1; cap for --every)\n"
        "  -i, --interval SEC    pause between sends (default 0)\n"
        "  -e, --every SEC       beacon: repeat until Ctrl-C, period SEC\n"
        "      --arq             v2.0: SABM, one I, wait ACK, DISC\n"
        "  -v, --verbose         show ctrl, pid, byte count\n"
        "  -h, --help            this help\n"
        "  -L, --license         print the software license\n"
        "\n"
        "INFO positional is the same as --text. Info max 256 bytes (AX.25 N1).\n"
        "--arq cannot mix with --every/--count/--raw-/--aprs-/--ctrl.\n"
        "APRS dest default APZ001 (TOCALL, not a broadcast address).\n"
        "APRS path default WIDE1-1,WIDE2-1. Override with -t / -p.\n",
        fp);
}

static void sleep_sec(double sec)
{
    struct timespec ts;
    struct timespec rem;

    if (sec <= 0.0 || g_stop) {
        return;
    }
    ts.tv_sec = (time_t)sec;
    ts.tv_nsec = (long)((sec - (double)ts.tv_sec) * 1e9);
    if (ts.tv_nsec < 0) {
        ts.tv_nsec = 0;
    }
    while (nanosleep(&ts, &rem) != 0) {
        if (errno != EINTR || g_stop) {
            return;
        }
        ts = rem;
    }
}

static void sleep_until(const struct timespec *deadline)
{
    struct timespec now;
    double sec;

    clock_gettime(CLOCK_MONOTONIC, &now);
    sec = (double)(deadline->tv_sec - now.tv_sec) +
          (double)(deadline->tv_nsec - now.tv_nsec) * 1e-9;
    if (sec > 0.0) {
        sleep_sec(sec);
    }
}

static void deadline_add(struct timespec *t, double sec)
{
    long nsec;

    t->tv_sec += (time_t)sec;
    nsec = t->tv_nsec + (long)((sec - (double)(time_t)sec) * 1e9);
    if (nsec >= 1000000000L) {
        t->tv_sec++;
        nsec -= 1000000000L;
    }
    t->tv_nsec = nsec;
}

int main(int argc, char **argv)
{
    const char *dev = NULL;
    const char *src_s = "TEST-0";
    const char *dst_s = "TEST-0";
    const char *path_s = NULL;
    const char *text = NULL;
    const char *hex = NULL;
    const char *file = NULL;
    const char *raw_hex = NULL;
    const char *raw_file = NULL;
    const char *aprs_pos = NULL;
    const char *aprs_status = NULL;
    const char *aprs_msg = NULL;
    const char *aprs_sym = "/-";
    const char *aprs_comment = NULL;
    int dst_set = 0;
    int path_set = 0;
    int wait_sec = 0;
    int count = 1;
    int count_set = 0;
    int beacon = 0;
    int arq = 0;
    double interval = 0.0;
    double every = 0.0;
    uint8_t pid = 0xF0;
    uint8_t ctrl = 0x03;
    int fd;
    int opt;
    int n_pay;
    int k;
    int n_digi = 0;
    int builder;
    struct gt_ax25_addr src, dst, digi[GT_AX25_DIGI_MAX];
    uint8_t info[GT_AX25_INFO_MAX];
    int info_len = 0;
    uint8_t payload[PAYLOAD_MAX];
    char src_pr[16];
    char dst_pr[16];
    static const struct option longopts[] = {
        {"device", required_argument, NULL, 'd'},
        {"wait", required_argument, NULL, 'w'},
        {"src", required_argument, NULL, 's'},
        {"dst", required_argument, NULL, 't'},
        {"path", required_argument, NULL, 'p'},
        {"pid", required_argument, NULL, 1},
        {"ctrl", required_argument, NULL, 2},
        {"text", required_argument, NULL, 3},
        {"hex", required_argument, NULL, 4},
        {"file", required_argument, NULL, 5},
        {"raw-hex", required_argument, NULL, 6},
        {"raw-file", required_argument, NULL, 7},
        {"aprs-pos", required_argument, NULL, 8},
        {"aprs-status", required_argument, NULL, 9},
        {"aprs-msg", required_argument, NULL, 10},
        {"sym", required_argument, NULL, 11},
        {"comment", required_argument, NULL, 12},
        {"count", required_argument, NULL, 'n'},
        {"interval", required_argument, NULL, 'i'},
        {"every", required_argument, NULL, 'e'},
        {"arq", no_argument, NULL, 13},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {"license", no_argument, NULL, 'L'},
        {NULL, 0, NULL, 0},
    };

    while ((opt = getopt_long(argc, argv, "d:w:s:t:p:n:i:e:vhL", longopts, NULL)) != -1) {
        char *end;

        switch (opt) {
        case 'd':
            dev = optarg;
            break;
        case 'w':
            wait_sec = (int)strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || wait_sec < 0) {
                fprintf(stderr, "gttx: invalid --wait '%s'\n", optarg);
                return 2;
            }
            break;
        case 's':
            src_s = optarg;
            break;
        case 't':
            dst_s = optarg;
            dst_set = 1;
            break;
        case 'p':
            path_s = optarg;
            path_set = 1;
            break;
        case 1:
            if (gt_parse_hex_byte(optarg, &pid) != 0) {
                fprintf(stderr, "gttx: invalid --pid '%s'\n", optarg);
                return 2;
            }
            break;
        case 2:
            if (gt_parse_hex_byte(optarg, &ctrl) != 0) {
                fprintf(stderr, "gttx: invalid --ctrl '%s'\n", optarg);
                return 2;
            }
            break;
        case 3:
            text = optarg;
            break;
        case 4:
            hex = optarg;
            break;
        case 5:
            file = optarg;
            break;
        case 6:
            raw_hex = optarg;
            break;
        case 7:
            raw_file = optarg;
            break;
        case 8:
            aprs_pos = optarg;
            break;
        case 9:
            aprs_status = optarg;
            break;
        case 10:
            aprs_msg = optarg;
            break;
        case 11:
            aprs_sym = optarg;
            break;
        case 12:
            aprs_comment = optarg;
            break;
        case 'n':
            count = (int)strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || count < 1) {
                fprintf(stderr, "gttx: invalid --count '%s'\n", optarg);
                return 2;
            }
            count_set = 1;
            break;
        case 'i':
            interval = strtod(optarg, &end);
            if (end == optarg || *end != '\0' || interval < 0.0) {
                fprintf(stderr, "gttx: invalid --interval '%s'\n", optarg);
                return 2;
            }
            break;
        case 'e':
            every = strtod(optarg, &end);
            if (end == optarg || *end != '\0' || every <= 0.0) {
                fprintf(stderr, "gttx: invalid --every '%s'\n", optarg);
                return 2;
            }
            beacon = 1;
            break;
        case 13:
            arq = 1;
            break;
        case 'v':
            g_verbose = 1;
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'L':
            gt_print_license(stdout, "gttx");
            return 0;
        default:
            usage(stderr);
            return 2;
        }
    }
    if (dev == NULL && optind < argc) {
        dev = argv[optind++];
    }
    if (optind < argc) {
        if (text != NULL || hex != NULL || file != NULL ||
            raw_hex != NULL || raw_file != NULL ||
            aprs_pos != NULL || aprs_status != NULL || aprs_msg != NULL) {
            fprintf(stderr, "gttx: INFO conflicts with --text/--hex/--file/--raw-/--aprs-*\n");
            return 2;
        }
        text = argv[optind++];
    }
    if (dev == NULL || optind != argc) {
        usage(stderr);
        return 2;
    }

    builder = (raw_hex == NULL && raw_file == NULL);
    if (!builder) {
        if (raw_hex != NULL && raw_file != NULL) {
            fprintf(stderr, "gttx: use only one of --raw-hex / --raw-file\n");
            return 2;
        }
        if (path_s != NULL || hex != NULL || file != NULL || text != NULL ||
            aprs_pos != NULL || aprs_status != NULL || aprs_msg != NULL) {
            fprintf(stderr, "gttx: --raw-* cannot mix with builder flags\n");
            return 2;
        }
        if (strcmp(src_s, "TEST-0") != 0 || strcmp(dst_s, "TEST-0") != 0) {
            fprintf(stderr, "gttx: --raw-* cannot mix with --src/--dst\n");
            return 2;
        }
        if (raw_hex != NULL) {
            n_pay = gt_parse_hex_blob(raw_hex, payload, PAYLOAD_MAX);
            if (n_pay < 16) {
                fprintf(stderr, "gttx: invalid or short --raw-hex\n");
                return 2;
            }
        } else {
            n_pay = gt_load_file(raw_file, payload, PAYLOAD_MAX);
            if (n_pay == -2) {
                fprintf(stderr, "gttx: --raw-file too large\n");
                return 2;
            }
            if (n_pay < 16) {
                fprintf(stderr, "gttx: --raw-file: %s\n",
                        n_pay < 0 ? strerror(errno) : "too short");
                return 2;
            }
        }
        snprintf(src_pr, sizeof(src_pr), "raw");
        snprintf(dst_pr, sizeof(dst_pr), "raw");
    } else {
        int n_info_src = (text != NULL) + (hex != NULL) + (file != NULL);
        int n_aprs = (aprs_pos != NULL) + (aprs_status != NULL) + (aprs_msg != NULL);

        if (n_aprs > 1) {
            fprintf(stderr, "gttx: use only one of --aprs-pos / --aprs-status / --aprs-msg\n");
            return 2;
        }
        if (n_aprs && (hex != NULL || file != NULL)) {
            fprintf(stderr, "gttx: --aprs-* cannot mix with --hex / --file\n");
            return 2;
        }
        if (n_aprs && aprs_pos == NULL && aprs_comment != NULL) {
            fprintf(stderr, "gttx: --comment is for --aprs-pos only\n");
            return 2;
        }
        if (!n_aprs && n_info_src > 1) {
            fprintf(stderr, "gttx: use only one of --text / --hex / --file / INFO\n");
            return 2;
        }
        if (n_aprs) {
            if (!dst_set) {
                dst_s = GT_APRS_DEST_DEFAULT;
            }
            if (!path_set) {
                path_s = GT_APRS_PATH_DEFAULT;
            }
        }
        if (gt_ax25_parse_call(src_s, &src) != 0) {
            fprintf(stderr, "gttx: bad --src '%s'\n", src_s);
            return 2;
        }
        if (gt_ax25_parse_call(dst_s, &dst) != 0) {
            fprintf(stderr, "gttx: bad --dst '%s'\n", dst_s);
            return 2;
        }
        if (gt_ax25_parse_path(path_s, digi, &n_digi) != 0) {
            fprintf(stderr, "gttx: bad --path '%s'\n", path_s);
            return 2;
        }
        if (aprs_pos != NULL) {
            const char *cmt = aprs_comment != NULL ? aprs_comment : text;

            info_len = gt_aprs_build_pos(info, GT_AX25_INFO_MAX, aprs_pos, aprs_sym, cmt);
            if (info_len == -2) {
                fprintf(stderr, "gttx: --sym must be two chars, table / or \\\n");
                return 2;
            }
            if (info_len < 0) {
                fprintf(stderr, "gttx: invalid --aprs-pos '%s'\n", aprs_pos);
                return 2;
            }
        } else if (aprs_status != NULL) {
            if (text != NULL) {
                fprintf(stderr, "gttx: --aprs-status already has the text\n");
                return 2;
            }
            info_len = gt_aprs_build_status(info, GT_AX25_INFO_MAX, aprs_status);
            if (info_len < 0) {
                fprintf(stderr, "gttx: --aprs-status too long\n");
                return 2;
            }
        } else if (aprs_msg != NULL) {
            info_len = gt_aprs_build_msg(info, GT_AX25_INFO_MAX, aprs_msg,
                                         text != NULL ? text : "");
            if (info_len < 0) {
                fprintf(stderr, "gttx: bad --aprs-msg '%s'\n", aprs_msg);
                return 2;
            }
        } else if (hex != NULL) {
            info_len = gt_parse_hex_blob(hex, info, GT_AX25_INFO_MAX);
            if (info_len < 0) {
                fprintf(stderr, "gttx: invalid --hex\n");
                return 2;
            }
        } else if (file != NULL) {
            info_len = gt_load_file(file, info, GT_AX25_INFO_MAX);
            if (info_len == -2) {
                fprintf(stderr, "gttx: --file longer than 256\n");
                return 2;
            }
            if (info_len < 0) {
                fprintf(stderr, "gttx: --file %s: %s\n", file, strerror(errno));
                return 2;
            }
        } else {
            if (text == NULL) {
                text = "GRIDDICK";
            }
            info_len = (int)strlen(text);
            if (info_len > GT_AX25_INFO_MAX) {
                fprintf(stderr, "gttx: info longer than 256\n");
                return 2;
            }
            memcpy(info, text, (size_t)info_len);
        }
        n_pay = gt_ax25_build(payload, PAYLOAD_MAX, &dst, &src, digi, n_digi,
                              ctrl, pid, info, info_len);
        if (n_pay < 0) {
            fprintf(stderr, "gttx: payload too large\n");
            return 2;
        }
        gt_ax25_fmt_call(src_pr, sizeof(src_pr), &src);
        gt_ax25_fmt_call(dst_pr, sizeof(dst_pr), &dst);
    }

    if (arq) {
        if (!builder || beacon || count != 1 ||
            aprs_pos != NULL || aprs_status != NULL || aprs_msg != NULL) {
            fprintf(stderr, "gttx: --arq cannot mix with --every/--count/--raw-/--aprs-*\n");
            return 2;
        }
        if (ctrl != 0x03) {
            fprintf(stderr, "gttx: --arq cannot mix with --ctrl\n");
            return 2;
        }
    }

    if (gt_tty_wait(dev, wait_sec) != 0) {
        fprintf(stderr, "gttx: %s did not appear\n", dev);
        return 1;
    }
    fd = gt_tty_open(dev);
    if (fd < 0) {
        fprintf(stderr, "gttx: open %s: %s\n", dev, strerror(errno));
        return 1;
    }
    if (beacon || arq) {
        interval = every;
        signal(SIGINT, on_stop);
        signal(SIGTERM, on_stop);
    }
    if (arq) {
        struct gt_link_opts lo;
        struct gt_cfg cfg;
        int r;
        char ts[40];

        memset(&lo, 0, sizeof(lo));
        lo.src = src;
        lo.dst = dst;
        memcpy(lo.digi, digi, sizeof(lo.digi));
        lo.n_digi = n_digi;
        lo.t1_ms = 2000;
        lo.n2 = 10;
        lo.paclen = GT_AX25_INFO_MAX;
        lo.txdelay = 30;
        lo.txtail = 2;
        r = gt_cfg_get(fd, &cfg);
        if (r == 1) {
            if (cfg.t1_ms > 0) {
                lo.t1_ms = cfg.t1_ms;
            }
            if (cfg.t2_ms > 0) {
                lo.t2_ms = cfg.t2_ms;
            }
            if (cfg.n2 > 0) {
                lo.n2 = cfg.n2;
            }
            if (cfg.paclen > 0) {
                lo.paclen = cfg.paclen;
            }
            lo.txdelay = cfg.txdelay;
            lo.txtail = cfg.txtail;
            if (strcmp(src_s, "TEST-0") == 0 && cfg.mycall[0] != '\0') {
                if (gt_ax25_parse_call(cfg.mycall, &lo.src) == 0) {
                    src = lo.src;
                    gt_ax25_fmt_call(src_pr, sizeof(src_pr), &src);
                }
            }
        }
        if (info_len > lo.paclen) {
            fprintf(stderr, "gttx: info longer than paclen %d\n", lo.paclen);
            gt_tty_close(fd);
            return 2;
        }
        if (g_verbose) {
            fprintf(stderr, "gttx: arq t1=%d n2=%d paclen=%d src=%s\n",
                    lo.t1_ms, lo.n2, lo.paclen, src_pr);
        }
        r = gt_link_send(fd, &lo, pid, info, info_len, &g_stop);
        gt_stamp(ts, sizeof(ts));
        if (r < 0) {
            fprintf(stderr, "gttx: arq i/o: %s\n", strerror(errno));
            gt_tty_close(fd);
            return 1;
        }
        if (r != 1) {
            printf("%s arq fail %s>%s\n", ts, src_pr, dst_pr);
            gt_tty_close(fd);
            return 1;
        }
        printf("%s arq ok %s>%s\n", ts, src_pr, dst_pr);
        gt_tty_close(fd);
        return 0;
    }

    {
        struct timespec next;
        char ts[40];

        clock_gettime(CLOCK_MONOTONIC, &next);
        for (k = 0; !g_stop; k++) {
            int tx;

            if (beacon && k > 0) {
                sleep_until(&next);
                if (g_stop) {
                    break;
                }
            }
            if (gt_kiss_write(fd, KISS_CMD_DATA, payload, n_pay) != 0) {
                fprintf(stderr, "gttx: write: %s\n", strerror(errno));
                gt_tty_close(fd);
                return 1;
            }
            (void)gt_kiss_drain(fd);
            tx = gt_kiss_wait_tx(fd, 5000, &g_stop);
            if (tx < 0) {
                fprintf(stderr, "gttx: wait Tx done: %s\n", strerror(errno));
                gt_tty_close(fd);
                return 1;
            }
            gt_stamp(ts, sizeof(ts));
            if (tx == 0) {
                printf("%s drop %s>%s (no Tx done)\n", ts, src_pr, dst_pr);
            } else if (builder && g_verbose) {
                printf("%s sent %s>%s ctrl=%02X pid=%02X bytes=%d\n",
                       ts, src_pr, dst_pr, ctrl, pid, n_pay);
            } else if (builder) {
                printf("%s sent %s>%s\n", ts, src_pr, dst_pr);
            } else if (g_verbose) {
                printf("%s sent raw bytes=%d\n", ts, n_pay);
            } else {
                printf("%s sent raw\n", ts);
            }
            fflush(stdout);
            {
                int more;

                if (beacon) {
                    more = (!count_set || k + 1 < count);
                } else {
                    more = (k + 1 < count);
                }
                if (!more) {
                    break;
                }
            }
            if (beacon) {
                deadline_add(&next, interval);
                {
                    struct timespec now;

                    clock_gettime(CLOCK_MONOTONIC, &now);
                    if (now.tv_sec > next.tv_sec ||
                        (now.tv_sec == next.tv_sec &&
                         now.tv_nsec > next.tv_nsec)) {
                        next = now;
                    }
                }
            } else {
                sleep_sec(interval);
            }
        }
    }
    gt_tty_close(fd);
    return 0;
}
