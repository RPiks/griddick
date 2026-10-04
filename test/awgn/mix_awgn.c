/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 *
 * mix_awgn — add Gaussian noise to a 48 kHz mono PCM16 WAV at a target SNR.
 *
 *   mix_awgn --snr DB --seed S in.wav out.wav
 *
 * SNR is 20*log10(RMS_bp(packet) / RMS_bp(noise)), BP = 300..3000 Hz.
 * Silence is filled at the same noise amplitude. --snr 99 copies.
 *
 * LICENCE: GNU Lesser General Public License v2.1 or later.
 * https://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 48000

struct wav {
    int16_t *x;
    int n;
};

static uint32_t xs32(uint32_t *st)
{
    uint32_t x = *st;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *st = x ? x : 1u;
    return *st;
}

/* N(0,1) via Box-Muller. */
static double gauss(uint32_t *st)
{
    double u = ((xs32(st) >> 8) + 1.0) / 16777217.0;
    double v = ((xs32(st) >> 8) + 1.0) / 16777217.0;

    return sqrt(-2.0 * log(u)) * cos(2.0 * M_PI * v);
}

static int read_wav(const char *path, struct wav *w)
{
    unsigned char hdr[44];
    FILE *fp;
    unsigned rate, bits, ch, data;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fread(hdr, 1, 44, fp) != 44) {
        fclose(fp);
        return -1;
    }
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        fclose(fp);
        return -1;
    }
    ch = hdr[22] | ((unsigned)hdr[23] << 8);
    rate = hdr[24] | ((unsigned)hdr[25] << 8) | ((unsigned)hdr[26] << 16) |
           ((unsigned)hdr[27] << 24);
    bits = hdr[34] | ((unsigned)hdr[35] << 8);
    data = hdr[40] | ((unsigned)hdr[41] << 8) | ((unsigned)hdr[42] << 16) |
           ((unsigned)hdr[43] << 24);
    if (ch != 1 || rate != RATE || bits != 16) {
        fclose(fp);
        return -2;
    }
    w->n = (int)(data / 2u);
    if (w->n < RATE / 10) {
        fclose(fp);
        return -1;
    }
    w->x = malloc((size_t)w->n * sizeof(int16_t));
    if (w->x == NULL) {
        fclose(fp);
        return -1;
    }
    if ((int)fread(w->x, 2, (size_t)w->n, fp) != w->n) {
        free(w->x);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int write_wav(const char *path, const struct wav *w)
{
    unsigned char hdr[44];
    FILE *fp;
    unsigned data = (unsigned)w->n * 2u;
    unsigned riff = 36u + data;
    unsigned br = RATE * 2u;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "RIFF", 4);
    hdr[4] = (unsigned char)riff;
    hdr[5] = (unsigned char)(riff >> 8);
    hdr[6] = (unsigned char)(riff >> 16);
    hdr[7] = (unsigned char)(riff >> 24);
    memcpy(hdr + 8, "WAVEfmt ", 8);
    hdr[16] = 16;
    hdr[20] = 1;
    hdr[22] = 1;
    hdr[24] = (unsigned char)RATE;
    hdr[25] = (unsigned char)(RATE >> 8);
    hdr[26] = (unsigned char)(RATE >> 16);
    hdr[27] = (unsigned char)(RATE >> 24);
    hdr[28] = (unsigned char)br;
    hdr[29] = (unsigned char)(br >> 8);
    hdr[30] = (unsigned char)(br >> 16);
    hdr[31] = (unsigned char)(br >> 24);
    hdr[32] = 2;
    hdr[34] = 16;
    memcpy(hdr + 36, "data", 4);
    hdr[40] = (unsigned char)data;
    hdr[41] = (unsigned char)(data >> 8);
    hdr[42] = (unsigned char)(data >> 16);
    hdr[43] = (unsigned char)(data >> 24);
    fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }
    if (fwrite(hdr, 1, 44, fp) != 44 ||
        fwrite(w->x, 2, (size_t)w->n, fp) != (size_t)w->n) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

