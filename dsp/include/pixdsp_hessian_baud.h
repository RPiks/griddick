#ifndef PIXDSP_HESSIAN_BAUD_H
#define PIXDSP_HESSIAN_BAUD_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * pixdsp_hessian_baud.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *
 * This file is proprietary to Roman Piksaykin. It is not licensed under
 * the GNU LGPL or any other open-source licence. You may include this
 * header when building Griddick TNC firmware. The corresponding
 * implementation is supplied only as a prebuilt Pico object archive.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pixdsp_hessian_baud {
    uint64_t _opaque[64];
} pixdsp_hessian_baud_t;

void pixdsp_hessian_baud_reset(pixdsp_hessian_baud_t *s);
int32_t pixdsp_hessian_baud_process(pixdsp_hessian_baud_t *s, int32_t input);

#ifdef __cplusplus
}
#endif

#endif /* PIXDSP_HESSIAN_BAUD_H */
