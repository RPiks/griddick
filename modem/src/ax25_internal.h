#ifndef AX25_INTERNAL_H
#define AX25_INTERNAL_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * ax25_internal.h - This file is part of Griddick TNC.
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

/**
 * @file ax25_internal.h
 * @brief Shared AX.25 mode state and helpers. Not a public API.
 *
 * Public entry points are declared in modem_mode_ax25.h /
 * modem_ax25_session.h. This header is the contract among ax25_*.c:
 * FCS, HDLC stuff/unstuff, NRZI, UI pack/parse, RX DSP, TX queue/PWM.
 */

#include "modem_mode.h"
#include "modem_mode_ax25.h"

#include "pixdsp_agc_i16.h"
#include "pixdsp_dpll.h"

#include <stddef.h>
#include <stdint.h>

/** RX / offline render sample rate (Hz). Must match MODEM_AX25_RX_FS_HZ. */
#define AX25_FS_HZ           MODEM_AX25_RX_FS_HZ
/** Default baud (Bell 202). */
#define AX25_BAUD            1200
/** Samples/symbol at 48 kHz / 1200 baud. PWM TX uses ax25_sps_for_fs(). */
#define AX25_SPS             40
#define AX25_MARK_HZ         1200.0f
#define AX25_SPACE_HZ        2200.0f

#define AX25_HDLC_FLAG       0x7Eu
#define AX25_HDLC_MAX_FRAME  MODEM_AX25_HDLC_MAX
#define AX25_CALL_MAX        MODEM_AX25_CALL_MAX
#define AX25_VIA_MAX         MODEM_AX25_VIA_MAX

/** HDLC opening flags required before state becomes DEMOD (log only). */
#define AX25_LOCK_MIN_FLAGS  3
/** Drop a duplicate good-FCS body if it repeats within this many samples. */
#define AX25_DUP_SAMPLES     4800 /* 100 ms @ 48 kHz */
#define AX25_TX_MAX_TONES    16384
#define AX25_TX_TRAIL_FLAGS  2
#define AX25_TX_MAX_LEAD_FLAGS 128
#define AX25_INFO_MAX        MODEM_AX25_INFO_MAX
/** Raw slicer bits between flags (1 byte/bit) + opening-flag seed. */
#define AX25_RAW_MAX_BITS    5120
/** Direwolf MIN_FRAME_LEN: dest+src+ctrl + 2-byte FCS. */
#define AX25_FIX_MIN_FRAME   17
/** F=1 live cap (raw tones including opening-flag seed). */
#define AX25_FIX_MAX_RAW     2048
/** Live RF: one search slice per core0_poll (µs). 0 disables the cap. */
#define AX25_FIX_SLICE_US    8000
/** @deprecated same as SLICE; kept for older call sites. */
#define AX25_FIX_BUDGET_US   AX25_FIX_SLICE_US
#define AX25_FIX_SANITY_NONE  0
#define AX25_FIX_SANITY_ASCII 1
/** Soft search: try pairs among this many least-|y| raw bits. */
#define AX25_FIX_SOFT_M      16

/** Gate 1: 25 frozen pre-rolls (symbol phases + longer pads) @ 48 kHz / 1200. */
#define AX25_CLEAN_NLEADS    25
#define AX25_CLEAN_LEADS                                                       \
    0, 1, 2, 4, 5, 8, 10, 13, 16, 20, 25, 32, 39, 40, 80, 160, 240, 320, 480,  \
        640, 960, 1280, 1920, 2400, 3840
/* Gate-1 lead table is used by Host over-air tools, not this library. */

/**
 * @brief HDLC RX hunter (post-NRZI data bits, LSB-first octets).
 *
 * pat_det is a sliding 8-bit window (MSB = newest). Flag 0x7E closes a
 * body when olen==7 and len≥3. 0xFE is abort (seven 1s). 0x7C with the
 * low bits matching stuffed 0 after five 1s discards that 0.
 */
