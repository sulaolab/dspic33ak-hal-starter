#ifndef DSPIC33AK_CLOCK_RESTART_H
#define DSPIC33AK_CLOCK_RESTART_H

#include "dspic33ak_clock.h"

/*
 * dspic33ak_clock_restart.h
 * --------------------------
 * Experimental PLL2 forced-stop/reprogram/relock path for
 * exp/pll2-soft-reset-restart. Added alongside (not inside) the pinned
 * dspic33ak_clock.* / dspic33ak_clock_reg.* files so that six-file byte
 * identity with upstream is preserved; see docs/clock_hal_integration.md.
 *
 * dspic33ak_clock_pll_configure() assumes the PLL either starts from a known
 * device-reset-default state or is already running on the SAME reference
 * source it is being asked to reconfigure to; it programs new dividers, then
 * switches NOSC, in that order. That ordering is exactly what this experiment
 * exists to question for a PLL that may be left "hot" (locked to a different
 * reference, or mid-handshake) across a software reset. dspic33ak_clock_pll_restart()
 * instead: force the PLL fully off first, verify it is actually off (including
 * that no PLLSWEN/FOUTSWEN/OSWEN/DIVSWEN request bit is left set), THEN program
 * dividers and NOSC while off, THEN re-enable and re-lock. See
 * docs/pll2_soft_reset_restart_experiment.md for the full protocol and results.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PLL_RESTART_STAGE_NONE = 0,
    PLL_RESTART_STAGE_SOURCE_READY,
    PLL_RESTART_STAGE_FORCE_OFF,
    PLL_RESTART_STAGE_WAIT_OFF,
    /* Not in the original stage sketch: a dedicated checkpoint for the
     * post-stop PLLSWEN/FOUTSWEN/OSWEN/DIVSWEN leftover-bit check (spec
     * section 6.2). Kept as its own stage so a stale-handshake failure is
     * distinguishable from an OFF-poll timeout, both of which land on
     * PLL_RESTART_STAGE_WAIT_OFF otherwise. */
    PLL_RESTART_STAGE_CHECK_STALE_BITS,
    PLL_RESTART_STAGE_PROGRAM_DIVIDERS,
    PLL_RESTART_STAGE_ENABLE,
    PLL_RESTART_STAGE_PLLSWEN,
    PLL_RESTART_STAGE_FOUTSWEN,
    PLL_RESTART_STAGE_OSWEN,
    PLL_RESTART_STAGE_WAIT_LOCK,
    PLL_RESTART_STAGE_VERIFY,
    PLL_RESTART_STAGE_DONE
} pll_restart_stage_t;

typedef struct {
    dspic33ak_clock_status_t status;
    pll_restart_stage_t last_stage;
    uint32_t actual_hz;

    uint32_t stop_time_us;
    uint32_t pllswen_time_us;
    uint32_t foutswen_time_us;
    uint32_t oswen_time_us;
    uint32_t lock_time_us;

    /* PLLSWEN/FOUTSWEN/OSWEN/DIVSWEN observed right after the PLL was
     * confirmed off, before this call clears or sets anything else. Non-zero
     * here means the force-stop did not return the PLL to a clean handshake
     * state; dspic33ak_clock_pll_restart() then fails at
     * PLL_RESTART_STAGE_CHECK_STALE_BITS without issuing PROGRAM_DIVIDERS or
     * any further request bit. */
    uint8_t post_stop_pllswen;
    uint8_t post_stop_foutswen;
    uint8_t post_stop_oswen;
    uint8_t post_stop_divswen;
} dspic33ak_clock_pll_restart_report_t;

/*
 * Force-stop, fully reprogram every PLL2DIV field and NOSC, and re-lock the
 * given PLL -- regardless of whether it was off, on-and-locked, or left
 * mid-handshake from before a software reset.
 *
 * PLL1 is intentionally refused (DSPIC33AK_CLOCK_ERR_NOT_SUPPORTED): PLL1
 * drives the live SYSCLK on this starter, and force-stopping it is out of
 * scope for this experiment (see docs/pll2_soft_reset_restart_experiment.md
 * section 3).
 *
 * actual_hz is optional and is written only on DSPIC33AK_CLOCK_OK.
 * dspic33ak_clock_pll_restart_last_report() returns stage/timing detail for
 * the most recent call, including failed calls; the pointer is to a static
 * instance owned by this module and is valid until the next call.
 *
 * On any stage timeout or a stale-handshake failure, this function stops
 * immediately at that stage: it does not issue the next handshake bit on top
 * of a failed one (see fail-fast rule, experiment spec section 6.5 / 16).
 */
dspic33ak_clock_status_t
dspic33ak_clock_pll_restart(
    dspic33ak_clock_pll_t pll,
    const dspic33ak_clock_pll_config_t *config,
    uint32_t *actual_hz);

const dspic33ak_clock_pll_restart_report_t *
dspic33ak_clock_pll_restart_last_report(void);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_CLOCK_RESTART_H */
