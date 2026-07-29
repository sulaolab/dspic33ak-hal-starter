#include "dspic33ak_clock_restart.h"
#include "dspic33ak_clock_device.h"
#include "dspic33ak_high_res_timer.h"

#include <xc.h>
#include <stddef.h>

/*
 * Device PLL facts, duplicated from dspic33ak_clock.c's solve_pll(). That
 * function is file-static, and dspic33ak_clock.c is one of the six files
 * pinned byte-identical to upstream (docs/clock_hal_integration.md), so it is
 * not extended here. This experiment only ever asks for one operating point
 * (FRC 8 MHz -> PLL2 520 MHz), but the restart API takes a general
 * dspic33ak_clock_pll_config_t, so the solver stays general too rather than
 * hardcoding that one answer.
 */
#define RESTART_PLLI_MIN_HZ      (5000000UL)
#define RESTART_PLLI_MAX_HZ      (64000000UL)
#define RESTART_VCO_MIN_HZ       (500000000UL)
#define RESTART_VCO_MAX_HZ       (1600000000UL)
#define RESTART_OUTPUT_MAX_HZ    (800000000UL)
#define RESTART_PLLFBDIV_MIN     (16UL)
#define RESTART_PLLFBDIV_MAX     (400UL)
#define RESTART_PLLPRE_MIN       (1U)
#define RESTART_PLLPRE_MAX       (15U)
#define RESTART_PLLPOST_MIN      (1U)
#define RESTART_PLLPOST_MAX      (7U)

#define RESTART_POLL_LIMIT       (1000000UL)

typedef struct {
    uint32_t feedback_div;
    uint16_t post_div1;
    uint16_t post_div2;
    uint16_t pre_div;
    uint32_t output_hz;
} restart_pll_solution_t;

static dspic33ak_clock_pll_restart_report_t s_last_report;

static dspic33ak_clock_status_t solve_pll_dividers(
    const dspic33ak_clock_pll_config_t *config,
    restart_pll_solution_t *solution)
{
    uint16_t pre_div;
    uint16_t post_div2;
    uint16_t post_div1;

    if ((config->input_hz == 0u) || (config->target_hz == 0u)) {
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }

    for (pre_div = RESTART_PLLPRE_MIN; pre_div <= RESTART_PLLPRE_MAX; pre_div++) {
        const uint64_t input_hz = config->input_hz;

        if (input_hz < ((uint64_t)RESTART_PLLI_MIN_HZ * pre_div) ||
            input_hz > ((uint64_t)RESTART_PLLI_MAX_HZ * pre_div)) {
            continue;
        }

        for (post_div2 = RESTART_PLLPOST_MIN; post_div2 <= RESTART_PLLPOST_MAX; post_div2++) {
            for (post_div1 = RESTART_PLLPOST_MIN; post_div1 <= RESTART_PLLPOST_MAX; post_div1++) {
                const uint64_t post_product = (uint64_t)post_div1 * post_div2;
                const uint64_t numerator =
                    (uint64_t)config->target_hz * pre_div * post_product;
                uint64_t feedback_div;
                uint64_t vco_numerator;

                if (post_div1 < post_div2) {
                    continue;
                }
                if ((uint64_t)config->target_hz > RESTART_OUTPUT_MAX_HZ) {
                    continue;
                }
                if ((numerator % input_hz) != 0u) {
                    continue;
                }

                feedback_div = numerator / input_hz;
                if (feedback_div < RESTART_PLLFBDIV_MIN ||
                    feedback_div > RESTART_PLLFBDIV_MAX) {
                    continue;
                }

                vco_numerator = input_hz * feedback_div;
                if (vco_numerator < ((uint64_t)RESTART_VCO_MIN_HZ * pre_div) ||
                    vco_numerator > ((uint64_t)RESTART_VCO_MAX_HZ * pre_div)) {
                    continue;
                }

                solution->feedback_div = (uint32_t)feedback_div;
                solution->post_div1 = post_div1;
                solution->post_div2 = post_div2;
                solution->pre_div = pre_div;
                solution->output_hz = config->target_hz;
                return DSPIC33AK_CLOCK_OK;
            }
        }
    }

    return DSPIC33AK_CLOCK_ERR_UNREPRESENTABLE;
}

