#ifndef MODEM_MODE_AX25_H
#define MODEM_MODE_AX25_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * modem_mode_ax25.h - This file is part of Griddick TNC.
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
 * @file modem_mode_ax25.h
 * @brief AX.25 UI over AFSK: PCM codec (RX, pack/parse, offline float TX).
 *
 * Sources live under modem/ax25/. This header is the embedder API: it does
 * not include modem_session.h. Pico PTT/PWM TX is modem_ax25_session.h.
 *
 * Product: PCM16 @ MODEM_AX25_RX_FS_HZ (48000) in, UI frames out;
 * pack/render the reverse (offline TX is still float). The app owns
 * audio I/O. Link librpimodem_ax25 and pixdsp. Pico PTT TX is
 * librpimodem_modem. modem_start_rx (12 kHz ADC_USB) is not AX.25 RX.
 *
 * Tract: IFIR P1 + dds_i32 mix + boxcar 45-13-15 + hypot_i32 + agc_i16.
 *
 * Threading: create / set_rx / feed / render / destroy are not thread-safe
 * on one instance. modem_ax25_rx_fn runs on the feed caller's thread;
 * hdlc[] / info / callsigns are valid only until the callback returns.
 *
 * See docs/ax25.txt.
 */

#include "modem_mode.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AX.25 RX / offline WAV sample rate (Hz).
 *
 * Live PWM TX stays MODEM_SAMPLE_RATE_HZ (typically 12 kHz). TX requires
 * fs % baud == 0.
 */
#define MODEM_AX25_RX_FS_HZ  48000.0f

/** HDLC body Address..FCS (no flags), bytes. */
#define MODEM_AX25_HDLC_MAX  512
/** UI Info field, bytes. */
#define MODEM_AX25_INFO_MAX  256
/** Callsign[-SSID] text including NUL. */
#define MODEM_AX25_CALL_MAX  16
/** Comma-separated digi path including NUL. */
#define MODEM_AX25_VIA_MAX   80

/**
 * @brief RX options for the fixed Bell 202 @ 48 kHz tract.
 */
typedef struct modem_ax25_phy {
    int fix_bits; /**< 0 off; 1 F=1; 2 Direwolf F=4 order. Default 0. */
} modem_ax25_phy_t;

/** phy.fix_bits / meta.retries: no repair. */
#define MODEM_AX25_FIX_NONE   0
/** Invert one pre-NRZI (slicer) bit and re-destuff (index order). */
#define MODEM_AX25_FIX_SINGLE 1
/** Index-order singles, adj doubles/triples, then two-sep (Direwolf F=4). */
#define MODEM_AX25_FIX_SOFT   2
/** meta.retries (unused): was 16-weakest pair; kept for TSV how=3. */
#define MODEM_AX25_FIX_SOFT_PAIR 3
/** meta.retries when an adjacent raw-bit double (Direwolf F=2) was accepted. */
#define MODEM_AX25_FIX_SOFT_ADJ  4
/** meta.retries when an adjacent raw-bit triple (Direwolf F=3) was accepted. */
#define MODEM_AX25_FIX_SOFT_TRIPLE 5
/** meta.retries when a two-separated pair (Direwolf F=4) was accepted. */
#define MODEM_AX25_FIX_SOFT_SEP 6

/**
 * @brief Fill default PHY (fix_bits = 0).
 * @param phy Profile to overwrite. No-op if NULL.
 */
void modem_ax25_phy_init(modem_ax25_phy_t *phy);

/**
 * @brief Named alias of modem_ax25_phy_init.
 * @param phy Profile to overwrite. No-op if NULL.
 */
void modem_ax25_phy_bell202_48k(modem_ax25_phy_t *phy);

/**
 * @brief Per-frame RX metadata filled when HDLC closes a body (Address..FCS).
 *
 * Delivered for both good and bad FCS. snr_db is unused (always NAN) and
 * kept for ABI. level_dbfs is packet RMS in dBFS from PCM energy since
 * the last reset / last frame.
 */
typedef struct modem_ax25_rx_meta {
    int64_t time_ms;       /**< CLOCK_MONOTONIC ms at FCS complete. */
    int fcs_ok;            /**< 1 if received FCS matches CRC-16/X.25. */
    uint16_t fcs_rx;       /**< FCS octets from the wire (LE). */
    uint16_t fcs_calc;     /**< FCS computed over Address..Info. */

    float snr_db;          /**< Unused; always NAN. Kept for ABI. */
    float level_dbfs;      /**< Packet RMS, 20·log10(rms); −100 if silent. */
    int sync_sample;       /**< sample_abs at last flag / lock (stream index). */
    int bit_len;           /**< 8 · hdlc_len (body bits, not stuffed). */

    uint8_t flags_leading; /**< Opening 0x7E count before this body (saturates 255). */
    uint8_t retries;       /**< 0 none; 1 F=1; 2 soft-single; 3 16-weakest pair; 4 adj-2; 5 adj-3; 6 two-sep. */
    uint8_t reserved[2];
} modem_ax25_rx_meta_t;