typedef struct ax25_hdlc_rx {
    uint8_t pat_det;   /**< Last 8 dbits; flag when == 0x7E. */
    uint8_t oacc;      /**< Octet accumulator (LSB-first shift). */
    int olen;          /**< −1 idle, 0..7 assembling. */
    int in_frame;      /**< 1 after an opening flag. */
    uint8_t buf[AX25_HDLC_MAX_FRAME];
    size_t len;        /**< Bytes in buf (Address..FCS when emit). */
    int flags_leading; /**< Opening flags before first data octet. */
    int data_seen;     /**< 1 after the first non-flag octet. */
    int got_octet;     /**< 1 if a data octet landed since the last flag. */
} ax25_hdlc_rx_t;

/**
 * @brief Pre-NRZI slicer bits between HDLC flags, plus analog y per bit.
 *
 * bit[0] is the last tone of the opening flag (Direwolf rrbb). y[] is the
 * DPLL-strobe decision statistic (same y used to slice); stored for a
 * later soft-decision search, unused by F=1.
 */
typedef struct ax25_raw_buf {
    uint8_t bit[AX25_RAW_MAX_BITS];
    int16_t y[AX25_RAW_MAX_BITS]; /* Q15 decision stat; unused by F=1 */
    int len;
} ax25_raw_buf_t;

/**
 * @brief Parsed UI fields from an HDLC body (FCS still attached).
 *
 * ok is 0 if the body is too short or the address extension walk fails.
 * info points into the caller's hdlc buffer (not copied).
 */
typedef struct ax25_ui_parse {
    char dst[AX25_CALL_MAX];
    char src[AX25_CALL_MAX];
    char via[AX25_VIA_MAX];
    uint8_t ctrl;
    uint8_t pid;
    const uint8_t *info;
    size_t info_len;
    int ok;
} ax25_ui_parse_t;

/**
 * @brief HDLC lock state for status / logs. Demod always runs.
 */
typedef enum ax25_rx_state {
    AX25_RX_SEARCH = 0, /**< Fewer than AX25_LOCK_MIN_FLAGS opening flags. */
    AX25_RX_DEMOD = 1   /**< ≥3 flags seen; still always-on bits. */
} ax25_rx_state_t;

/**
 * @brief AX.25 mode private state (modem_mode_t::impl).
 *
 * RX: two pixdsp_agc_i16 + pixdsp_dpll → NRZI → HDLC.
 * TX: stuffed bits → NRZI tones → cosine NCO (float or PWM).
 */
typedef struct ax25_impl {
    float sample_rate_hz;
    float baud;
    float mark_hz;
    float space_hz;
    modem_ax25_rx_fn rx_cb;
    void *rx_cb_ctx;
    int rx_cfg; /**< 1 after successful set_rx / set_rx_phy. */

    int64_t sample_abs; /**< Samples consumed from stream start. */

    int sync_sample;
    float level_acc;
    int level_n;

    ax25_rx_state_t state;
    pixdsp_agc_i16_t agc_m_i16;
    pixdsp_agc_i16_t agc_s_i16;
    pixdsp_dpll_t dpll;
    int prev_tone; /**< 0 = mark, 1 = space; start 1 = Direwolf prev_raw=0. */
    int fix_bits;  /**< MODEM_AX25_FIX_NONE, FIX_SINGLE, or FIX_SOFT. */
    ax25_hdlc_rx_t hdlc;
    ax25_raw_buf_t raw;
    uint8_t dup_hdlc[AX25_HDLC_MAX_FRAME];
    size_t dup_len;
    int64_t dup_abs;

    /** Last emitted frame scratch (valid during callback). */
    uint8_t frame_hdlc[AX25_HDLC_MAX_FRAME];
    size_t frame_len;
    ax25_ui_parse_t ui;
    char via_empty;

    /* TX */
    int tx_queued;
    int tx_active;
    int tx_tones[AX25_TX_MAX_TONES];
    int tx_n_tones;
    int tx_tone_i;
    int tx_sample_in_sym;
    int tx_sps;
    double tx_phase;
    float tx_amp; /**< PWM peak as fraction of MODEM_PWM_MID. */
    float tx_fs;
    int tx_lead_flags;
    int tx_hdlc_len;
    int64_t tx_pcm_n;

    /* RX stats / log */
    modem_ax25_log_fn log_fn;
    void *log_ctx;
    int64_t rx_pcm_n;
    float rx_last_rms;
    int rx_demod_bits;
    int rx_lock_announced;
    int64_t rx_log_at;
} ax25_impl_t;

