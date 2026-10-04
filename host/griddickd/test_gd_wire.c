#define _POSIX_C_SOURCE 200809L
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * test_gd_wire.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gd_conf.h"
#include "gd_wire.h"
#include "gt_ax25.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_fail;

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        g_fail = 1;
    }
}

static void write_conf(const char *path, const char *body)
{
    FILE *fp = fopen(path, "w");

    if (fp == NULL) {
        perror(path);
        g_fail = 1;
        return;
    }
    fputs(body, fp);
    fclose(fp);
}

int main(void)
{
    uint8_t buf[256];
    uint8_t info[16];
    const uint8_t *pl;
    int n;
    int ninfo;
    uint16_t sp;
    uint16_t dp;
    struct gd_conf c;
    char err[128];
    const char *path = "/tmp/griddickd_test.conf";
    struct gt_ax25_addr a;
    struct gt_ax25_addr b;

    ninfo = gd_wire_pack(buf, (int)sizeof(buf), 1234, 80,
                         (const uint8_t *)"hi", 2, NULL);
    expect(ninfo == 7, "pack no-auth len");
    expect(buf[0] == GD_WIRE_VER, "ver");
    expect(gd_wire_unpack(buf, ninfo, NULL, &sp, &dp, &pl, &n) == 0,
           "unpack no-auth");
    expect(sp == 1234 && dp == 80 && n == 2 && memcmp(pl, "hi", 2) == 0,
           "ports+payload");

    ninfo = gd_wire_pack(buf, (int)sizeof(buf), 0, 0, NULL, 0, NULL);
    expect(ninfo == 5, "empty payload");
    expect(gd_wire_unpack(buf, ninfo, NULL, &sp, &dp, &pl, &n) == 0,
           "unpack empty");
    expect(sp == 0 && dp == 0 && n == 0, "port 0");

    ninfo = gd_wire_pack(buf, (int)sizeof(buf), 9, 7,
                         (const uint8_t *)"xyz", 3, "s3cret");
    expect(ninfo == 10, "pack auth len");
    expect(gd_wire_unpack(buf, ninfo, "s3cret", &sp, &dp, &pl, &n) == 0,
           "unpack auth");
    expect(n == 3 && memcmp(pl, "xyz", 3) == 0, "auth payload");
    expect(gd_wire_unpack(buf, ninfo, "wrong", &sp, &dp, &pl, &n) == -2,
           "bad password");

    memcpy(info, buf, (size_t)ninfo);
    info[0] = 0x02;
    expect(gd_wire_unpack(info, ninfo, "s3cret", &sp, &dp, &pl, &n) == -1,
           "bad ver");

    expect(gt_ax25_parse_call("JBLADE", &a) == 0, "parse JBLADE");
    expect(gt_ax25_parse_call("JBLADE-0", &b) == 0, "parse JBLADE-0");
    expect(gt_ax25_addr_eq(&a, &b), "JBLADE == JBLADE-0");

    write_conf(path,
               "Device /dev/ttyACM0\n"
               "TunName gt0\n"
               "TunAddr 10.64.0.1/24\n"
               "Call JBLADE-8\n"
               "Peer N0CALL-1 10.64.0.2\n"
               "Peer W1XYZ-7 10.64.0.3 s3cret\n");
    expect(gd_conf_load(path, &c, err, (int)sizeof(err)) == 0, "conf ok");
    expect(c.n_peer == 2, "2 peers");
    expect(c.peer[0].password[0] == '\0', "peer0 no pass");
    expect(strcmp(c.peer[1].password, "s3cret") == 0, "peer1 pass");

    write_conf(path,
               "Device /dev/ttyACM0\n"
               "TunAddr 10.64.0.1/24\n"
               "Call JBLADE-8\n"
               "Peer AA1AA 10.64.0.2\n"
               "Peer AA1AA-0 10.64.0.3\n");
    expect(gd_conf_load(path, &c, err, (int)sizeof(err)) != 0,
           "duplicate call SSID 0");

    write_conf(path,
               "Device /dev/ttyACM0\n"
               "TunAddr 10.64.0.1/24\n"
               "Call JBLADE-8\n"
               "Peer AA1AA-1 10.64.0.2\n"
               "Peer BB1BB-1 10.64.0.2\n");
    expect(gd_conf_load(path, &c, err, (int)sizeof(err)) != 0,
           "duplicate IP");

    unlink(path);
    if (g_fail) {
        return 1;
    }
    puts("ok");
    return 0;
}
