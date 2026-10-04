/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_cfg.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gt_cfg.h"
#include "gt_kiss.h"

#ifndef GT_VERSION
#define GT_VERSION "0.9.7"
#endif

#include <stdio.h>
#include <string.h>

static void from_kiss(struct gt_cfg *c, const kiss_cfg_t *k)
{
    c->txdelay = k->txdelay;
    c->persist = k->persist;
    c->slottime = k->slottime;
    c->txtail = k->txtail;
    c->fulldup = k->fulldup;
    c->legacy = k->legacy;
    c->log_events = k->log_events;
    c->have_txdelay = k->have_txdelay;
    c->have_persist = k->have_persist;
    c->have_slottime = k->have_slottime;
    c->have_txtail = k->have_txtail;
    c->have_fulldup = k->have_fulldup;
    c->have_legacy = k->have_legacy;
    c->have_log_events = k->have_log_events;
    c->linkmode = k->linkmode;
    memcpy(c->mycall, k->mycall, sizeof(c->mycall));
    c->paclen = k->paclen;
    c->maxframe = k->maxframe;
    c->t1_ms = k->t1_ms;
    c->t2_ms = k->t2_ms;
    c->t3_ms = k->t3_ms;
    c->n2 = k->n2;
    c->have_linkmode = k->have_linkmode;
    c->have_mycall = k->have_mycall;
    c->have_paclen = k->have_paclen;
    c->have_maxframe = k->have_maxframe;
    c->have_t1 = k->have_t1;
    c->have_t2 = k->have_t2;
    c->have_t3 = k->have_t3;
    c->have_n2 = k->have_n2;
    c->bitfix_cnt = k->bitfix_cnt;
    c->bitfix_algo = k->bitfix_algo;
    c->bitfix_sanity = k->bitfix_sanity;
    c->have_bitfix_cnt = k->have_bitfix_cnt;
    c->have_bitfix_algo = k->have_bitfix_algo;
    c->have_bitfix_sanity = k->have_bitfix_sanity;
    c->tx_awgn_on = k->tx_awgn_on;
    c->tx_awgn_cb = k->tx_awgn_cb;
    c->tx_awgn_seed = k->tx_awgn_seed;
    c->have_tx_awgn_on = k->have_tx_awgn_on;
    c->have_tx_awgn_cb = k->have_tx_awgn_cb;
    c->have_tx_awgn_seed = k->have_tx_awgn_seed;
}

int gt_cfg_from_json(const char *js, struct gt_cfg *c)
{
    kiss_cfg_t k;
    int n;
    int off = 0;
    int jn;

    if (js == NULL || c == NULL) {
        return -1;
    }
    n = (int)strlen(js);
    jn = kiss_json_span(js, n, &off);
    if (jn < 2) {
        return -1;
    }
    if (kiss_cfg_from_json(js + off, jn, &k) != 0) {
        return -1;
    }
    if (!k.have_txdelay && !k.have_persist && !k.have_slottime &&
        !k.have_txtail && !k.have_fulldup && !k.have_legacy &&
        !k.have_linkmode && !k.have_mycall && !k.have_paclen &&
        !k.have_maxframe && !k.have_t1 && !k.have_t2 && !k.have_t3 &&
        !k.have_n2 && !k.have_bitfix_cnt && !k.have_bitfix_algo &&
        !k.have_bitfix_sanity && !k.have_tx_awgn_on && !k.have_tx_awgn_cb &&
        !k.have_tx_awgn_seed) {
        return -1;
    }
    from_kiss(c, &k);
    return 0;
}