/**
 * @brief Format a timestamped log line and pass it to ax->log_fn.
 * @param ax  Mode impl; no-op if NULL or log_fn is NULL.
 * @param fmt printf-style format.
 */
void ax25_logf(ax25_impl_t *ax, const char *fmt, ...);

/**
 * @brief Integer baud for TXDELAY / sps. Values < 1 map to 1200.
 * @param baud Requested baud (float).
 * @return Rounded baud ≥ 1, or AX25_BAUD.
 */
static inline int ax25_baud_i(float baud)
{
    int b;

    if (baud < 1.0f) {
        return AX25_BAUD;
    }
    b = (int)(baud + 0.5f);
    if (b < 1) {
        return AX25_BAUD;
    }
    return b;
}

/**
 * @brief Samples per symbol at an integer fs that divides baud.
 * @param sample_rate_hz PCM rate (Hz).
 * @param baud           Symbol rate; < 1 → 1200.
 * @return fs/baud, or −1 if fs < 2·baud or fs % baud ≠ 0.
 */
static inline int ax25_sps_for_fs(float sample_rate_hz, float baud)
{
    int fs_i;
    int baud_i;

    if (sample_rate_hz < 1.0f) {
        return -1;
    }
    fs_i = (int)(sample_rate_hz + 0.5f);
    baud_i = ax25_baud_i(baud);
    if (fs_i < baud_i * 2 || (fs_i % baud_i) != 0) {
        return -1;
    }
    return fs_i / baud_i;
}

/**
 * @brief CRC-16/X.25 (ISO 3309 / AX.25) over data[0..len).
 *
 * Reflected poly 0x1021 (0x8408), init/xor 0xFFFF. On the wire the FCS
 * is appended low-byte first (Direwolf / classic TNC).
 * @param data Frame body without FCS; NULL → 0.
 * @param len  Byte count.
 * @return FCS value (not inverted again).
 */
uint16_t ax25_fcs_calc(const uint8_t *data, size_t len);

/**
 * @brief Clear HDLC hunter to idle (olen = −1, no body).
 * @param h Hunter; no-op if NULL.
 */
void ax25_hdlc_rx_init(ax25_hdlc_rx_t *h);

/**
 * @brief Consume one post-NRZI data bit.
 * @param h   Hunter.
 * @param bit Data bit (0/1); only the LSB is used.
 * @return 1 if a frame body (Address..FCS) is ready in h->buf[0..len).
 */
int ax25_hdlc_rx_bit(ax25_hdlc_rx_t *h, int bit);

void ax25_raw_reset(ax25_raw_buf_t *r);
void ax25_raw_append(ax25_raw_buf_t *r, int bit, int32_t y);
void ax25_raw_chop8(ax25_raw_buf_t *r);

/**
 * @brief Destuff a raw NRZI tone block (bit 0 = prev / last opening-flag tone).
 * @param invert_i Index of one raw bit to invert, or −1 for none.
 * @param invert_j Second invert index, or −1 for none.
 * @param invert_k Third invert index, or −1 for none.
 * @return 1 if a whole-octet body is in @p out, 0 otherwise.
 */
int ax25_fix_decode_raw(const uint8_t *raw, int nraw, int invert_i, int invert_j,
                        int invert_k, uint8_t *out, size_t max_out, size_t *len);

