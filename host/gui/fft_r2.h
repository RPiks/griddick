#ifndef FFT_R2_H
#define FFT_R2_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * fft_r2.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

/* In-place radix-2 FFT. n must be a power of two. */
void fft_radix2(float *re, float *im, int n);

#endif
