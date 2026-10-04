#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gtfeed.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

/*
 * gtfeed — play a 48 kHz mono PCM16 WAV into the Pico tract.
 */
#include "gt.h"

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#define PREROLL  4

static volatile sig_atomic_t g_stop;
static FILE *g_log;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void usage(FILE *fp)
{
    gt_print_notice(fp, "gtfeed");
    fputs(
        "Usage: gtfeed [options] --device PATH [--out LOG] WAV\n"
        "\n"
        "Play a 48 kHz mono PCM16 WAV into the Pico demod tract.\n"
        "\n"
        "  -d, --device PATH   CDC device node\n"
        "  -o, --out FILE      also write the host log to FILE\n"
        "  -w, --wait SEC      wait for PATH to appear (default 0)\n"
        "  -h, --help          this help\n"
        "  -L, --license       print the software license\n",
        fp);
}

static void host_log(const char *tag, const char *fmt, ...)
{
    char ts[40];
    char line[256];
    int n;
    int m;
    va_list ap;

    gt_stamp(ts, sizeof(ts));
    n = snprintf(line, sizeof(line), "%s [%s] ", ts, tag);
    if (n < 0) {
        return;
    }
    if (n >= (int)sizeof(line)) {
        n = (int)sizeof(line) - 1;
    }
    va_start(ap, fmt);
    m = vsnprintf(line + n, sizeof(line) - (size_t)n, fmt, ap);
    va_end(ap);
    if (m < 0) {
        return;
    }
    n += m;
    if (n >= (int)sizeof(line)) {
        n = (int)sizeof(line) - 1;
    }
    line[n] = '\0';
    fputs(line, stderr);
    fputc('\n', stderr);
    if (g_log != NULL) {
        fputs(line, g_log);
        fputc('\n', g_log);
        fflush(g_log);
    }
}

static void timespec_add_ns(struct timespec *t, long ns)
{
    t->tv_nsec += ns;
    while (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        t->tv_sec++;
    }
}