int gt_cfg_to_json(const struct gt_cfg *c, char *out, int max)
{
    int n;

    n = snprintf(out, (size_t)max,
                     "{\n"
                     "  \"version\": \"%s\",\n"
                     "  \"kiss\": {\n"
                     "    \"legacyMode\": %s,\n"
                     "    \"logEvents\": %s,\n"
                     "    \"txDelay\": %d,\n"
                     "    \"persist\": %d,\n"
                     "    \"slotTime\": %d,\n"
                     "    \"txTail\": %d,\n"
                     "    \"fullDup\": %d\n"
                     "  },\n"
                     "  \"link\": {\n"
                     "    \"linkMode\": \"%s\",\n"
                     "    \"myCall\": \"%s\",\n"
                     "    \"paclen\": %d,\n"
                     "    \"maxframe\": %d,\n"
                     "    \"t1_ms\": %d,\n"
                     "    \"t2_ms\": %d,\n"
                     "    \"t3_ms\": %d,\n"
                     "    \"n2\": %d\n"
                     "  },\n"
                     "  \"modem\": {\n"
                     "    \"bitFixCnt\": %d,\n"
                     "    \"bitFixAlgo\": 0,\n"
                     "    \"bitFixSanity\": \"%s\"\n"
                     "  },\n"
                     "  \"debug\": {\n"
                     "    \"txAwgnInject\": %s,\n"
                     "    \"txAwgnSNRdB\": %d.%02d,\n"
                     "    \"txAwgnSeed\": %d\n"
                     "  }\n"
                     "}\n",
                     GT_VERSION, c->legacy ? "true" : "false",
                     c->log_events ? "true" : "false",
                     c->txdelay, c->persist, c->slottime, c->txtail, c->fulldup,
                     c->linkmode ? "I" : "UI",
                     c->mycall[0] != '\0' ? c->mycall : "TEST-0",
                     c->paclen > 0 ? c->paclen : 256,
                     c->maxframe > 0 ? c->maxframe : 4,
                     c->t1_ms > 0 ? c->t1_ms : 2000,
                     c->t2_ms,
                     c->t3_ms > 0 ? c->t3_ms : 30000,
                     c->n2 > 0 ? c->n2 : 10,
                     c->bitfix_cnt == 1 ? 1 : 0,
                     c->bitfix_sanity ? "ASCII" : "none",
                     c->tx_awgn_on ? "true" : "false",
                     (c->tx_awgn_cb > 0 ? c->tx_awgn_cb : 325) / 100,
                     (c->tx_awgn_cb > 0 ? c->tx_awgn_cb : 325) % 100,
                     c->tx_awgn_seed > 0 ? c->tx_awgn_seed : 1);
    if (n < 0 || n >= max) {
        return -1;
    }
    return n;
}

int gt_cfg_parse_log(const char *line, struct gt_cfg *c)
{
    kiss_cfg_t k;
    int off = 0;
    int jn = 0;
    int n;

    if (line == NULL || c == NULL) {
        return 0;
    }
    n = (int)strlen(line);
    if (kiss_cfg_unwrap(line, n, &off, &jn) != 1) {
        return 0;
    }
    if (kiss_cfg_from_json(line + off, jn, &k) != 0) {
        return 0;
    }
    from_kiss(c, &k);
    return 1;
}

struct cfg_wait {
    struct gt_cfg *c;
    int crc_ok;
};

static int match_cfg_reply(const char *line, void *ctx)
{
    struct cfg_wait *w = (struct cfg_wait *)ctx;

    if (gt_cfg_parse_log(line, w->c)) {
        w->crc_ok = 1;
        return 1;
    }
    if (strstr(line, "[Cfg]") != NULL && strstr(line, "crc") != NULL &&
        strchr(line, '{') == NULL) {
        w->crc_ok = 0;
        return 1;
    }
    return 0;
}

int gt_cfg_get(int fd, struct gt_cfg *c)
{
    uint8_t hw = KISS_HW_CFG;
    struct cfg_wait w;

    memset(c, 0, sizeof(*c));
    w.c = c;
    w.crc_ok = 0;
    if (gt_kiss_write(fd, KISS_CMD_SETHW, &hw, 1) != 0) {
        return -1;
    }
    (void)gt_kiss_drain(fd);
    if (gt_kiss_wait_log(fd, 1500, NULL, match_cfg_reply, &w, NULL, 0) != 1) {
        return 0;
    }
    if (!w.crc_ok) {
        return 0;
    }
    return 1;
}

struct raw_wait {
    char *out;
    int max;
    int n;
    int crc_ok;
};

static int match_cfg_raw(const char *line, void *ctx)
{
    struct raw_wait *w = (struct raw_wait *)ctx;
    int off = 0;
    int jn = 0;
    int n;

    n = (int)strlen(line);
    if (kiss_cfg_unwrap(line, n, &off, &jn) == 1) {
        if (jn + 1 > w->max) {
            w->n = -2;
            w->crc_ok = 0;
            return 1;
        }
        memcpy(w->out, line + off, (size_t)jn);
        w->out[jn] = '\0';
        w->n = jn;
        w->crc_ok = 1;
        return 1;
    }
    if (strstr(line, "[Cfg]") != NULL && strstr(line, "crc") != NULL &&
        strchr(line, '{') == NULL) {
        w->crc_ok = 0;
        w->n = 0;
        return 1;
    }
    return 0;
}

