/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gtlog.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

/*
 * gtlog — print KISS CMD_LOG (0x07) lines from a Pico TNC CDC port.
 *
 *   gtlog --device /dev/ttyACM0
 *   gtlog --wait 20 /dev/serial/by-id/usb-Raspberry_Pi_Griddick_TNC-*-if00
 */
#include "gt.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *fp)
{
    gt_print_notice(fp, "gtlog");
    fputs(
        "Usage: gtlog [options] --device PATH\n"
        "       gtlog [options] PATH\n"
        "\n"
        "Print ASCII KISS log frames (command 0x07) from a Pico CDC port.\n"
        "\n"
        "  -d, --device PATH   CDC device node (required unless PATH is positional)\n"
        "  -w, --wait SEC      wait up to SEC seconds for PATH to appear (default 0)\n"
        "  -h, --help          this help\n"
        "  -L, --license       print the software license\n",
        fp);
}

int main(int argc, char **argv)
{
    const char *dev = NULL;
    int wait_sec = 0;
    int fd;
    int opt;
    kiss_rx_t rx;
    static const struct option longopts[] = {
        {"device", required_argument, NULL, 'd'},
        {"wait", required_argument, NULL, 'w'},
        {"help", no_argument, NULL, 'h'},
        {"license", no_argument, NULL, 'L'},
        {NULL, 0, NULL, 0},
    };

    while ((opt = getopt_long(argc, argv, "d:w:hL", longopts, NULL)) != -1) {
        char *end;

        switch (opt) {
        case 'd':
            dev = optarg;
            break;
        case 'w':
            wait_sec = (int)strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || wait_sec < 0) {
                fprintf(stderr, "gtlog: invalid --wait '%s'\n", optarg);
                return 2;
            }
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'L':
            gt_print_license(stdout, "gtlog");
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
        fprintf(stderr, "gtlog: %s did not appear\n", dev);
        return 1;
    }
    fd = gt_tty_open(dev);
    if (fd < 0) {
        fprintf(stderr, "gtlog: open %s: %s\n", dev, strerror(errno));
        return 1;
    }

    kiss_rx_init(&rx);
    for (;;) {
        char line[KISS_MAX_FRAME];
        int r = gt_kiss_read_frame(fd, &rx, -1, NULL);

        if (r < 0) {
            if (errno == 0) {
                break;
            }
            fprintf(stderr, "gtlog: read: %s\n", strerror(errno));
            gt_tty_close(fd);
            return 1;
        }
        if (r == 0) {
            continue;
        }
        if (gt_kiss_log_line(&rx, line, (int)sizeof(line))) {
            fputs(line, stdout);
            fputc('\n', stdout);
            fflush(stdout);
        }
    }
    gt_tty_close(fd);
    return 0;
}
