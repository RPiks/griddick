#ifndef GT_PCM_H
#define GT_PCM_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_pcm.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "kiss.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GT_PCM_RATE_HZ  48000u

void gt_wav_hdr_48k_mono(uint8_t *h, uint32_t n_samp);
int gt_wav_open_48k_mono(const char *path, int *out_fd, uint32_t *n_samp);

/* pay = seq + LE PCM16 + CRC16. Returns encoded payload length. */
int gt_pcm_pack(uint8_t *pay, int max, uint8_t seq, const int16_t *s, int ns);
int gt_pcm_write(int fd, uint8_t seq, const int16_t *s, int ns);

/*
 * Parse a completed KISS frame (cmd byte included).
 * Returns 1 if PCM with good layout, 0 if not PCM.
 * *crc_ok / *seq / *samples / *n_samp set when return is 1.
 */
int gt_pcm_parse(const uint8_t *frame, int len,
                 uint8_t *seq, const uint8_t **samples, int *n_samp,
                 int *crc_ok);

#ifdef __cplusplus
}
#endif

#endif
