#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gtwav.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

/*
 * gtwav — capture Pico polydec 48 kHz int16 dump to a WAV file.
 */
#include "gt.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void usage(FILE *fp)
{
    gt_print_notice(fp, "gtwav");
    fputs(
        "Usage: gtwav [options] --device PATH --out FILE\n"
        "\n"
        "Capture KISS PCM (command 0x08) from a Pico TNC into a 48 kHz\n"
        "mono PCM16 WAV. Stop with Ctrl-C.\n"
        "\n"
        "  -d, --device PATH   CDC device node (or positional PATH)\n"
        "  -o, --out FILE      output WAV path (required)\n"
        "  -w, --wait SEC      wait for PATH to appear (default 0)\n"
        "  -h, --help          this help\n"
        "  -L, --license       print the software license\n",
        fp);
}

static void host_log(const char *tag, const char *fmt, ...)
{
    char ts[40];
    va_list ap;

    gt_stamp(ts, sizeof(ts));
    fprintf(stderr, "%s [%s] ", ts, tag);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

int main(int argc, char **argv)
{
    const char *dev = NULL;
    const char *out_path = NULL;
    int wait_sec = 0;
    int fd;
    int out;
    int opt;
    kiss_rx_t rx;
    int have_seq = 0;
    uint8_t expect_seq = 0;
    uint32_t n_samp = 0;
    uint32_t last_rep = 0;
    int rc = 0;
    int drain_ms;
    int off_sent = 0;
    uint8_t hdr[44];
    static const struct option longopts[] = {
        {"device", required_argument, NULL, 'd'},
        {"out", required_argument, NULL, 'o'},
        {"wait", required_argument, NULL, 'w'},
        {"help", no_argument, NULL, 'h'},
        {"license", no_argument, NULL, 'L'},
        {NULL, 0, NULL, 0},
    };

    while ((opt = getopt_long(argc, argv, "d:o:w:hL", longopts, NULL)) != -1) {
        char *end;

        switch (opt) {
        case 'd':
            dev = optarg;
            break;
        case 'o':
            out_path = optarg;
            break;
        case 'w':
            wait_sec = (int)strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || wait_sec < 0) {
                fprintf(stderr, "gtwav: invalid --wait '%s'\n", optarg);
                return 2;
            }
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'L':
            gt_print_license(stdout, "gtwav");
            return 0;
        default:
            usage(stderr);
            return 2;
        }
    }
    if (dev == NULL && optind < argc) {
        dev = argv[optind++];
    }
    if (out_path == NULL && optind < argc) {
        out_path = argv[optind++];
    }
    if (dev == NULL || out_path == NULL || optind != argc) {
        usage(stderr);
        return 2;
    }

    if (gt_tty_wait(dev, wait_sec) != 0) {
        fprintf(stderr, "gtwav: %s did not appear\n", dev);
        return 1;
    }
    fd = gt_tty_open(dev);
    if (fd < 0) {
        host_log("Open", "%s: %s", dev, strerror(errno));
        return 1;
    }
    host_log("Open", "%s", dev);
    out = open(out_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (out < 0) {
        host_log("Wav", "%s: %s", out_path, strerror(errno));
        gt_tty_close(fd);
        return 1;
    }
    gt_wav_hdr_48k_mono(hdr, 0);
    if (write(out, hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
        host_log("Wav", "header write: %s", strerror(errno));
        close(out);
        gt_tty_close(fd);
        return 1;
    }
    host_log("Wav", "%s 48000 Hz mono PCM16", out_path);

    signal(SIGINT, on_stop);
    signal(SIGTERM, on_stop);

    {
        uint8_t on = KISS_HW_DUMP_ON;

        if (gt_kiss_write(fd, KISS_CMD_SETHW, &on, 1) != 0) {
            host_log("Kiss", "TX SET-HW DUMP_ON 0x21 failed");
            close(out);
            gt_tty_close(fd);
            return 1;
        }
        host_log("Kiss", "TX SET-HW DUMP_ON 0x21");
    }

    kiss_rx_init(&rx);
    drain_ms = 0;
    while (!g_stop || drain_ms < 400) {
        fd_set rfds;
        struct timeval tv;
        unsigned char buf[4096];
        ssize_t nr;
        int sl;
        int off;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        sl = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (sl < 0) {
            if (errno == EINTR) {
                continue;
            }
            host_log("Error", "select: %s", strerror(errno));
            rc = 1;
            break;
        }
        if (g_stop) {
            drain_ms += 50;
            if (!off_sent) {
                uint8_t offb = KISS_HW_DUMP_OFF;

                if (gt_kiss_write(fd, KISS_CMD_SETHW, &offb, 1) != 0) {
                    host_log("Kiss", "TX SET-HW DUMP_OFF 0x20 failed");
                } else {
                    host_log("Kiss", "TX SET-HW DUMP_OFF 0x20");
                }
                off_sent = 1;
            }
        }
        if (sl == 0) {
            continue;
        }
        nr = read(fd, buf, sizeof(buf));
        if (nr < 0) {
            if (errno == EINTR) {
                continue;
            }
            host_log("Error", "read: %s", strerror(errno));
            rc = 1;
            break;
        }
        if (nr == 0) {
            break;
        }
        off = 0;
        while (off < (int)nr) {
            int used = 0;
            char line[KISS_MAX_FRAME];
            uint8_t seq;
            const uint8_t *samples;
            int ns;
            int crc_ok;

            if (!gt_kiss_feed(&rx, buf + off, (int)nr - off, &used)) {
                break;
            }
            off += used;
            if (gt_kiss_log_line(&rx, line, (int)sizeof(line))) {
                if (strstr(line, "[Dump] abort") != NULL) {
                    host_log("Dump", "Pico abort (PCM ring overflow)");
                    rc = 4;
                    g_stop = 1;
                }
            } else if (gt_pcm_parse(rx.buf, rx.frame_len, &seq, &samples, &ns, &crc_ok)) {
                if (!crc_ok) {
                    host_log("Error", "CRC fail seq=%u", (unsigned)seq);
                    rc = 3;
                    g_stop = 1;
                } else if (have_seq && seq != expect_seq) {
                    host_log("Error", "seq gap got=%u expect=%u",
                             (unsigned)seq, (unsigned)expect_seq);
                    rc = 3;
                    g_stop = 1;
                } else if (ns > 0) {
                    if (write(out, samples, (size_t)ns * 2u) != (ssize_t)ns * 2) {
                        host_log("Wav", "write: %s", strerror(errno));
                        rc = 1;
                        g_stop = 1;
                    } else {
                        if (!have_seq) {
                            host_log("RX", "first frame seq=%u n=%d",
                                     (unsigned)seq, ns);
                        }
                        n_samp += (uint32_t)ns;
                        have_seq = 1;
                        expect_seq = (uint8_t)(seq + 1u);
                        if (n_samp - last_rep >= GT_PCM_RATE_HZ) {
                            host_log("RX", "%lu samples", (unsigned long)n_samp);
                            last_rep = n_samp;
                        }
                    }
                }
            }
        }
    }

    if (lseek(out, 0, SEEK_SET) == 0) {
        gt_wav_hdr_48k_mono(hdr, n_samp);
        (void)write(out, hdr, sizeof(hdr));
    }
    close(out);
    gt_tty_close(fd);
    host_log("Wav", "wrote %lu samples (%.3f s) to %s",
             (unsigned long)n_samp, (double)n_samp / (double)GT_PCM_RATE_HZ,
             out_path);
    return rc;
}