/**
 * @brief One deframed HDLC body plus optional UI parse.
 *
 * hdlc[] is Address .. Info .. FCS (no flags). Pointers (hdlc, dst, src,
 * via, info) are valid only during the callback; copy if needed. UI fields
 * are empty strings / 0 when the body is not a parseable UI frame.
 */
typedef struct modem_ax25_rx_frame {
    const uint8_t *hdlc; /**< Address .. Info .. FCS; no flags. */
    size_t hdlc_len;
    modem_ax25_rx_meta_t meta;

    const char *dst;     /**< Dest callsign[-SSID], or "". */
    const char *src;     /**< Source callsign[-SSID], or "". */
    const char *via;     /**< Comma-separated digis, or "". */
    uint8_t ctrl;        /**< Control octet (UI is 0x03). */
    uint8_t pid;         /**< Protocol ID if UI; else 0. */
    const uint8_t *info; /**< Info field; NULL if unparsed. */
    size_t info_len;
} modem_ax25_rx_frame_t;

/**
 * @brief RX callback. Invoked from modem_ax25_rx_feed_i16 (same thread).
 * @param ctx User pointer from set_rx / set_rx_phy.
 * @param fr  Frame view; do not retain pointers after return.
 */
typedef void (*modem_ax25_rx_fn)(void *ctx, const modem_ax25_rx_frame_t *fr);

/**
 * @brief Allocate an AX.25 mode instance (heap).
 *
 * Default PHY is 1200 baud Bell 202. Free with modem_ax25_destroy unless
 * the pointer was passed to modem_set_mode (session then owns it). malloc
 * only here and in destroy; feed / render / process do not allocate.
 * @return New mode, or NULL on allocation failure.
 */
modem_mode_t *modem_mode_ax25_create(void);

/**
 * @brief Bind @p mode to a process-lifetime static impl (no malloc).
 *
 * Pico TNC uses this. Do not call create() on the same storage.
 * destroy() only shuts down DSP; it does not free @p mode.
 * @param mode Caller-owned mode object.
 * @return 0 on success, −1 if mode is NULL.
 */
int modem_mode_ax25_init(modem_mode_t *mode);

/**
 * @brief Free an instance from modem_mode_ax25_create.
 *
 * NULL-safe. Do not call if the pointer was given to modem_set_mode.
 * @param mode Instance, or NULL.
 */
void modem_ax25_destroy(modem_mode_t *mode);

/**
 * @brief Configure RX with default Bell 202 geometry at @p sample_rate_hz.
 *
 * Equivalent to set_rx_phy with sample_rate_hz set and other fields 0.
 * @param mode            AX.25 mode from create().
 * @param sample_rate_hz  Must be MODEM_AX25_RX_FS_HZ (48000).
 * @param cb              Frame callback; may be NULL (demod still runs).
 * @param cb_ctx          Passed to @p cb.
 * @return 0 on success, −1 on bad args or DSP init failure.
 */
int modem_mode_ax25_set_rx(modem_mode_t *mode, float sample_rate_hz,
                           modem_ax25_rx_fn cb, void *cb_ctx);

/**
 * @brief Configure RX with a PHY profile and callback.
 *
 * @p phy NULL uses modem_ax25_phy_bell202_48k (fix_bits = 0). A second
 * call resets RX state and replaces the callback.
 * @param mode   AX.25 mode from create().
 * @param phy    Profile, or NULL for Bell 202 @ 48 kHz.
 * @param cb     Frame callback; may be NULL.
 * @param cb_ctx Passed to @p cb.
 * @return 0 on success, −1 on bad args, fs mismatch, or DSP init failure.
 */
int modem_mode_ax25_set_rx_phy(modem_mode_t *mode, const modem_ax25_phy_t *phy,
                               modem_ax25_rx_fn cb, void *cb_ctx);

/**
 * @brief Feed PCM16. Q30 = s16<<15. No-op until set_rx succeeds.
 *
 * Not thread-safe vs other calls on the same instance. The frame
 * callback runs before this returns.
 * @param mode    AX.25 mode.
 * @param samples Mono int16 PCM.
 * @param count   Sample count; ignored if ≤ 0.
 */
void modem_ax25_rx_feed_i16(modem_mode_t *mode, const int16_t *samples,
                            int count);

