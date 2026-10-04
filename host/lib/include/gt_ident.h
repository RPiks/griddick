#ifndef GT_IDENT_H
#define GT_IDENT_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_ident.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void gt_print_notice(FILE *fp, const char *prog);
void gt_print_license(FILE *fp, const char *prog);

#ifdef __cplusplus
}
#endif

#endif
