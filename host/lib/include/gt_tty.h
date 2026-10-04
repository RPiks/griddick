#ifndef GT_TTY_H
#define GT_TTY_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_tty.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#ifdef __cplusplus
extern "C" {
#endif

int gt_tty_wait(const char *path, int wait_sec);
int gt_tty_open(const char *path);
void gt_tty_close(int fd);

#ifdef __cplusplus
}
#endif

#endif
