#ifndef KISS_H
#define KISS_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * kiss.h - This file is part of Griddick TNC.
 *
 * DESCRIPTION
 *     The Griddick project provides off-grid comm function using a cheap
 * FM handheld radio and Pi Pico board.
 *
 * PLATFORM
 *     Raspberry Pi Pico (or Linux for host utils)
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

#include <stdint.h>

#define KISS_FEND  0xC0u
#define KISS_FESC  0xDBu
#define KISS_TFEND 0xDCu
#define KISS_TFESC 0xDDu

#define KISS_CMD_DATA     0x00u
#define KISS_CMD_TXDELAY  0x01u
#define KISS_CMD_PERSIST  0x02u
#define KISS_CMD_SLOTTIME 0x03u
#define KISS_CMD_TXTAIL   0x04u
#define KISS_CMD_FULLDUP  0x05u
#define KISS_CMD_SETHW    0x06u /* TAPR SetHardware */
#define KISS_CMD_LOG      0x07u /* Pico → host ASCII log line */
#define KISS_CMD_PCM      0x08u /* Pico ↔ host 48 kHz int16 chunk */
#define KISS_CMD_MASK     0x0Fu

#define KISS_HW_CAP_OFF   0x00u
#define KISS_HW_CAP_ON    0x01u
#define KISS_HW_TIME      0x10u /* + 8-byte LE unix epoch ms */
#define KISS_HW_DUMP_OFF    0x20u
#define KISS_HW_DUMP_ON     0x21u
#define KISS_HW_INJECT_OFF  0x22u
#define KISS_HW_INJECT_ON   0x23u
#define KISS_HW_CFG         0x11u /* query or put JSON cfg + ,$CRC16 */

#define KISS_PCM_CHUNK    240 /* samples per full PCM frame */
#define KISS_PCM_SEQ      1
#define KISS_PCM_CRC      2

#define KISS_MAX_FRAME 1024 /* cmd + JSON cfg or HDLC body */
#define KISS_MAX_WIRE  (2 * KISS_MAX_FRAME + 2)

typedef struct {
    uint8_t buf[KISS_MAX_FRAME];
    int len;
    int frame_len;
    int escaped;
    int in_frame;
} kiss_rx_t;

void kiss_rx_init(kiss_rx_t *s);

/**
 * @brief Feed one wire byte.
 * @return 1 if a frame is in buf[0 .. frame_len-1] (consume before next call),
 *         0 if more bytes needed.
 */
int kiss_rx_byte(kiss_rx_t *s, uint8_t b);

/**
 * @brief Write FEND + escaped (cmd || data) + FEND.
 * @return Bytes written, or −1.
 */
int kiss_encode(uint8_t cmd, const uint8_t *data, int n_data,
                uint8_t *out, int max);

/** CRC-16/CCITT-FALSE (init 0xFFFF, poly 0x1021). */
uint16_t kiss_crc16(const uint8_t *data, int n);

typedef struct {
    int txdelay;
    int persist;
    int slottime;
    int txtail;
    int fulldup;
    int legacy;
    int have_txdelay;
    int have_persist;
    int have_slottime;
    int have_txtail;
    int have_fulldup;
    int have_legacy;
    int log_events;
    int have_log_events;
    int linkmode;          /* 0 = UI, 1 = I */
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
} kiss_cfg_t;

/* First '{' to last '}'. Returns slice length, or −1. */
int kiss_json_span(const char *s, int n, int *off);

/*
 * CRC of s[0 .. json_n-1] (the {…} slice). Append ,$XXXX (uppercase).
 * Returns new length, or −1.
 */
int kiss_cfg_wrap(char *s, int json_n, int max);

/*
 * Locate {…},$XXXX in s[0 .. n).
 * Returns 1 if CRC matches, 0 if wrapper present but CRC/format bad, −1 if none.
 */
int kiss_cfg_unwrap(const char *s, int n, int *json_off, int *json_n);

int kiss_cfg_from_json(const char *js, int n, kiss_cfg_t *c);
/* version NULL: build.version from the firmware stamp (manifest). */
int kiss_cfg_to_json(const kiss_cfg_t *c, const char *version,
                     char *out, int max);

#endif /* KISS_H */
