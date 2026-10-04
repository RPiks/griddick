/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_ax25.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#define _POSIX_C_SOURCE 200809L

#include "gt_ax25.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gt_ax25_parse_call(const char *in, struct gt_ax25_addr *out)
{
    char tmp[16];
    char *dash;
    int i;
    int n;
    int ssid = 0;

    if (in == NULL || in[0] == '\0' || strlen(in) > 10) {
        return -1;
    }
    snprintf(tmp, sizeof(tmp), "%s", in);
    dash = strrchr(tmp, '-');
    if (dash != NULL) {
        char *end;
        long v;

        if (dash == tmp) {
            return -1;
        }
        v = strtol(dash + 1, &end, 10);
        if (end == dash + 1 || *end != '\0' || v < 0 || v > 15) {
            return -1;
        }
        ssid = (int)v;
        *dash = '\0';
    }
    n = (int)strlen(tmp);
    if (n < 1 || n > 6) {
        return -1;
    }
    memset(out->call, ' ', 6);
    out->call[6] = '\0';
    for (i = 0; i < n; i++) {
        char c = tmp[i];

        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 32);
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
            return -1;
        }
        out->call[i] = c;
    }
    out->ssid = ssid;
    return 0;
}

int gt_ax25_parse_path(const char *in, struct gt_ax25_addr *out, int *n_out)
{
    char buf[128];
    char *tok;
    char *save = NULL;
    int n = 0;

    if (in == NULL || in[0] == '\0') {
        *n_out = 0;
        return 0;
    }
    if (strlen(in) >= sizeof(buf)) {
        return -1;
    }
    memcpy(buf, in, strlen(in) + 1);
    for (tok = strtok_r(buf, ",", &save); tok != NULL; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') {
            tok++;
        }
        if (*tok == '\0') {
            continue;
        }
        if (n >= GT_AX25_DIGI_MAX) {
            return -1;
        }
        if (gt_ax25_parse_call(tok, &out[n]) != 0) {
            return -1;
        }
        n++;
    }
    *n_out = n;
    return 0;
}

void gt_ax25_fmt_call(char *out, size_t out_sz, const struct gt_ax25_addr *a)
{
    char call[7];
    int i;

    memcpy(call, a->call, 6);
    call[6] = '\0';
    for (i = 5; i >= 0 && call[i] == ' '; i--) {
        call[i] = '\0';
    }
    if (a->ssid != 0) {
        snprintf(out, out_sz, "%s-%d", call, a->ssid);
    } else {
        snprintf(out, out_sz, "%s", call);
    }
}

void gt_ax25_decode_call(const uint8_t *a, char *out, size_t out_sz)
{
    char call[7];
    int i;
    int n = 0;
    int ssid;

    for (i = 0; i < 6; i++) {
        char c = (char)((a[i] >> 1) & 0x7f);

        if (c == ' ' || c == '\0') {
            break;
        }
        call[n++] = c;
    }
    call[n] = '\0';
    ssid = (a[6] >> 1) & 0x0f;
    if (ssid != 0) {
        snprintf(out, out_sz, "%s-%d", call, ssid);
    } else {
        snprintf(out, out_sz, "%s", call);
    }
}

static int put_addr(uint8_t *out, const struct gt_ax25_addr *a, int last,
                    int cbit)
{
    int i;

    for (i = 0; i < 6; i++) {
        out[i] = (uint8_t)((uint8_t)a->call[i] << 1);
    }
    out[6] = (uint8_t)(0x60u | ((a->ssid & 15) << 1) | (last ? 1u : 0u) |
                       (cbit ? 0x80u : 0u));
    return GT_AX25_ADDR_LEN;
}

int gt_ax25_addr_eq(const struct gt_ax25_addr *a, const struct gt_ax25_addr *b)
{
    int i;

    if (a == NULL || b == NULL || a->ssid != b->ssid) {
        return 0;
    }
    for (i = 0; i < 6; i++) {
        char ca = a->call[i] ? a->call[i] : ' ';
        char cb = b->call[i] ? b->call[i] : ' ';

        if (ca != cb) {
            return 0;
        }
    }
    return 1;
}

