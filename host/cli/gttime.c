#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gttime.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

/*
 * gttime — set / read Pico wall clock over KISS SETHW.
 *
 *   gttime -d /dev/ttyACM0 --sync
 *   gttime -d /dev/ttyACM0 --get
 *   gttime -d /dev/ttyACM0 --sync --get
 */
#include "gt.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *fp)
{
    gt_print_notice(fp, "gttime");
    fputs(
        "Usage: gttime [options] --device PATH\n"
        "\n"
        "Sync or read the Pico TNC wall clock (unix epoch milliseconds).\n"
        "\n"
        "  -d, --device PATH   CDC device node (or positional PATH)\n"
        "  -w, --wait SEC      wait for PATH to appear (default 0)\n"
        "  -s, --sync          set TNC clock from host CLOCK_REALTIME\n"
        "  -g, --get           read TNC clock and compare to host\n"
        "  -h, --help          this help\n"
        "  -L, --license       print the software license\n"
        "\n"
        "If neither --sync nor --get is given, both are done (sync then get).\n",
        fp);
}

int main(int argc, char **argv)
{
    const char *dev = NULL;
    int wait_sec = 0;
    int do_sync = 0;
    int do_get = 0;
    int fd;
    int opt;
    static const struct option longopts[] = {
        {"device", required_argument, NULL, 'd'},
        {"wait", required_argument, NULL, 'w'},
        {"sync", no_argument, NULL, 's'},
        {"get", no_argument, NULL, 'g'},
        {"help", no_argument, NULL, 'h'},
        {"license", no_argument, NULL, 'L'},
        {NULL, 0, NULL, 0},
    };

    while ((opt = getopt_long(argc, argv, "d:w:sghL", longopts, NULL)) != -1) {
        char *end;

        switch (opt) {
        case 'd':
            dev = optarg;
            break;
        case 'w':
            wait_sec = (int)strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || wait_sec < 0) {
                fprintf(stderr, "gttime: invalid --wait '%s'\n", optarg);
                return 2;
            }
            break;
        case 's':
            do_sync = 1;
            break;
        case 'g':
            do_get = 1;
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'L':
            gt_print_license(stdout, "gttime");
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
    if (!do_sync && !do_get) {
        do_sync = 1;
        do_get = 1;
    }

    if (gt_tty_wait(dev, wait_sec) != 0) {
        fprintf(stderr, "gttime: %s did not appear\n", dev);
        return 1;
    }
    fd = gt_tty_open(dev);
    if (fd < 0) {
        fprintf(stderr, "gttime: open %s: %s\n", dev, strerror(errno));
        return 1;
    }

    if (do_sync) {
        uint64_t host = 0;
        uint64_t tnc = 0;
        int r = gt_time_sync(fd, &host, &tnc, NULL);

        if (r < 0) {
            fprintf(stderr, "gttime: write sync: %s\n", strerror(errno));
            gt_tty_close(fd);
            return 1;
        }
        if (r != 1) {
            fprintf(stderr, "gttime: no Time ack after sync\n");
            gt_tty_close(fd);
            return 1;
        }
        printf("sync host=%llu tnc=%llu\n",
               (unsigned long long)host, (unsigned long long)tnc);
    }

    if (do_get) {
        uint64_t host = 0;
        uint64_t tnc = 0;
        int64_t delta;
        int r = gt_time_get(fd, &host, &tnc, NULL);

        if (r < 0) {
            fprintf(stderr, "gttime: write get: %s\n", strerror(errno));
            gt_tty_close(fd);
            return 1;
        }
        if (r != 1) {
            fprintf(stderr, "gttime: no Time reply\n");
            gt_tty_close(fd);
            return 1;
        }
        delta = (int64_t)tnc - (int64_t)host;
        printf("get  host=%llu tnc=%llu delta_ms=%lld\n",
               (unsigned long long)host, (unsigned long long)tnc,
               (long long)delta);
    }

    gt_tty_close(fd);
    return 0;
}