int main(int argc, char **argv)
{
    const char *dev = NULL;
    const char *log_path = NULL;
    const char *wav_path = NULL;
    int wait_sec = 0;
    int fd;
    int wav;
    int opt;
    kiss_rx_t rx;
    uint32_t wav_n = 0;
    uint32_t sent = 0;
    uint32_t last_rep = 0;
    uint32_t ax25_n = 0;
    uint32_t fcs_ok = 0;
    uint32_t fcs_fail = 0;
    int rc = 0;
    int off_sent = 0;
    int drain_ms = 0;
    int preroll = 0;
    uint8_t seq = 0;
    struct timespec due;
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
            log_path = optarg;
            break;
        case 'w':
            wait_sec = (int)strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || wait_sec < 0) {
                fprintf(stderr, "gtfeed: invalid --wait '%s'\n", optarg);
                return 2;
            }
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'L':
            gt_print_license(stdout, "gtfeed");
            return 0;
        default:
            usage(stderr);
            return 2;
        }
    }
    if (dev == NULL && optind < argc) {
        dev = argv[optind++];
    }
    if (wav_path == NULL && optind < argc) {
        wav_path = argv[optind++];
    }
    if (dev == NULL || wav_path == NULL || optind != argc) {
        usage(stderr);
        return 2;
    }

    if (log_path != NULL) {
        g_log = fopen(log_path, "w");
        if (g_log == NULL) {
            fprintf(stderr, "gtfeed: open %s: %s\n", log_path, strerror(errno));
            return 1;
        }
    }

    if (gt_tty_wait(dev, wait_sec) != 0) {
        host_log("Open", "%s did not appear", dev);
        if (g_log != NULL) {
            fclose(g_log);
        }
        return 1;
    }
    if (gt_wav_open_48k_mono(wav_path, &wav, &wav_n) != 0) {
        host_log("Wav", "%s: need 48 kHz mono PCM16", wav_path);
        if (g_log != NULL) {
            fclose(g_log);
        }
        return 1;
    }
    fd = gt_tty_open(dev);
    if (fd < 0) {
        host_log("Open", "%s: %s", dev, strerror(errno));
        close(wav);
        if (g_log != NULL) {
            fclose(g_log);
        }
        return 1;
    }

    signal(SIGINT, on_stop);
    signal(SIGTERM, on_stop);

    host_log("Open", "%s", dev);
    host_log("Wav", "%s %lu samples (%.3f s)", wav_path,
             (unsigned long)wav_n, (double)wav_n / (double)GT_PCM_RATE_HZ);
    {
        uint8_t on = KISS_HW_INJECT_ON;

        if (gt_kiss_write(fd, KISS_CMD_SETHW, &on, 1) != 0) {
            host_log("Kiss", "TX SET-HW INJECT_ON 0x23 failed");
            close(wav);
            gt_tty_close(fd);
            if (g_log != NULL) {
                fclose(g_log);
            }
            return 1;
        }
        host_log("Kiss", "TX SET-HW INJECT_ON 0x23");
    }

    kiss_rx_init(&rx);
    clock_gettime(CLOCK_MONOTONIC, &due);
    while (!g_stop || drain_ms < 500) {
        fd_set rfds;
        struct timeval tv;
        unsigned char buf[4096];
        ssize_t nr;
        int sl;
        int want_send = 0;
        int off;

        if (!g_stop && sent < wav_n) {
            if (preroll < PREROLL) {
                want_send = 1;
            } else {
                struct timespec now;

                clock_gettime(CLOCK_MONOTONIC, &now);
                if (now.tv_sec > due.tv_sec ||
                    (now.tv_sec == due.tv_sec && now.tv_nsec >= due.tv_nsec)) {
                    want_send = 1;
                }
            }
        }
        if (want_send) {
            int16_t smp[KISS_PCM_CHUNK];
            uint32_t left = wav_n - sent;
            int ns = (left > KISS_PCM_CHUNK) ? KISS_PCM_CHUNK : (int)left;
            ssize_t nb = read(wav, smp, (size_t)ns * 2u);

            if (nb != (ssize_t)ns * 2) {
                host_log("Wav", "short read");
                rc = 1;
                g_stop = 1;
            } else if (gt_pcm_write(fd, seq, smp, ns) != 0) {
                host_log("Kiss", "TX PCM seq=%u failed", (unsigned)seq);
                rc = 1;
                g_stop = 1;
            } else {
                if (sent == 0u) {
                    host_log("TX", "first frame seq=%u n=%d", (unsigned)seq, ns);
                }
                sent += (uint32_t)ns;
                seq++;
                preroll++;
                timespec_add_ns(&due, (long)ns * 1000000000L / (long)GT_PCM_RATE_HZ);
                if (sent - last_rep >= GT_PCM_RATE_HZ) {
                    host_log("TX", "%lu samples", (unsigned long)sent);
                    last_rep = sent;
                }
            }
        }
        if (!g_stop && sent >= wav_n && !off_sent) {
            uint8_t offb = KISS_HW_INJECT_OFF;

            if (gt_kiss_write(fd, KISS_CMD_SETHW, &offb, 1) != 0) {
                host_log("Kiss", "TX SET-HW INJECT_OFF 0x22 failed");
            } else {
                host_log("Kiss", "TX SET-HW INJECT_OFF 0x22");
            }
            off_sent = 1;
            g_stop = 1;
        }

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = want_send ? 0 : 5000;
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
            drain_ms += 5;
            if (!off_sent) {
                uint8_t offb = KISS_HW_INJECT_OFF;

                if (gt_kiss_write(fd, KISS_CMD_SETHW, &offb, 1) != 0) {
                    host_log("Kiss", "TX SET-HW INJECT_OFF 0x22 failed");
                } else {
                    host_log("Kiss", "TX SET-HW INJECT_OFF 0x22");
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

            if (!gt_kiss_feed(&rx, buf + off, (int)nr - off, &used)) {
                break;
            }
            off += used;
            if (gt_kiss_log_line(&rx, line, (int)sizeof(line))) {
                if (g_log != NULL) {
                    fputs(line, g_log);
                    fputc('\n', g_log);
                    fflush(g_log);
                }
                if (strstr(line, "[Inject] abort") != NULL) {
                    host_log("Inject", "Pico abort");
                    rc = 4;
                    g_stop = 1;
                } else if (strstr(line, "[Ax25]") != NULL) {
                    const char *p = strstr(line, "len=");
                    unsigned alen = 0;
                    int fcs = 0;

                    if (p != NULL) {
                        sscanf(p, "len=%u fcs=%d", &alen, &fcs);
                    }
                    ax25_n++;
                    if (fcs) {
                        fcs_ok++;
                    } else {
                        fcs_fail++;
                    }
                    host_log("AX25", "len=%u fcs=%d", alen, fcs);
                }
            } else if ((rx.buf[0] & KISS_CMD_MASK) == KISS_CMD_DATA &&
                       rx.frame_len > 1) {
                host_log("Kiss", "RX DATA len=%d", rx.frame_len - 1);
            }
        }
    }

    close(wav);
    gt_tty_close(fd);
    host_log("Report", "tx=%lu ax25=%lu fcs_ok=%lu fcs_fail=%lu",
             (unsigned long)sent, (unsigned long)ax25_n,
             (unsigned long)fcs_ok, (unsigned long)fcs_fail);
    if (g_log != NULL) {
        fclose(g_log);
    }
    return rc;
}