void gt_ax25_load_addr(const uint8_t *raw, struct gt_ax25_addr *out)
{
    int i;

    memset(out->call, ' ', 6);
    out->call[6] = '\0';
    for (i = 0; i < 6; i++) {
        char c = (char)((raw[i] >> 1) & 0x7f);

        if (c == '\0') {
            c = ' ';
        }
        out->call[i] = c;
    }
    out->ssid = (raw[6] >> 1) & 0x0f;
}

int gt_ax25_is_cmd(const uint8_t *pay, int n)
{
    if (pay == NULL || n < 7) {
        return 0;
    }
    return (pay[6] & 0x80u) != 0;
}

int gt_ax25_build_ex(uint8_t *out, int max,
                     const struct gt_ax25_addr *dst,
                     const struct gt_ax25_addr *src,
                     const struct gt_ax25_addr *digi, int n_digi,
                     uint8_t ctrl, uint8_t pid,
                     const uint8_t *info, int info_len,
                     int command, int with_pid)
{
    int n = 0;
    int i;
    int n_addr = 2 + n_digi;
    int need = n_addr * GT_AX25_ADDR_LEN + 1;

    if (info_len < 0 || info_len > GT_AX25_INFO_MAX) {
        return -1;
    }
    if (with_pid) {
        need += 1 + info_len;
    }
    if (need > max) {
        return -1;
    }
    n += put_addr(out + n, dst, 0, command ? 1 : 0);
    n += put_addr(out + n, src, n_digi == 0, command ? 0 : 1);
    for (i = 0; i < n_digi; i++) {
        n += put_addr(out + n, &digi[i], i + 1 == n_digi, 0);
    }
    out[n++] = ctrl;
    if (with_pid) {
        out[n++] = pid;
        if (info_len > 0 && info != NULL) {
            memcpy(out + n, info, (size_t)info_len);
            n += info_len;
        }
    }
    return n;
}

int gt_ax25_build(uint8_t *out, int max,
                  const struct gt_ax25_addr *dst,
                  const struct gt_ax25_addr *src,
                  const struct gt_ax25_addr *digi, int n_digi,
                  uint8_t ctrl, uint8_t pid,
                  const uint8_t *info, int info_len)
{
    return gt_ax25_build_ex(out, max, dst, src, digi, n_digi, ctrl, pid,
                            info, info_len, 0, 1);
}

int gt_ax25_parse_frame(const uint8_t *pay, int n,
                        struct gt_ax25_addr *dst,
                        struct gt_ax25_addr *src,
                        uint8_t *ctrl, uint8_t *pid, int *has_pid,
                        const uint8_t **info, int *info_len)
{
    int off;
    int ext;
    uint8_t c;

    if (pay == NULL || n < 15) {
        return -1;
    }
    gt_ax25_load_addr(pay, dst);
    gt_ax25_load_addr(pay + 7, src);
    ext = pay[13] & 0x01;
    off = 14;
    while (!ext && off + 7 <= n) {
        ext = pay[off + 6] & 0x01;
        off += 7;
    }
    if (off >= n) {
        return -1;
    }
    c = pay[off++];
    if (ctrl != NULL) {
        *ctrl = c;
    }
    if ((c & 0x01u) == 0u || (c & 0xefu) == 0x03u) {
        if (off >= n) {
            return -1;
        }
        if (pid != NULL) {
            *pid = pay[off];
        }
        if (has_pid != NULL) {
            *has_pid = 1;
        }
        off++;
    } else {
        if (pid != NULL) {
            *pid = 0;
        }
        if (has_pid != NULL) {
            *has_pid = 0;
        }
    }
    if (info != NULL) {
        *info = pay + off;
    }
    if (info_len != NULL) {
        *info_len = n - off;
    }
    return 0;
}

static const char *ctrl_name(uint8_t ctrl)
{
    uint8_t u = (uint8_t)(ctrl & 0xefu);

    if (u == 0x03u) {
        return "UI";
    }
    if (u == 0x2fu) {
        return "SABM";
    }
    if (u == 0x43u) {
        return "DISC";
    }
    if (u == 0x63u) {
        return "UA";
    }
    if (u == 0x0fu) {
        return "DM";
    }
    if (u == 0x87u) {
        return "FRMR";
    }
    if ((ctrl & 0x01u) == 0u) {
        return "I";
    }
    if ((ctrl & 0x03u) == 0x01u) {
        switch ((ctrl >> 2) & 3) {
        case 0:
            return "RR";
        case 1:
            return "RNR";
        case 2:
            return "REJ";
        default:
            return "SREJ";
        }
    }
    return "U";
}