/**
 * @brief Reset RX DSP, HDLC hunter, and lock state.
 *
 * Does not clear the callback or PHY. Demod stays configured.
 * @param mode AX.25 mode.
 */
void modem_ax25_rx_reset(modem_mode_t *mode);

/**
 * @brief Offline / session TX amplitude and leading-flag duration.
 */
typedef struct modem_ax25_tx_opts {
    float amp;         /**< Peak as a fraction of MODEM_PWM_MID; ≤ MODEM_AMP_SUM_MAX. */
    float txdelay_sec; /**< Leading 0x7E duration before Address. */
} modem_ax25_tx_opts_t;

/**
 * @brief Fill amp = 0.35, txdelay = 0.300 s (FM PTT rise).
 * @param opt Options to overwrite. No-op if NULL.
 */
void modem_ax25_tx_opts_init(modem_ax25_tx_opts_t *opt);

/**
 * @brief Parsed UI fields from an HDLC body that includes FCS.
 *
 * dst/src/via are copies. info points into the caller's hdlc (not copied)
 * and is valid only while that buffer lives.
 */
typedef struct modem_ax25_ui {
    char dst[MODEM_AX25_CALL_MAX];
    char src[MODEM_AX25_CALL_MAX];
    char via[MODEM_AX25_VIA_MAX];
    uint8_t ctrl;        /**< Control octet (UI is 0x03). */
    uint8_t pid;         /**< Protocol ID if UI; else 0. */
    const uint8_t *info; /**< Info field; NULL if parse failed. */
    size_t info_len;
} modem_ax25_ui_t;

/**
 * @brief Pack dest/src/via + UI ctrl 0x03 + PID + info + FCS.
 *
 * via NULL or "" means no digipeaters. Does not need a mode instance.
 * @param out      Output buffer (Address..FCS, no flags).
 * @param max      Capacity of @p out; need ≤ MODEM_AX25_HDLC_MAX.
 * @param dst      Dest callsign[-SSID].
 * @param src      Source callsign[-SSID].
 * @param via      Comma-separated digis, or NULL / "".
 * @param pid      Protocol ID (APRS-style Info uses 0xF0).
 * @param info     Info octets; NULL allowed if info_len is 0.
 * @param info_len Info length (≤ MODEM_AX25_INFO_MAX).
 * @return Bytes written, or −1.
 */
int modem_ax25_pack_ui(uint8_t *out, size_t max, const char *dst, const char *src,
                       const char *via, uint8_t pid, const uint8_t *info,
                       size_t info_len);

/**
 * @brief Parse dest/src/via/ctrl/PID/info from an HDLC body that includes FCS.
 *
 * Does not check FCS. info aliases into @p hdlc.
 * @param hdlc Body Address..FCS.
 * @param len  Byte count (must include 2-byte FCS).
 * @param out  Result; zeroed on failure.
 * @return 0 if the address walk succeeded, −1 otherwise.
 */
int modem_ax25_parse_ui(const uint8_t *hdlc, size_t len, modem_ax25_ui_t *out);

/**
 * @brief Render an HDLC body (Address..FCS) to analog float PCM.
 *
 * Peak = opt->amp. No session / PTT. sample_rate_hz ≤ 0 → MODEM_AX25_RX_FS_HZ.
 * fs must divide the instance baud (create default 1200; else last set_rx_phy).
 * Phase is continuous across symbol boundaries (cosine NCO).
 * @param mode           AX.25 mode.
 * @param hdlc           Address .. Info .. FCS (no flags); length 3…MODEM_AX25_HDLC_MAX.
 * @param len            Byte count of @p hdlc.
 * @param opt            TX options; NULL → defaults.
 * @param sample_rate_hz PCM rate; ≤ 0 → 48000.
 * @param out            Output buffer.
 * @param max            Capacity of @p out in samples.
 * @return Sample count written, or −1 if args / buffer / queue fail.
 */
int modem_ax25_render_hdlc(modem_mode_t *mode, const uint8_t *hdlc, size_t len,
                           const modem_ax25_tx_opts_t *opt, float sample_rate_hz,
                           float *out, int max);

/**
 * @brief Pack a UI frame then render to float PCM.
 *
 * Control is 0x03 (UI). via NULL or "" means no digipeaters.
 * @param mode           AX.25 mode.
 * @param dst            Dest callsign[-SSID].
 * @param src            Source callsign[-SSID].
 * @param via            Comma-separated digis, or NULL / "".
 * @param pid            Protocol ID (APRS-style Info uses 0xF0).
 * @param info           Info octets; may be NULL if info_len is 0.
 * @param info_len       Info length; max MODEM_AX25_INFO_MAX.
 * @param opt            TX options; NULL → defaults.
 * @param sample_rate_hz PCM rate; ≤ 0 → 48000.
 * @param out            Output buffer.
 * @param max            Capacity of @p out in samples.
 * @return Sample count written, or −1 on pack / render failure.
 */
