/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * fft_r2.cpp - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "fft_r2.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void fft_radix2(float *re, float *im, int n)
{
    int i;
    int j = 0;
    int k;

    if (n < 2) {
        return;
    }
    for (i = 0; i < n; i++) {
        if (i < j) {
            float tr = re[i];
            float ti = im[i];

            re[i] = re[j];
            im[i] = im[j];
            re[j] = tr;
            im[j] = ti;
        }
        k = n >> 1;
        while (k > 0 && j >= k) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }
    for (int len = 2; len <= n; len <<= 1) {
        const float ang = -2.0f * static_cast<float>(M_PI) / static_cast<float>(len);
        const float wr = std::cos(ang);
        const float wi = std::sin(ang);
        int t;

        for (t = 0; t < n; t += len) {
            float cr = 1.0f;
            float ci = 0.0f;
            int u;

            for (u = 0; u < len / 2; u++) {
                const int i0 = t + u;
                const int i1 = i0 + len / 2;
                const float tr = cr * re[i1] - ci * im[i1];
                const float ti = cr * im[i1] + ci * re[i1];

                re[i1] = re[i0] - tr;
                im[i1] = im[i0] - ti;
                re[i0] += tr;
                im[i0] += ti;
                const float nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}
