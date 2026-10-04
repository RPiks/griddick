#ifndef GT_CFG_H
#define GT_CFG_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_cfg.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#ifdef __cplusplus
extern "C" {
#endif

struct gt_cfg {
    int txdelay;
    int persist;
    int slottime;
    int txtail;
    int fulldup;
    int legacy;
    int log_events;
    int have_txdelay;
    int have_persist;
    int have_slottime;
    int have_txtail;
    int have_fulldup;
    int have_legacy;
    int have_log_events;
    int linkmode;
    char mycall[10];
    int paclen;
    int maxframe;
    int t1_ms;
    int t2_ms;
    int t3_ms;
    int n2;
    int have_linkmode;
    int have_mycall;
    int have_paclen;
    int have_maxframe;
    int have_t1;
    int have_t2;
    int have_t3;
    int have_n2;
    int bitfix_cnt;
    int bitfix_algo;
    int bitfix_sanity;
    int have_bitfix_cnt;
    int have_bitfix_algo;
    int have_bitfix_sanity;
    int tx_awgn_on;
    int tx_awgn_cb;
    int tx_awgn_seed;
    int have_tx_awgn_on;
    int have_tx_awgn_cb;
    int have_tx_awgn_seed;
};

int gt_cfg_from_json(const char *js, struct gt_cfg *c);
int gt_cfg_to_json(const struct gt_cfg *c, char *out, int max);
int gt_cfg_parse_log(const char *line, struct gt_cfg *c);

int gt_cfg_get(int fd, struct gt_cfg *c);
/* Copy the Cfg reply object, no ,$CRC. Length, 0 if none/bad, −1 I/O, −2 short. */
int gt_cfg_get_json(int fd, char *out, int max);
/* Indent a JSON object. Copies tokens; does not interpret keys. */
int gt_cfg_pretty(const char *in, char *out, int max);
/* Send file JSON {…},$CRC on SETHW 0x11. 1=ack, 0=crc/timeout, −1=I/O, −2=bad JSON. */
int gt_cfg_put_json(int fd, const char *js);

#ifdef __cplusplus
}
#endif

#endif
