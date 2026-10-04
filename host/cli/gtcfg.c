#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gtcfg.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

/*
 * gtcfg — upload / download RAM KISS parameters as JSON.
 *
 *   gtcfg -d /dev/ttyACM0 --get
 *   gtcfg -d /dev/ttyACM0 --get -o tnc.json
 *   gtcfg -d /dev/ttyACM0 --put tnc.json
 */
#include "gt.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *fp)
{
    gt_print_notice(fp, "gtcfg");
    fputs(
        "Usage: gtcfg [options] --device PATH --get\n"
        "       gtcfg [options] --device PATH --put FILE\n"
        "\n"
        "Upload or download RAM KISS parameters (not stored in flash).\n"
        "Values are KISS units: txDelay/slotTime/txTail in 10 ms ticks.\n"
        "legacyMode true: only TAPR KISS + gtcfg (SETHW 0x11).\n"
        "\n"
        "  -d, --device PATH   CDC device node (or positional PATH)\n"
        "  -w, --wait SEC      wait for PATH to appear (default 0)\n"
        "  -g, --get           read device settings as JSON\n"
        "  -p, --put FILE      write JSON to device ('-' = stdin)\n"
        "  -o, --out FILE      write --get JSON to FILE (default stdout)\n"
        "  -h, --help          this help\n"
        "  -L, --license       print the software license\n",
        fp);
}

int main(int argc, char **argv)
{
    const char *dev = NULL;
    const char *put_path = NULL;
    const char *out_path = NULL;
    int do_get_flag = 0;
    int wait_sec = 0;
    int fd;
    int opt;
    static const struct option longopts[] = {
        {"device", required_argument, NULL, 'd'},
        {"wait", required_argument, NULL, 'w'},
        {"get", no_argument, NULL, 'g'},
        {"put", required_argument, NULL, 'p'},
        {"out", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'h'},
        {"license", no_argument, NULL, 'L'},
        {NULL, 0, NULL, 0},
    };

    while ((opt = getopt_long(argc, argv, "d:w:gp:o:hL", longopts, NULL)) != -1) {
        char *end;

        switch (opt) {
        case 'd':
            dev = optarg;
            break;
        case 'w':
            wait_sec = (int)strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || wait_sec < 0) {
                fprintf(stderr, "gtcfg: invalid --wait '%s'\n", optarg);
                return 2;
            }
            break;
        case 'g':
            do_get_flag = 1;
            break;
        case 'p':
            put_path = optarg;
            break;
        case 'o':
            out_path = optarg;
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'L':
            gt_print_license(stdout, "gtcfg");
            return 0;
        default:
            usage(stderr);
            return 2;
        }
    }
    if (dev == NULL && optind < argc) {
        dev = argv[optind++];
    }
    if (dev == NULL || optind != argc || (do_get_flag == 0 && put_path == NULL) ||
        (do_get_flag && put_path != NULL)) {
        usage(stderr);
        return 2;
    }

    if (gt_tty_wait(dev, wait_sec) != 0) {
        fprintf(stderr, "gtcfg: %s did not appear\n", dev);
        return 1;
    }
    fd = gt_tty_open(dev);
    if (fd < 0) {
        fprintf(stderr, "gtcfg: open %s: %s\n", dev, strerror(errno));
        return 1;
    }

    if (put_path != NULL) {
        char js[2048];
        int r;

        if (gt_load_text(put_path, js, (int)sizeof(js)) != 0) {
            fprintf(stderr, "gtcfg: read %s: %s\n", put_path, strerror(errno));
            gt_tty_close(fd);
            return 1;
        }
        r = gt_cfg_put_json(fd, js);
        if (r == -2) {
            fprintf(stderr, "gtcfg: bad JSON (need kiss txDelay/persist/…)\n");
            gt_tty_close(fd);
            return 2;
        }
        if (r != 1) {
            fprintf(stderr, "gtcfg: put failed (crc or no Cfg reply)\n");
            gt_tty_close(fd);
            return 1;
        }
        printf("put ok\n");
        gt_tty_close(fd);
        return 0;
    }

    {
        char raw[1024];
        char js[4096];
        FILE *out = stdout;
        int r = gt_cfg_get_json(fd, raw, (int)sizeof(raw));

        if (r == -1) {
            fprintf(stderr, "gtcfg: write: %s\n", strerror(errno));
            gt_tty_close(fd);
            return 1;
        }
        if (r == -2) {
            fprintf(stderr, "gtcfg: Cfg object does not fit\n");
            gt_tty_close(fd);
            return 1;
        }
        if (r < 2) {
            fprintf(stderr, "gtcfg: no Cfg reply (flash a UF2 that supports 0x11)\n");
            gt_tty_close(fd);
            return 1;
        }
        if (gt_cfg_pretty(raw, js, (int)sizeof(js)) < 0) {
            fprintf(stderr, "gtcfg: pretty-print failed\n");
            gt_tty_close(fd);
            return 1;
        }
        if (out_path != NULL) {
            out = fopen(out_path, "w");
            if (out == NULL) {
                fprintf(stderr, "gtcfg: open %s: %s\n", out_path, strerror(errno));
                gt_tty_close(fd);
                return 1;
            }
        }
        fputs(js, out);
        if (out != stdout) {
            fclose(out);
        }
    }
    gt_tty_close(fd);
    return 0;
}