int modem_ax25_render_ui(modem_mode_t *mode, const char *dst, const char *src,
                         const char *via, uint8_t pid, const uint8_t *info,
                         size_t info_len, const modem_ax25_tx_opts_t *opt,
                         float sample_rate_hz, float *out, int max);

/**
 * @brief Default PN15 payload length after the seq u32 (bytes).
 *
 * Test / interop helper, not part of the on-air protocol.
 * Layout: seq (u32 LE) + packed PN15 bits.
 */
#define MODEM_AX25_PN_BYTES_DEFAULT 32

/**
 * @brief Fill info[] with seq + PN15 bytes (seed PIXDSP_PN15_SEED_DEFAULT).
 *
 * Test helper (modem/tests, Host over-air). Not required for a TNC/APRS app.
 * @param info     Output buffer.
 * @param max      Capacity of @p info.
 * @param seq      Sequence number stored little-endian in the first 4 bytes.
 * @param pn_bytes PN15 payload length after seq (1…MODEM_AX25_INFO_MAX−4).
 * @return Total length written (4 + pn_bytes), or −1.
 */
int modem_ax25_pn_fill(uint8_t *info, size_t max, uint32_t seq, int pn_bytes);

/**
 * @brief Same as modem_ax25_pn_fill with an explicit PN15 seed (test helper).
 * @param info     Output buffer.
 * @param max      Capacity of @p info.
 * @param seq      Sequence number (u32 LE).
 * @param pn_bytes PN15 payload length after seq.
 * @param seed     LFSR seed; 0 → PIXDSP_PN15_SEED_DEFAULT.
 * @return Total length written, or −1.
 */
int modem_ax25_pn_fill_seed(uint8_t *info, size_t max, uint32_t seq, int pn_bytes,
                            uint16_t seed);

/**
 * @brief Check a PN payload filled by modem_ax25_pn_fill (test helper).
 * @param info        Received Info field.
 * @param len         Length of @p info (must be ≥ 5).
 * @param seq_out     Optional; set to the decoded seq.
 * @param bit_err_out Optional; set to Hamming distance on PN bits.
 * @return 0 if every PN bit matches, −1 if short or any bit error.
 */
int modem_ax25_pn_check(const uint8_t *info, size_t len, uint32_t *seq_out,
                        int *bit_err_out);

/**
 * @brief Same as modem_ax25_pn_check with an explicit PN15 seed (test helper).
 * @param info        Received Info field.
 * @param len         Length of @p info.
 * @param seed        LFSR seed; 0 → PIXDSP_PN15_SEED_DEFAULT.
 * @param seq_out     Optional; decoded seq.
 * @param bit_err_out Optional; PN bit errors.
 * @return 0 if PN matches, −1 if short or any bit error.
 */
int modem_ax25_pn_check_seed(const uint8_t *info, size_t len, uint16_t seed,
                             uint32_t *seq_out, int *bit_err_out);

/**
 * @brief Optional log sink (RX LOCK / FRAME, TX queue / pcm).
 * @param ctx User pointer from modem_ax25_set_log.
 * @param msg Timestamped line, valid only during the call.
 */
typedef void (*modem_ax25_log_fn)(void *ctx, const char *msg);

/**
 * @brief Install a log callback. NULL fn disables logging.
 * @param mode AX.25 mode.
 * @param fn   Sink, or NULL.
 * @param ctx  Passed to @p fn.
 */
void modem_ax25_set_log(modem_mode_t *mode, modem_ax25_log_fn fn, void *ctx);

/**
 * @brief RX snapshot for diagnostics.
 *
 * state follows HDLC flags (≥3 opening flags → DEMOD), not PLL-DCD
 * (pixdsp_dpll_dcd). Demod always runs in both states.
 */
typedef struct modem_ax25_rx_status {
    int state;          /**< 0 = SEARCH, 1 = DEMOD after ≥3 HDLC flags. */
    int64_t pcm_samples; /**< Samples consumed since last reset. */
    float rms;          /**< RMS of the last feed chunk. */
    int hdlc_flags;     /**< flags_leading (same count that sets state). */
    int hdlc_len;       /**< Bytes assembled in the current body. */
} modem_ax25_rx_status_t;

/**
 * @brief Copy a RX status snapshot.
 * @param mode AX.25 mode.
 * @param out  Destination; zeroed then filled.
 * @return 0 on success, −1 if mode or out is NULL.
 */
int modem_ax25_rx_status(modem_mode_t *mode, modem_ax25_rx_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODEM_MODE_AX25_H */