static void dump_info(char *dst, size_t dst_sz, const uint8_t *p, int n)
{
    size_t o = 0;
    int i;

    for (i = 0; i < n && o + 1 < dst_sz; i++) {
        unsigned char c = p[i];

        if (c >= 32 && c < 127 && c != '\\') {
            dst[o++] = (char)c;
        } else {
            if (o + 4 >= dst_sz) {
                break;
            }
            o += (size_t)snprintf(dst + o, dst_sz - o, "\\x%02x", c);
        }
    }
    dst[o] = '\0';
}

int gt_ax25_format_data(const uint8_t *pay, int n, int verbose,
                        const char *ts, char *line, int max)
{
    char dst[16];
    char src[16];
    char via[96];
    char info[260];
    int off;
    int ext;
    int via_n = 0;
    uint8_t ctrl;
    uint8_t pid = 0;
    int has_pid = 0;
    const uint8_t *ip;
    int ilen;
    const char *prefix = (ts != NULL) ? ts : "";
    const char *sp = (ts != NULL && ts[0] != '\0') ? " " : "";

    if (n < 15) {
        return snprintf(line, (size_t)max, "%s%sDATA short len=%d",
                        prefix, sp, n);
    }
    gt_ax25_decode_call(pay, dst, sizeof(dst));
    gt_ax25_decode_call(pay + 7, src, sizeof(src));
    ext = pay[13] & 0x01;
    off = 14;
    via[0] = '\0';
    while (!ext && off + 7 <= n) {
        char d[16];

        gt_ax25_decode_call(pay + off, d, sizeof(d));
        ext = pay[off + 6] & 0x01;
        off += 7;
        if (via_n > 0 && via_n + 1 < (int)sizeof(via)) {
            via[via_n++] = ',';
        }
        {
            int dl = (int)strlen(d);

            if (via_n + dl < (int)sizeof(via)) {
                memcpy(via + via_n, d, (size_t)dl);
                via_n += dl;
                via[via_n] = '\0';
            }
        }
        if (via_n >= (int)sizeof(via) - 1) {
            break;
        }
    }
    if (off >= n) {
        return snprintf(line, (size_t)max, "%s%sDATA truncated", prefix, sp);
    }
    ctrl = pay[off++];
    if (((ctrl & 0x01u) == 0u || (ctrl & 0xefu) == 0x03u) && off < n) {
        pid = pay[off++];
        has_pid = 1;
    }
    ip = pay + off;
    ilen = n - off;
    dump_info(info, sizeof(info), ip, ilen);
    {
        /* TYPE 4 (SABM/DISC/FRMR/SREJ), P/F 1, cmd/resp 4. */
        const char *cr = gt_ax25_is_cmd(pay, n) ? "cmd" : "resp";
        const char *pf = (ctrl & 0x10u) ? (gt_ax25_is_cmd(pay, n) ? "P" : "F")
                                       : "-";
        char via_s[104];
        char seq_s[24];
        char verb_s[32];
        char info_s[264];

        via_s[0] = '\0';
        seq_s[0] = '\0';
        verb_s[0] = '\0';
        info_s[0] = '\0';
        if (via[0] != '\0') {
            snprintf(via_s, sizeof(via_s), " via %s", via);
        }
        if ((ctrl & 0x01u) == 0u) {
            snprintf(seq_s, sizeof(seq_s), " ns=%d nr=%d",
                     (ctrl >> 1) & 7, (ctrl >> 5) & 7);
        } else if ((ctrl & 0x03u) == 0x01u) {
            snprintf(seq_s, sizeof(seq_s), " nr=%d", (ctrl >> 5) & 7);
        }
        if (verbose) {
            if (has_pid) {
                snprintf(verb_s, sizeof(verb_s), " pid=%02X len=%d", pid, ilen);
            } else {
                snprintf(verb_s, sizeof(verb_s), " len=%d", ilen);
            }
        }
        if (info[0] != '\0') {
            snprintf(info_s, sizeof(info_s), " %s", info);
        }
        return snprintf(line, (size_t)max, "%s%s%-4s  %s  %-4s  %s>%s%s%s%s%s",
                        prefix, sp, ctrl_name(ctrl), pf, cr, src, dst,
                        via_s, seq_s, verb_s, info_s);
    }
}