struct bq {
    double b0, b1, b2, a1, a2;
    double z1, z2;
};

static void rbj_lp(struct bq *f, double hz)
{
    double w = 2.0 * M_PI * hz / (double)RATE;
    double cw = cos(w);
    double sw = sin(w);
    double al = sw / (2.0 * 0.707);
    double a0 = 1.0 + al;

    f->b0 = ((1.0 - cw) * 0.5) / a0;
    f->b1 = (1.0 - cw) / a0;
    f->b2 = f->b0;
    f->a1 = (-2.0 * cw) / a0;
    f->a2 = (1.0 - al) / a0;
    f->z1 = 0.0;
    f->z2 = 0.0;
}

static void rbj_hp(struct bq *f, double hz)
{
    double w = 2.0 * M_PI * hz / (double)RATE;
    double cw = cos(w);
    double sw = sin(w);
    double al = sw / (2.0 * 0.707);
    double a0 = 1.0 + al;

    f->b0 = ((1.0 + cw) * 0.5) / a0;
    f->b1 = (-(1.0 + cw)) / a0;
    f->b2 = f->b0;
    f->a1 = (-2.0 * cw) / a0;
    f->a2 = (1.0 - al) / a0;
    f->z1 = 0.0;
    f->z2 = 0.0;
}

static double bq_step(struct bq *f, double x)
{
    double y = f->b0 * x + f->z1;

    f->z1 = f->b1 * x - f->a1 * y + f->z2;
    f->z2 = f->b2 * x - f->a2 * y;
    return y;
}

static double rms_bp_i16(const int16_t *x, int a, int b)
{
    struct bq hp;
    struct bq lp;
    double acc = 0.0;
    int n = 0;
    int i;

    rbj_hp(&hp, 300.0);
    rbj_lp(&lp, 3000.0);
    for (i = a; i < b; i++) {
        double y = bq_step(&lp, bq_step(&hp, (double)x[i]));

        acc += y * y;
        n++;
    }
    if (n < 1) {
        return 0.0;
    }
    return sqrt(acc / (double)n);
}

static double rms_bp_f(const double *x, int a, int b)
{
    struct bq hp;
    struct bq lp;
    double acc = 0.0;
    int n = 0;
    int i;

    rbj_hp(&hp, 300.0);
    rbj_lp(&lp, 3000.0);
    for (i = a; i < b; i++) {
        double y = bq_step(&lp, bq_step(&hp, x[i]));

        acc += y * y;
        n++;
    }
    if (n < 1) {
        return 0.0;
    }
    return sqrt(acc / (double)n);
}

static void find_pkt(const int16_t *x, int n, int *a, int *b)
{
    int hop = RATE / 50;
    int i;
    double floor_e = 0.0;
    double thresh;
    int on = 0;
    int aa = 0;
    int bb = n;

    *a = 0;
    *b = n;
    if (n < hop * 8) {
        return;
    }
    {
        double hop_e[64];
        int nh = 0;

        for (i = 0; i + hop <= n && nh < 40; i += hop) {
            double e = 0.0;
            int j;

            for (j = 0; j < hop; j++) {
                e += (double)x[i + j] * (double)x[i + j];
            }
            hop_e[nh++] = sqrt(e / (double)hop);
        }
        {
            int p, q;

            for (p = 1; p < nh; p++) {
                double t = hop_e[p];

                for (q = p; q > 0 && hop_e[q - 1] > t; q--) {
                    hop_e[q] = hop_e[q - 1];
                }
                hop_e[q] = t;
            }
        }
        floor_e = (nh > 0) ? hop_e[nh / 5] : 1.0;
    }
    thresh = floor_e * 4.0 + 50.0;
    for (i = 0; i + hop <= n; i += hop) {
        double e = 0.0;
        int j;

        for (j = 0; j < hop; j++) {
            e += (double)x[i + j] * (double)x[i + j];
        }
        e = sqrt(e / (double)hop);
        if (!on && e >= thresh) {
            on = 1;
            aa = i;
        } else if (on && e < thresh) {
            bb = i + hop;
            break;
        }
    }
    if (on) {
        if (bb <= aa) {
            bb = n;
        }
        *a = aa;
        *b = bb;
    }
}