/**
 * @brief AX.25 address sanity (Direwolf SANITY_AX25). @p len is without FCS.
 * @return 1 if the address part looks like AX.25, 0 otherwise.
 */
int ax25_fix_sanity_ax25(const uint8_t *buf, size_t len);

/**
 * @brief Direwolf SANITY_APRS (atest default): AX.25 address + UI/PID + Info bytes.
 * @param len Byte count without FCS.
 * @return 1 if it looks like APRS, 0 otherwise.
 */
int ax25_fix_sanity_aprs(const uint8_t *buf, size_t len);

/**
 * @brief Info octets (after address/ctrl/PID) are 0x20..0x7E. Empty Info OK.
 * @param len Byte count including FCS.
 */
int ax25_fix_sanity_ascii(const uint8_t *buf, size_t len);

/**
 * @brief Direwolf F=1: invert each raw tone until FCS (+ optional ASCII) pass.
 * @param sanity AX25_FIX_SANITY_NONE or _ASCII (ASCII only gates repairs).
 * @param budget_us Cap; 0 or now_us NULL disables.
 * @param io_pos Resume index; NULL starts at 0. On return: next i,
 *        nraw if the walk finished without a hit.
 * @return 1 if a repaired body is in @p out, 0 otherwise.
 */
int ax25_fix_invert_single(const uint8_t *raw, int nraw, uint8_t *out,
                           size_t max_out, size_t *len, int sanity,
                           uint32_t budget_us, uint32_t (*now_us)(void),
                           int *io_pos);

/**
 * @brief Direwolf F=4 order: index singles, adj 2/3, then two-sep. @p y unused.
 * @param retries_out Optional; FIX_SOFT, ADJ, TRIPLE, or SEP on success.
 * @return 1 if a repaired body is in @p out, 0 otherwise.
 */
int ax25_fix_invert_soft(const uint8_t *raw, const int16_t *y, int nraw,
                         uint8_t *out, size_t max_out, size_t *len,
                         uint8_t *retries_out);

/**
 * @brief One DPLL strobe: NRZI, raw-buffer, HDLC hunt, optional F=1/F=soft emit.
 */
void ax25_rx_strobe(ax25_impl_t *ax, int tone, int32_t y);

/**
 * @brief Bit-stuff one octet LSB-first into bit_out.
 * @param bit_out  Output bits (0/1 per byte).
 * @param max_bits Capacity of @p bit_out.
 * @param byte     Octet to stuff.
 * @param ones     In/out consecutive-1s count across octets.
 * @return Bits written, or −1 if the buffer is too small.
 */
int ax25_hdlc_stuff_byte(uint8_t *bit_out, int max_bits, uint8_t byte, int *ones);

/**
 * @brief Append one HDLC flag (0x7E, LSB-first bits 0,1,1,1,1,1,1,0).
 * @param bit_out  Output bits.
 * @param max_bits Capacity (must be ≥ 8).
 * @return 8, or −1.
 */
int ax25_hdlc_append_flag(uint8_t *bit_out, int max_bits);

/**
 * @brief NRZI-encode data bits to mark/space tones.
 *
 * bit 0 = transition, bit 1 = no transition. tone 0 = mark, 1 = space.
 * @param bits       Data bits (LSB of each byte).
 * @param n_bits     Count.
 * @param tones_out  Output tones; length n_bits.
 * @param start_tone Initial tone (only LSB used).
 * @return 0, or −1 on bad args.
 */
int ax25_nrzi_encode_bits(const uint8_t *bits, int n_bits, int *tones_out,
                          int start_tone);

/**
 * @brief NRZI-decode a tone sequence to data bits.
 *
 * First output bit is 1 (no prior sample). If start_tone_io is non-NULL
 * it is set to tones[0].
 * @param tones         Tone sequence (0 = mark, 1 = space).
 * @param n             Count (≥ 1).
 * @param bits_out      Output data bits.
 * @param start_tone_io Optional; set to first tone.
 * @return 0, or −1 on bad args.
 */