static dspic33ak_clock_status_t fail(
    dspic33ak_clock_pll_restart_report_t *report,
    pll_restart_stage_t stage,
    dspic33ak_clock_status_t status)
{
    report->last_stage = stage;
    report->status = status;
    return status;
}

dspic33ak_clock_status_t
dspic33ak_clock_pll_restart(
    dspic33ak_clock_pll_t pll,
    const dspic33ak_clock_pll_config_t *config,
    uint32_t *actual_hz)
{
    dspic33ak_clock_pll_restart_report_t *report = &s_last_report;
    restart_pll_solution_t solution;
    uint16_t nosc;
    uint32_t poll;
    uint32_t t0;
    uint32_t t1;
    dspic33ak_clock_status_t status;

    report->status = DSPIC33AK_CLOCK_OK;
    report->last_stage = PLL_RESTART_STAGE_NONE;
    report->actual_hz = 0u;
    report->stop_time_us = 0u;
    report->pllswen_time_us = 0u;
    report->foutswen_time_us = 0u;
    report->oswen_time_us = 0u;
    report->lock_time_us = 0u;
    report->post_stop_pllswen = 0u;
    report->post_stop_foutswen = 0u;
    report->post_stop_oswen = 0u;
    report->post_stop_divswen = 0u;

    if (pll == DSPIC33AK_CLOCK_PLL_1) {
        return fail(report, PLL_RESTART_STAGE_NONE, DSPIC33AK_CLOCK_ERR_NOT_SUPPORTED);
    }
    if (pll != DSPIC33AK_CLOCK_PLL_2) {
        return fail(report, PLL_RESTART_STAGE_NONE, DSPIC33AK_CLOCK_ERR_INVALID_ARG);
    }
    if (config == NULL) {
        return fail(report, PLL_RESTART_STAGE_NONE, DSPIC33AK_CLOCK_ERR_INVALID_ARG);
    }
    if (!dspic33ak_clock_device_encode_pll_source(config->source, &nosc)) {
        return fail(report, PLL_RESTART_STAGE_NONE, DSPIC33AK_CLOCK_ERR_INVALID_ARG);
    }

    report->last_stage = PLL_RESTART_STAGE_SOURCE_READY;

    /* ---- Force stop ---- */
    report->last_stage = PLL_RESTART_STAGE_FORCE_OFF;
    t0 = dspic33ak_high_res_timer_get_count();
    PLL2CONbits.ON = 0;
    OSCCTRLbits.PLL2EN = 0;

    report->last_stage = PLL_RESTART_STAGE_WAIT_OFF;
    poll = RESTART_POLL_LIMIT;
    while ((OSCCTRLbits.PLL2RDY != 0u) || (PLL2CONbits.CLKRDY != 0u)) {
        if (--poll == 0u) {
            return fail(report, PLL_RESTART_STAGE_WAIT_OFF, DSPIC33AK_CLOCK_ERR_TIMEOUT);
        }
    }
    t1 = dspic33ak_high_res_timer_get_count();
    report->stop_time_us = dspic33ak_high_res_timer_elapsed_us(t0);
    (void)t1;

    /* ---- Check for a stale handshake left over from before the stop ----
     * Recorded unconditionally, not cleared here: a non-zero bit means the
     * PLL did not return to a clean state, and this call fails right here
     * rather than proceeding to reprogram dividers on top of it. */
    report->last_stage = PLL_RESTART_STAGE_CHECK_STALE_BITS;
    report->post_stop_pllswen  = (uint8_t)PLL2CONbits.PLLSWEN;
    report->post_stop_foutswen = (uint8_t)PLL2CONbits.FOUTSWEN;
    report->post_stop_oswen    = (uint8_t)PLL2CONbits.OSWEN;
    report->post_stop_divswen  = (uint8_t)PLL2CONbits.DIVSWEN;
    if ((report->post_stop_pllswen != 0u) || (report->post_stop_foutswen != 0u) ||
        (report->post_stop_oswen != 0u) || (report->post_stop_divswen != 0u)) {
        return fail(report, PLL_RESTART_STAGE_CHECK_STALE_BITS, DSPIC33AK_CLOCK_ERR_TIMEOUT);
    }

    /* ---- Program every divider field, and NOSC, while PLL2 is off ---- */
    report->last_stage = PLL_RESTART_STAGE_PROGRAM_DIVIDERS;
    status = solve_pll_dividers(config, &solution);
    if (status != DSPIC33AK_CLOCK_OK) {
        return fail(report, PLL_RESTART_STAGE_PROGRAM_DIVIDERS, status);
    }
    PLL2DIVbits.PLLFBDIV = solution.feedback_div;
    PLL2DIVbits.PLLPRE = solution.pre_div;
    PLL2DIVbits.POSTDIV1 = solution.post_div1;
    PLL2DIVbits.POSTDIV2 = solution.post_div2;
    PLL2CONbits.NOSC = nosc;

    /* ---- Re-enable ---- */
    report->last_stage = PLL_RESTART_STAGE_ENABLE;
    PLL2CONbits.ON = 1;
    PLL2CONbits.OE = 1;
    OSCCTRLbits.PLL2EN = 1;

    /* ---- PLLSWEN ---- */
    report->last_stage = PLL_RESTART_STAGE_PLLSWEN;
    t0 = dspic33ak_high_res_timer_get_count();
    PLL2CONbits.PLLSWEN = 1;
    poll = RESTART_POLL_LIMIT;
    while (PLL2CONbits.PLLSWEN != 0u) {
        if (--poll == 0u) {
            return fail(report, PLL_RESTART_STAGE_PLLSWEN, DSPIC33AK_CLOCK_ERR_TIMEOUT);
        }
    }
    report->pllswen_time_us = dspic33ak_high_res_timer_elapsed_us(t0);

    /* ---- FOUTSWEN ---- */
    report->last_stage = PLL_RESTART_STAGE_FOUTSWEN;
    t0 = dspic33ak_high_res_timer_get_count();
    PLL2CONbits.FOUTSWEN = 1;
    poll = RESTART_POLL_LIMIT;
    while (PLL2CONbits.FOUTSWEN != 0u) {
        if (--poll == 0u) {
            return fail(report, PLL_RESTART_STAGE_FOUTSWEN, DSPIC33AK_CLOCK_ERR_TIMEOUT);
        }
    }
    report->foutswen_time_us = dspic33ak_high_res_timer_elapsed_us(t0);

    /* ---- OSWEN ---- (NOSC was already programmed above, while off) */
    report->last_stage = PLL_RESTART_STAGE_OSWEN;
    t0 = dspic33ak_high_res_timer_get_count();
    PLL2CONbits.OSWEN = 1;
    poll = RESTART_POLL_LIMIT;
    while (PLL2CONbits.OSWEN != 0u) {
        if (--poll == 0u) {
            return fail(report, PLL_RESTART_STAGE_OSWEN, DSPIC33AK_CLOCK_ERR_TIMEOUT);
        }
    }
    report->oswen_time_us = dspic33ak_high_res_timer_elapsed_us(t0);

    /* ---- Wait for lock ---- */
    report->last_stage = PLL_RESTART_STAGE_WAIT_LOCK;
    poll = RESTART_POLL_LIMIT;
    while ((OSCCTRLbits.PLL2RDY == 0u) || (PLL2CONbits.CLKRDY == 0u)) {
        if (--poll == 0u) {
            return fail(report, PLL_RESTART_STAGE_WAIT_LOCK, DSPIC33AK_CLOCK_ERR_TIMEOUT);
        }
    }
    report->lock_time_us = dspic33ak_high_res_timer_elapsed_us(t0);

    /* ---- Verify the request bits settled and NOSC took effect ---- */
    report->last_stage = PLL_RESTART_STAGE_VERIFY;
    if ((PLL2CONbits.PLLSWEN != 0u) || (PLL2CONbits.FOUTSWEN != 0u) ||
        (PLL2CONbits.OSWEN != 0u) || (PLL2CONbits.DIVSWEN != 0u) ||
        (PLL2CONbits.COSC != nosc)) {
        return fail(report, PLL_RESTART_STAGE_VERIFY, DSPIC33AK_CLOCK_ERR_TIMEOUT);
    }

    report->last_stage = PLL_RESTART_STAGE_DONE;
    report->status = DSPIC33AK_CLOCK_OK;
    report->actual_hz = solution.output_hz;
    if (actual_hz != NULL) {
        *actual_hz = solution.output_hz;
    }
    return DSPIC33AK_CLOCK_OK;
}

const dspic33ak_clock_pll_restart_report_t *
dspic33ak_clock_pll_restart_last_report(void)
{
    return &s_last_report;
}
