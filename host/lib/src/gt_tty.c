/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_tty.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gt_tty.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

int gt_tty_wait(const char *path, int wait_sec)
{
    int i;

    if (wait_sec < 0) {
        wait_sec = 0;
    }
    for (i = 0; i <= wait_sec * 4; i++) {
        if (access(path, F_OK) == 0) {
            return 0;
        }
        if (i == wait_sec * 4) {
            break;
        }
        usleep(250000);
    }
    return -1;
}

int gt_tty_open(const char *path)
{
    int fd;
    struct termios tio;

    fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        return -1;
    }
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        return -1;
    }
    cfmakeraw(&tio);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void gt_tty_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}
