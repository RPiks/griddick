#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gtdump.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

/*
 * gtdump — sync Pico clock, then print decoded KISS DATA.
 *
 *   gtdump -d /dev/ttyACM0
 *   gtdump -d /dev/ttyACM0 --no-sync -o heard.log
 */
#include "gt.h"

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t g_stop;
static FILE *g_out;
static int g_verbose;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void usage(FILE *fp)
{
    gt_print_notice(fp, "gtdump");
    fputs(
        "Usage: gtdump [options] --device PATH\n"
        "\n"
        "Sync the Pico clock, then dump decoded AX.25 frames\n"
        "(KISS DATA from the TNC). Firmware logs: use gtlog. Ctrl-C to stop.\n"
        "Do not run gtlog / gtwav / gtfeed on the same port.\n"
        "\n"
        "  -d, --device PATH   CDC device node (or positional PATH)\n"
        "  -w, --wait SEC      wait for PATH to appear (default 0)\n"
        "      --no-sync       do not set the TNC clock\n"
        "  -v, --verbose       show ctrl, pid, length\n"
        "  -o, --out FILE      also write output to FILE\n"
        "  -h, --help          this help\n"
        "  -L, --license       print the software license\n",
        fp);
}

static void emit(const char *line)
{
    fputs(line, stdout);
    fputc('\n', stdout);
    fflush(stdout);
    if (g_out != NULL) {
        fputs(line, g_out);
        fputc('\n', g_out);
        fflush(g_out);
    }
}

int main(int argc, char **argv)
{
    const char *dev = NULL;
    const char *out_path = NULL;
    int wait_sec = 0;
    int do_sync = 1;
    int fd;
    int opt;
    kiss_rx_t rx;
    static const struct option longopts[] = {
        {"device", required_argument, NULL, 'd'},
        {"wait", required_argument, NULL, 'w'},
        {"no-sync", no_argument, NULL, 1},
        {"verbose", no_argument, NULL, 'v'},
        {"out", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'h'},
        {"license", no_argument, NULL, 'L'},
        {NULL, 0, NULL, 0},
    };

    while ((opt = getopt_long(argc, argv, "d:w:vo:hL", longopts, NULL)) != -1) {
        char *end;

        switch (opt) {
        case 'd':
            dev = optarg;
            break;
        case 'w':
            wait_sec = (int)strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || wait_sec < 0) {
                fprintf(stderr, "gtdump: invalid --wait '%s'\n", optarg);
                return 2;
            }
            break;
        case 1:
            do_sync = 0;
            break;
        case 'v':
            g_verbose = 1;
            break;
        case 'o':
            out_path = optarg;
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'L':
            gt_print_license(stdout, "gtdump");
            return 0;
        default:
            usage(stderr);
            return 2;
        }
    }
    if (dev == NULL && optind < argc) {
        dev = argv[optind++];
    }
    if (dev == NULL || optind != argc) {
        usage(stderr);
        return 2;
    }

    if (gt_tty_wait(dev, wait_sec) != 0) {
        fprintf(stderr, "gtdump: %s did not appear\n", dev);
        return 1;
    }
    fd = gt_tty_open(dev);
    if (fd < 0) {
        fprintf(stderr, "gtdump: open %s: %s\n", dev, strerror(errno));
        return 1;
    }
    if (out_path != NULL) {
        g_out = fopen(out_path, "a");
        if (g_out == NULL) {
            fprintf(stderr, "gtdump: open %s: %s\n", out_path, strerror(errno));
            gt_tty_close(fd);
            return 1;
        }
    }

    signal(SIGINT, on_stop);
    signal(SIGTERM, on_stop);

    if (do_sync) {
        uint64_t host = 0;
        uint64_t tnc = 0;
        int r = gt_time_sync(fd, &host, &tnc, &g_stop);
        char ts[40];
        char line[128];

        if (r < 0) {
            fprintf(stderr, "gtdump: write sync: %s\n", strerror(errno));
            if (g_out != NULL) {
                fclose(g_out);
            }
            gt_tty_close(fd);
            return 1;
        }
        if (r != 1) {
            fprintf(stderr, "gtdump: no Time ack after sync\n");
            if (g_out != NULL) {
                fclose(g_out);
            }
            gt_tty_close(fd);
            return 1;
        }
        gt_stamp(ts, sizeof(ts));
        snprintf(line, sizeof(line), "%s sync host=%llu tnc=%llu",
                 ts, (unsigned long long)host, (unsigned long long)tnc);
        emit(line);
    }

    kiss_rx_init(&rx);
    while (!g_stop) {
        int r = gt_kiss_read_frame(fd, &rx, 200, &g_stop);

        if (r < 0) {
            break;
        }
        if (r == 0) {
            continue;
        }
        if ((rx.buf[0] & KISS_CMD_MASK) == KISS_CMD_DATA && rx.frame_len > 1) {
            char ts[40];
            char line[512];

            gt_stamp(ts, sizeof(ts));
            gt_ax25_format_data(rx.buf + 1, rx.frame_len - 1, g_verbose,
                                ts, line, (int)sizeof(line));
            emit(line);
        }
    }

    if (g_out != NULL) {
        fclose(g_out);
    }
    gt_tty_close(fd);
    return 0;
}
