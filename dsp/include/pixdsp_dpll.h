#ifndef PIXDSP_DPLL_H
#define PIXDSP_DPLL_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 *
 * pixdsp_dpll.h - This file is part of Griddick TNC.
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
 * @file pixdsp_dpll.h
 * @brief Bit-clock DPLL (Direwolf-style NCO + transition inertia + PLL-DCD).
 *
 * process() consumes one int32 demod_out per audio sample (sign only:
 * >0 mark, else space). Init still takes float params. Other TEDs
 * (Gardner, Mueller–Müller, …) can drive the same NCO later; they are
 * not implemented here.
 *
 * Inertia is 0.50 while unlocked and 0.74 once PLL-DCD latches, matching
 * Direwolf demod_afsk nudge_pll / pll_dcd_* (not HDLC flags).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PIXDSP_DPLL_INERTIA_LOCKED    0.74f
#define PIXDSP_DPLL_INERTIA_SEARCHING 0.50f

/* Direwolf fsk_demod_state.h pll_dcd_* for 1200 AFSK. */
#define PIXDSP_DPLL_DCD_THRESH_ON  30
#define PIXDSP_DPLL_DCD_THRESH_OFF 6
#define PIXDSP_DPLL_DCD_GOOD_ABS   536870912 /* 512 * 1024 * 1024 */

typedef struct pixdsp_dpll_params {
    float sample_rate_hz;
    float baud;
    float inertia;            /**< Locked (0, 1]; <=0 uses LOCKED. */
    float searching_inertia;  /**< Unlocked (0, 1]; <=0 uses SEARCHING. */
} pixdsp_dpll_params_t;

typedef struct pixdsp_dpll {
    int32_t pll;
    int32_t step;
    float inertia;
    float searching_inertia;
    int32_t locked_q;     /* * 10000; 10000 = identity */
    int32_t searching_q;
    int prev_sign;
    int have_prev;
    int good_flag;
    int bad_flag;
    uint8_t good_hist;
    uint8_t bad_hist;
    uint32_t score;
    int data_detect;
    int initialized;
} pixdsp_dpll_t;

/**
 * @brief Set NCO step = 2^32 · baud / fs and Q inertia (×10000).
 * @return 0, or −1 if fs/baud invalid or inertia > 1.
 */
int pixdsp_dpll_init(pixdsp_dpll_t *s, const pixdsp_dpll_params_t *params);
/** @brief Zero NCO, DCD histograms, and previous-sign. Keep step/inertia. */
void pixdsp_dpll_reset(pixdsp_dpll_t *s);

/**
 * @brief Extra delay before the next strobe, in audio samples.
 *
 * Subtracts phase_samples · step from the NCO (unsigned wrap). Used to
 * align a known symbol phase; AX.25 RX does not call this today.
 * @return 0, or −1.
 */
int pixdsp_dpll_seed_phase(pixdsp_dpll_t *s, int phase_samples);

/**
 * @brief 1 when PLL-DCD is latched (Direwolf data_detect).
 *
 * On when ≥30 of the last 32 symbol scores are “good”, off when ≤6.
 * Switches inertia only; it does not mute HDLC bits.
 */
int pixdsp_dpll_dcd(const pixdsp_dpll_t *s);

/**
 * @brief Float y → DPLL input. Do not cast y to int32 (values in (0,1)
 * become 0 and flip mark to space).
 */
static inline int32_t pixdsp_dpll_from_f32(float y)
{
    return (y > 0.0f) ? 1 : 0;
}

/**
 * @brief One audio sample of demod_out (int32; only the sign is used).
 *
 * Advances a signed 32-bit NCO by step. strobe=1 on +→− wrap (symbol
 * instant). On a sign change of demod_out, pll *= inertia (0.50 search /
 * 0.74 lock via DCD). *bit is (demod_out > 0); AX.25 RX ignores *bit and
 * slices y itself so y==0 is space.
 * @return 0, or −1.
 */
int pixdsp_dpll_process(pixdsp_dpll_t *s, int32_t demod_out, int *strobe,
                        int *bit);

#ifdef __cplusplus
}
#endif

#endif /* PIXDSP_DPLL_H */
