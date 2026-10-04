#ifndef GT_H
#define GT_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt.h - This file is part of Griddick TNC.
 *
 * DESCRIPTION
 *     Umbrella header for libgtnc (host KISS / AX.25 / PCM helpers).
 *
 * PLATFORM
 *     Linux host
 *
 * PROJECT PAGE
 *     https://github.com/RPiks/griddick
 *
 * LICENCE
 *
 * Griddick TNC is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * See the GNU Lesser General Public License for more details.
 * https://www.gnu.org/licenses/lgpl-2.1.html
 */

#define GT_VERSION "0.9.7"

#ifdef __cplusplus
extern "C" {
#endif

#include "gt_ident.h"
#include "gt_tty.h"
#include "gt_kiss.h"
#include "gt_util.h"
#include "gt_ax25.h"
#include "gt_aprs.h"
#include "gt_time.h"
#include "gt_cfg.h"
#include "gt_pcm.h"
#include "gt_link.h"

#ifdef __cplusplus
}
#endif

#endif /* GT_H */