static void usage(void)
{
    fputs("Usage: mix_awgn --snr DB --seed S in.wav out.wav\n"
          "  DB is in-band SNR (300..3000 Hz). 99 = copy.\n",
          stderr);
}

int main(int argc, char **argv)
{
    double snr_req = -1.0;
    unsigned seed = 1;
    const char *in_path = NULL;
    const char *out_path = NULL;
    struct wav w;
    double *nz;
    uint32_t st;
    int i;
    int clip = 0;
    int pa, pb;
    double prms;
    double nrms;
    double scale;
    double snr;
    int ai;

    for (ai = 1; ai < argc; ai++) {
        if (strcmp(argv[ai], "--snr") == 0 && ai + 1 < argc) {
            snr_req = strtod(argv[++ai], NULL);
        } else if (strcmp(argv[ai], "--seed") == 0 && ai + 1 < argc) {
            seed = (unsigned)strtoul(argv[++ai], NULL, 10);
        } else if (strcmp(argv[ai], "-h") == 0) {
            usage();
            return 0;
        } else if (in_path == NULL) {
            in_path = argv[ai];
        } else if (out_path == NULL) {
            out_path = argv[ai];
        } else {
            usage();
            return 2;
        }
    }
    if (snr_req < 0.0 || snr_req > 99.0 || in_path == NULL || out_path == NULL) {
        usage();
        return 2;
    }
    if (read_wav(in_path, &w) != 0) {
        fprintf(stderr, "mix_awgn: bad wav %s (need 48k mono PCM16)\n", in_path);
        return 1;
    }
    find_pkt(w.x, w.n, &pa, &pb);
    prms = rms_bp_i16(w.x, pa, pb);
    nz = calloc((size_t)w.n, sizeof(double));
    if (nz == NULL) {
        return 1;
    }
    st = seed ? seed : 1u;
    if (snr_req >= 90.0 || prms < 1.0) {
        scale = 0.0;
    } else {
        for (i = 0; i < w.n; i++) {
            nz[i] = gauss(&st);
        }
        nrms = rms_bp_f(nz, pa, pb);
        if (nrms < 1e-9) {
            scale = 0.0;
        } else {
            scale = prms / (nrms * pow(10.0, snr_req / 20.0));
        }
    }
    for (i = 0; i < w.n; i++) {
        double y = (double)w.x[i] + scale * nz[i];

        if (y > 32767.0) {
            y = 32767.0;
            clip++;
        } else if (y < -32767.0) {
            y = -32767.0;
            clip++;
        }
        w.x[i] = (int16_t)lrint(y);
    }
    nrms = rms_bp_i16(w.x, 0, w.n);
    /* Noise-only RMS from the scaled sequence, packet window. */
    {
        double acc = 0.0;
        struct bq hp, lp;
        int n = 0;

        rbj_hp(&hp, 300.0);
        rbj_lp(&lp, 3000.0);
        for (i = pa; i < pb; i++) {
            double y = bq_step(&lp, bq_step(&hp, scale * nz[i]));

            acc += y * y;
            n++;
        }
        nrms = (n > 0) ? sqrt(acc / (double)n) : 0.0;
    }
    if (nrms < 1e-9 || prms < 1.0) {
        snr = 99.0;
    } else {
        snr = 20.0 * log10(prms / nrms);
    }
    if (write_wav(out_path, &w) != 0) {
        fprintf(stderr, "mix_awgn: write %s\n", out_path);
        return 1;
    }
    printf("snr_req=%.2f snr_db=%.2f seed=%u clip=%d pkt_rms=%.2f noise_rms=%.2f\n",
           snr_req, snr, seed, clip, prms, nrms);
    free(nz);
    free(w.x);
    return 0;
}