int gt_cfg_get_json(int fd, char *out, int max)
{
    uint8_t hw = KISS_HW_CFG;
    struct raw_wait w;

    if (out == NULL || max < 3) {
        return -2;
    }
    out[0] = '\0';
    w.out = out;
    w.max = max;
    w.n = 0;
    w.crc_ok = 0;
    if (gt_kiss_write(fd, KISS_CMD_SETHW, &hw, 1) != 0) {
        return -1;
    }
    (void)gt_kiss_drain(fd);
    if (gt_kiss_wait_log(fd, 1500, NULL, match_cfg_raw, &w, NULL, 0) != 1) {
        return 0;
    }
    if (!w.crc_ok) {
        return w.n < 0 ? w.n : 0;
    }
    return w.n;
}

static int pretty_put(char *out, int max, int n, char c)
{
    if (n < 0 || n + 1 >= max) {
        return -1;
    }
    out[n] = c;
    return n + 1;
}

static int pretty_indent(char *out, int max, int n, int ind)
{
    int i;

    n = pretty_put(out, max, n, '\n');
    for (i = 0; i < ind; i++) {
        n = pretty_put(out, max, n, ' ');
        n = pretty_put(out, max, n, ' ');
    }
    return n;
}

int gt_cfg_pretty(const char *in, char *out, int max)
{
    int i;
    int n = 0;
    int ind = 0;
    int str = 0;
    int esc = 0;

    if (in == NULL || out == NULL || max < 3) {
        return -1;
    }
    for (i = 0; in[i] != '\0'; i++) {
        char c = in[i];

        if (str) {
            n = pretty_put(out, max, n, c);
            if (esc) {
                esc = 0;
            } else if (c == '\\') {
                esc = 1;
            } else if (c == '"') {
                str = 0;
            }
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        if (c == '"') {
            n = pretty_put(out, max, n, c);
            str = 1;
            continue;
        }
        if (c == '{' || c == '[') {
            n = pretty_put(out, max, n, c);
            ind++;
            n = pretty_indent(out, max, n, ind);
            continue;
        }
        if (c == '}' || c == ']') {
            ind--;
            if (ind < 0) {
                return -1;
            }
            n = pretty_indent(out, max, n, ind);
            n = pretty_put(out, max, n, c);
            continue;
        }
        if (c == ',') {
            n = pretty_put(out, max, n, c);
            n = pretty_indent(out, max, n, ind);
            continue;
        }
        if (c == ':') {
            n = pretty_put(out, max, n, c);
            n = pretty_put(out, max, n, ' ');
            continue;
        }
        n = pretty_put(out, max, n, c);
    }
    if (str || ind != 0) {
        return -1;
    }
    n = pretty_put(out, max, n, '\n');
    if (n < 0) {
        return -1;
    }
    out[n] = '\0';
    return n;
}

int gt_cfg_put_json(int fd, const char *js)
{
    char body[KISS_MAX_FRAME];
    uint8_t pay[KISS_MAX_FRAME];
    struct gt_cfg parsed;
    struct gt_cfg ack;
    struct cfg_wait w;
    int n;
    int off = 0;
    int jn;
    int wrapped;

    if (js == NULL) {
        return -2;
    }
    n = (int)strlen(js);
    jn = kiss_json_span(js, n, &off);
    if (jn < 2 || jn + 6 >= (int)sizeof(body)) {
        return -2;
    }
    if (gt_cfg_from_json(js, &parsed) != 0) {
        return -2;
    }
    memcpy(body, js + off, (size_t)jn);
    wrapped = kiss_cfg_wrap(body, jn, (int)sizeof(body));
    if (wrapped < 0) {
        return -1;
    }
    if (1 + wrapped > (int)sizeof(pay)) {
        return -1;
    }
    pay[0] = KISS_HW_CFG;
    memcpy(pay + 1, body, (size_t)wrapped);
    if (gt_kiss_write(fd, KISS_CMD_SETHW, pay, 1 + wrapped) != 0) {
        return -1;
    }
    (void)gt_kiss_drain(fd);
    memset(&ack, 0, sizeof(ack));
    w.c = &ack;
    w.crc_ok = 0;
    if (gt_kiss_wait_log(fd, 1500, NULL, match_cfg_reply, &w, NULL, 0) != 1) {
        return 0;
    }
    return w.crc_ok ? 1 : 0;
}
