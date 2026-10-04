/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gd_tun.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#define _POSIX_C_SOURCE 200809L

#include "gd_tun.h"

#include <fcntl.h>
#include <stdio.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

int gd_tun_open(const char *name, uint32_t addr, uint32_t mask, int mtu)
{
    int fd;
    int s;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    if (name == NULL || name[0] == '\0' || mtu < 28) {
        return -1;
    }
    fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        return -1;
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);
    if (ioctl(fd, TUNSETIFF, &ifr) != 0) {
        close(fd);
        return -1;
    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        close(fd);
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);
    sin = (struct sockaddr_in *)&ifr.ifr_addr;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = addr;
    if (ioctl(s, SIOCSIFADDR, &ifr) != 0) {
        close(s);
        close(fd);
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);
    sin = (struct sockaddr_in *)&ifr.ifr_netmask;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = mask;
    if (ioctl(s, SIOCSIFNETMASK, &ifr) != 0) {
        close(s);
        close(fd);
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);
    ifr.ifr_mtu = mtu;
    if (ioctl(s, SIOCSIFMTU, &ifr) != 0) {
        close(s);
        close(fd);
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);
    if (ioctl(s, SIOCGIFFLAGS, &ifr) != 0) {
        close(s);
        close(fd);
        return -1;
    }
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(s, SIOCSIFFLAGS, &ifr) != 0) {
        close(s);
        close(fd);
        return -1;
    }
    close(s);
    return fd;
}

void gd_tun_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}
