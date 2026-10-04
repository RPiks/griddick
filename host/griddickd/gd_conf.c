/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gd_conf.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#define _POSIX_C_SOURCE 200809L

#include "gd_conf.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_err(char *err, int err_max, int line, const char *msg)
{
    if (err == NULL || err_max < 1) {
        return;
    }
    if (line > 0) {
        snprintf(err, (size_t)err_max, "line %d: %s", line, msg);
    } else {
        snprintf(err, (size_t)err_max, "%s", msg);
    }
}

static char *skip_ws(char *s)
{
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

static void rstrip(char *s)
{
    int n = (int)strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
}

static int parse_ipv4(const char *s, uint32_t *out)
{
    struct in_addr a;

    if (inet_pton(AF_INET, s, &a) != 1) {
        return -1;
    }
    *out = a.s_addr;
    return 0;
}

static int parse_tunaddr(const char *s, struct gd_conf *c)
{
    char ip[32];
    const char *slash;
    size_t n;
    long pfx;
    char *end;

    slash = strchr(s, '/');
    if (slash == NULL) {
        return -1;
    }
    n = (size_t)(slash - s);
    if (n < 7 || n >= sizeof(ip)) {
        return -1;
    }
    memcpy(ip, s, n);
    ip[n] = '\0';
    if (parse_ipv4(ip, &c->tun_ip) != 0) {
        return -1;
    }
    pfx = strtol(slash + 1, &end, 10);
    if (end == slash + 1 || *end != '\0' || pfx < 0 || pfx > 32) {
        return -1;
    }
    c->tun_prefix = (int)pfx;
    if (pfx == 0) {
        c->tun_mask = 0;
    } else {
        c->tun_mask = htonl((uint32_t)(0xffffffffu << (32 - (int)pfx)));
    }
    return 0;
}

int gd_conf_load(const char *path, struct gd_conf *c, char *err, int err_max)
{
    FILE *fp;
    char line[512];
    int lineno = 0;
    int i;
    int have_device = 0;
    int have_tun_name = 0;
    int have_tun_addr = 0;
    int have_call = 0;

    if (path == NULL || c == NULL) {
        set_err(err, err_max, 0, "bad args");
        return -1;
    }
    memset(c, 0, sizeof(*c));
    snprintf(c->tun_name, sizeof(c->tun_name), "gt0");
    c->device_wait = GD_WAIT_DEFAULT;
    have_tun_name = 1;

    fp = fopen(path, "r");
    if (fp == NULL) {
        set_err(err, err_max, 0, "cannot open config");
        return -1;
    }
    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        char *p;
        char *key;
        char *arg1;
        char *arg2;
        char *arg3;

        lineno++;
        p = skip_ws(line);
        rstrip(p);
        if (*p == '\0' || *p == '#') {
            continue;
        }
        key = p;
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p != '\0') {
            *p++ = '\0';
            p = skip_ws(p);
        }
        arg1 = *p != '\0' ? p : NULL;
        arg2 = NULL;
        arg3 = NULL;
        if (arg1 != NULL) {
            char *q = arg1;

            while (*q != '\0' && *q != ' ' && *q != '\t') {
                q++;
            }
            if (*q != '\0') {
                *q++ = '\0';
                q = skip_ws(q);
                if (*q != '\0') {
                    arg2 = q;
                    while (*q != '\0' && *q != ' ' && *q != '\t') {
                        q++;
                    }
                    if (*q != '\0') {
                        *q++ = '\0';
                        q = skip_ws(q);
                        if (*q != '\0') {
                            arg3 = q;
                            while (*q != '\0' && *q != ' ' && *q != '\t') {
                                q++;
                            }
                            if (*q != '\0') {
                                *q = '\0';
                            }
                        }
                    }
                }
            }
        }

        if (strcmp(key, "Device") == 0) {
            if (arg1 == NULL || arg2 != NULL) {
                set_err(err, err_max, lineno, "Device PATH");
                fclose(fp);
                return -1;
            }
            if (strlen(arg1) >= sizeof(c->device)) {
                set_err(err, err_max, lineno, "Device path too long");
                fclose(fp);
                return -1;
            }
            snprintf(c->device, sizeof(c->device), "%s", arg1);
            have_device = 1;
        } else if (strcmp(key, "TunName") == 0) {
            if (arg1 == NULL || arg2 != NULL) {
                set_err(err, err_max, lineno, "TunName NAME");
                fclose(fp);
                return -1;
            }
            if (strlen(arg1) < 1 || strlen(arg1) > 15) {
                set_err(err, err_max, lineno, "TunName length");
                fclose(fp);
                return -1;
            }
            snprintf(c->tun_name, sizeof(c->tun_name), "%s", arg1);
            have_tun_name = 1;
        } else if (strcmp(key, "TunAddr") == 0) {
            if (arg1 == NULL || arg2 != NULL) {
                set_err(err, err_max, lineno, "TunAddr A.B.C.D/P");
                fclose(fp);
                return -1;
            }
            if (parse_tunaddr(arg1, c) != 0) {
                set_err(err, err_max, lineno, "bad TunAddr");
                fclose(fp);
                return -1;
            }
            have_tun_addr = 1;
        } else if (strcmp(key, "Call") == 0) {
            if (arg1 == NULL || arg2 != NULL) {
                set_err(err, err_max, lineno, "Call CALL[-N]");
                fclose(fp);
                return -1;
            }
            if (gt_ax25_parse_call(arg1, &c->local_call) != 0) {
                set_err(err, err_max, lineno, "bad Call");
                fclose(fp);
                return -1;
            }
            have_call = 1;
        } else if (strcmp(key, "DeviceWait") == 0) {
            char *end;
            long v;

            if (arg1 == NULL || arg2 != NULL) {
                set_err(err, err_max, lineno, "DeviceWait SEC");
                fclose(fp);
                return -1;
            }
            v = strtol(arg1, &end, 10);
            if (end == arg1 || *end != '\0' || v < 0 || v > 300) {
                set_err(err, err_max, lineno, "bad DeviceWait");
                fclose(fp);
                return -1;
            }
            c->device_wait = (int)v;
        } else if (strcmp(key, "Peer") == 0) {
            struct gd_peer *pr;

            if (arg1 == NULL || arg2 == NULL) {
                set_err(err, err_max, lineno, "Peer CALL IP [PASSWORD]");
                fclose(fp);
                return -1;
            }
            if (c->n_peer >= GD_PEER_MAX) {
                set_err(err, err_max, lineno, "too many Peer lines");
                fclose(fp);
                return -1;
            }
            pr = &c->peer[c->n_peer];
            memset(pr, 0, sizeof(*pr));
            if (gt_ax25_parse_call(arg1, &pr->call) != 0) {
                set_err(err, err_max, lineno, "bad Peer call");
                fclose(fp);
                return -1;
            }
            if (parse_ipv4(arg2, &pr->ip) != 0) {
                set_err(err, err_max, lineno, "bad Peer IP");
                fclose(fp);
                return -1;
            }
            if (arg3 != NULL) {
                if (strlen(arg3) > GD_PASS_MAX) {
                    set_err(err, err_max, lineno, "password too long");
                    fclose(fp);
                    return -1;
                }
                snprintf(pr->password, sizeof(pr->password), "%s", arg3);
            }
            c->n_peer++;
        } else {
            set_err(err, err_max, lineno, "unknown keyword");
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);

    if (!have_device) {
        set_err(err, err_max, 0, "Device is required");
        return -1;
    }
    if (!have_tun_name) {
        set_err(err, err_max, 0, "TunName is required");
        return -1;
    }
    if (!have_tun_addr) {
        set_err(err, err_max, 0, "TunAddr is required");
        return -1;
    }
    if (!have_call) {
        set_err(err, err_max, 0, "Call is required");
        return -1;
    }

    for (i = 0; i < c->n_peer; i++) {
        int j;

        if (gt_ax25_addr_eq(&c->peer[i].call, &c->local_call)) {
            set_err(err, err_max, 0, "Peer call matches Call");
            return -1;
        }
        if (c->peer[i].ip == c->tun_ip) {
            set_err(err, err_max, 0, "Peer IP matches TunAddr");
            return -1;
        }
        for (j = i + 1; j < c->n_peer; j++) {
            if (gt_ax25_addr_eq(&c->peer[i].call, &c->peer[j].call)) {
                set_err(err, err_max, 0, "duplicate Peer call");
                return -1;
            }
            if (c->peer[i].ip == c->peer[j].ip) {
                set_err(err, err_max, 0, "duplicate Peer IP");
                return -1;
            }
        }
    }
    return 0;
}

struct gd_peer *gd_conf_peer_by_ip(struct gd_conf *c, uint32_t ip)
{
    int i;

    if (c == NULL) {
        return NULL;
    }
    for (i = 0; i < c->n_peer; i++) {
        if (c->peer[i].ip == ip) {
            return &c->peer[i];
        }
    }
    return NULL;
}

struct gd_peer *gd_conf_peer_by_call(struct gd_conf *c,
                                     const struct gt_ax25_addr *call)
{
    int i;

    if (c == NULL || call == NULL) {
        return NULL;
    }
    for (i = 0; i < c->n_peer; i++) {
        if (gt_ax25_addr_eq(&c->peer[i].call, call)) {
            return &c->peer[i];
        }
    }
    return NULL;
}