int ax25_nrzi_decode_tones(const int *tones, int n, uint8_t *bits_out,
                           int *start_tone_io);

/**
 * @brief Parse dest/src/via/ctrl/PID/info from an HDLC body that includes FCS.
 * @param hdlc Body Address..FCS.
 * @param len  Byte count (must include 2-byte FCS).
 * @param out  Result; ok = 0 on failure.
 */
void ax25_ui_parse(const uint8_t *hdlc, size_t len, ax25_ui_parse_t *out);

/**
 * @brief Pack dest/src/via + UI ctrl 0x03 + PID + info + FCS.
 * @param out      Output buffer.
 * @param max      Capacity of @p out.
 * @param dst      Dest callsign[-SSID].
 * @param src      Source callsign[-SSID].
 * @param via      Comma-separated digis, or NULL / "".
 * @param pid      Protocol ID.
 * @param info     Info octets; NULL allowed if info_len is 0.
 * @param info_len Info length (≤ AX25_INFO_MAX).
 * @return Bytes written, or −1.
 */
int ax25_ui_pack(uint8_t *out, size_t max, const char *dst, const char *src,
                 const char *via, uint8_t pid, const uint8_t *info,
                 size_t info_len);

/**
 * @brief Init envelope demod, per-tone AGC, and DPLL from a PHY profile.
 *
 * fs is 48 kHz. phy->fix_bits selects optional FCS repair.
 * @param ax  Mode impl.
 * @param phy Profile; NULL treated as all zeros (Bell 202 defaults).
 * @return 0, or −1 on bad fs or DSP init failure.
 */
int ax25_rx_init(ax25_impl_t *ax, const modem_ax25_phy_t *phy);

/**
 * @brief Mark RX DSP as uninitialized (no free; compact FIR is inline).
 * @param ax Mode impl; no-op if NULL.
 */
void ax25_rx_shutdown(ax25_impl_t *ax);

/**
 * @brief Reset envelope / AGC / DPLL / HDLC / dup / counters. PHY stays.
 * @param ax Mode impl; no-op if NULL.
 */
void ax25_rx_reset_state(ax25_impl_t *ax);

void ax25_rx_process_i16(ax25_impl_t *ax, const int16_t *samples, int count);
void ax25_on_pcm_i16(modem_mode_t *mode, const int16_t *samples, int count);

/**
 * @brief Stuff HDLC flags + body, NRZI-encode, store tones on ax.
 *
 * Does not start the NCO; ax25_tx_prepare / render does that.
 * @param ax             Mode impl.
 * @param hdlc           Address..FCS.
 * @param len            Byte count (3…AX25_HDLC_MAX_FRAME).
 * @param amp            Peak fraction of MODEM_PWM_MID.
 * @param txdelay_sec    Leading-flag duration.
 * @param sample_rate_hz PCM rate (must divide baud).
 * @return 0, or −1.
 */
int ax25_tx_queue_hdlc(ax25_impl_t *ax, const uint8_t *hdlc, size_t len,
                       float amp, float txdelay_sec, float sample_rate_hz);

/**
 * @brief mode->tx_prepare: arm the cosine NCO from a queued tone list.
 * @param mode Mode.
 * @param s    Session (unused; fs comes from ax25_tx_queue_hdlc).
 * @param req  Unused.
 * @return 0, or −1 if nothing queued / sps invalid.
 */
int ax25_tx_prepare(modem_mode_t *mode, struct modem_session *s,
                    const struct modem_tx_request *req);

/**
 * @brief mode->tx_pull_pcm: next PWM samples (MID + y·MID).
 * @param mode      Mode.
 * @param out       PWM buffer.
 * @param max_count Capacity.
 * @return Samples written; 0 when the burst is done; −1 on bad args.
 */
int ax25_tx_pull_pcm(modem_mode_t *mode, uint16_t *out, int max_count);

#endif /* AX25_INTERNAL_H */
