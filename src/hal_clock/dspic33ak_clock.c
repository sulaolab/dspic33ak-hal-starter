#include "dspic33ak_clock.h"
#include "dspic33ak_clock_device.h"
#include "dspic33ak_clock_reg.h"

#include <stddef.h>

#define DSPIC33AK_CLOCK_PLLI_MIN_HZ      (5000000UL)
#define DSPIC33AK_CLOCK_PLLI_MAX_HZ      (64000000UL)
#define DSPIC33AK_CLOCK_VCO_MIN_HZ       (500000000UL)
#define DSPIC33AK_CLOCK_VCO_MAX_HZ       (1600000000UL)
#define DSPIC33AK_CLOCK_OUTPUT_MAX_HZ    (800000000UL)
#define DSPIC33AK_CLOCK_PLLFBDIV_MIN     (16UL)
#define DSPIC33AK_CLOCK_PLLFBDIV_MAX     (400UL)
#define DSPIC33AK_CLOCK_PLLPRE_MIN       (1U)
#define DSPIC33AK_CLOCK_PLLPRE_MAX       (15U)
#define DSPIC33AK_CLOCK_PLLPOST_MIN      (1U)
#define DSPIC33AK_CLOCK_PLLPOST_MAX      (7U)
#define DSPIC33AK_CLOCK_CLKGEN_FRAC_HALF (256U)

typedef struct {
    uint32_t feedback_div;
    uint16_t post_div1;
    uint16_t post_div2;
    uint16_t pre_div;
    uint32_t output_hz;
} dspic33ak_clock_pll_solution_t;

/* --------------------------------------------------------------------------
 * Local helper prototypes
 *
 * Public Clock HAL entry points stay at the top of this file.  Static helpers
 * below perform validation, clock-math solving, and internal register-layer
 * request construction; raw SFR access lives in dspic33ak_clock_reg.c.
 * -------------------------------------------------------------------------- */

static uint16_t clkgen_integer_divider_intdiv(uint16_t divide_by);
static uint16_t clkgen_integer_divider_fracdiv(uint16_t divide_by);
static dspic33ak_clock_status_t solve_pll(
    const dspic33ak_clock_pll_config_t *config,
    dspic33ak_clock_pll_solution_t *solution);
static dspic33ak_clock_status_t configure_pll(
    dspic33ak_clock_pll_t pll,
    const dspic33ak_clock_pll_config_t *config,
    uint32_t *actual_hz);
static dspic33ak_clock_status_t configure_clkgen(
    dspic33ak_clock_clkgen_t clkgen,
    dspic33ak_clock_source_t source,
    uint16_t divide_by);

/* ========================================================================== */
/* 1. Public API                                                              */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* Configure PLL from a logical clock request                                 */
/* -------------------------------------------------------------------------- */
dspic33ak_clock_status_t
dspic33ak_clock_pll_configure(
    dspic33ak_clock_pll_t pll,
    const dspic33ak_clock_pll_config_t *config,
    uint32_t *actual_hz)
{
    if (config == NULL) {
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }

    switch (pll) {
    case DSPIC33AK_CLOCK_PLL_1:
    case DSPIC33AK_CLOCK_PLL_2:
        return configure_pll(pll, config, actual_hz);
    default:
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }
}

/* -------------------------------------------------------------------------- */
/* Configure CLKGEN from a logical clock request                              */
/* -------------------------------------------------------------------------- */
dspic33ak_clock_status_t
dspic33ak_clock_clkgen_configure(
    dspic33ak_clock_clkgen_t clkgen,
    const dspic33ak_clock_clkgen_config_t *config)
{
    if (config == NULL || config->divide_by == 0u) {
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }

    return configure_clkgen(clkgen, config->source, config->divide_by);
}

/* ========================================================================== */
/* 2. Local helpers                                                           */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* Convert integer CLKGEN divider to INTDIV field                             */
/* -------------------------------------------------------------------------- */
static uint16_t clkgen_integer_divider_intdiv(uint16_t divide_by)
{
    if (divide_by == 1u) {
        return 0u;
    }

    return (uint16_t)(divide_by / 2u);
}

/* -------------------------------------------------------------------------- */
/* Convert integer CLKGEN divider to FRACDIV field                            */
/* -------------------------------------------------------------------------- */
static uint16_t clkgen_integer_divider_fracdiv(uint16_t divide_by)
{
    if ((divide_by <= 1u) || ((divide_by & 1u) == 0u)) {
        return 0u;
    }

    return DSPIC33AK_CLOCK_CLKGEN_FRAC_HALF;
}

/* -------------------------------------------------------------------------- */
/* Solve PLL divider fields for an exact target frequency                     */
/* -------------------------------------------------------------------------- */
static dspic33ak_clock_status_t solve_pll(
    const dspic33ak_clock_pll_config_t *config,
    dspic33ak_clock_pll_solution_t *solution)
{
    uint16_t pre_div;
    uint16_t post_div2;
    uint16_t post_div1;

    if (config == NULL || solution == NULL ||
        config->input_hz == 0u || config->target_hz == 0u) {
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }

    for (pre_div = DSPIC33AK_CLOCK_PLLPRE_MIN;
         pre_div <= DSPIC33AK_CLOCK_PLLPRE_MAX;
         pre_div++) {
        const uint64_t input_hz = config->input_hz;

        if (input_hz < ((uint64_t)DSPIC33AK_CLOCK_PLLI_MIN_HZ * pre_div) ||
            input_hz > ((uint64_t)DSPIC33AK_CLOCK_PLLI_MAX_HZ * pre_div)) {
            continue;
        }

        for (post_div2 = DSPIC33AK_CLOCK_PLLPOST_MIN;
             post_div2 <= DSPIC33AK_CLOCK_PLLPOST_MAX;
             post_div2++) {
            for (post_div1 = DSPIC33AK_CLOCK_PLLPOST_MIN;
                 post_div1 <= DSPIC33AK_CLOCK_PLLPOST_MAX;
                 post_div1++) {
                const uint64_t post_product = (uint64_t)post_div1 * post_div2;
                const uint64_t numerator =
                    (uint64_t)config->target_hz * pre_div * post_product;
                uint64_t feedback_div;
                uint64_t vco_numerator;

                if (post_div1 < post_div2) {
                    continue;
                }
                if ((uint64_t)config->target_hz > DSPIC33AK_CLOCK_OUTPUT_MAX_HZ) {
                    continue;
                }
                if ((numerator % input_hz) != 0u) {
                    continue;
                }

                feedback_div = numerator / input_hz;
                if (feedback_div < DSPIC33AK_CLOCK_PLLFBDIV_MIN ||
                    feedback_div > DSPIC33AK_CLOCK_PLLFBDIV_MAX) {
                    continue;
                }

                vco_numerator = input_hz * feedback_div;
                if (vco_numerator < ((uint64_t)DSPIC33AK_CLOCK_VCO_MIN_HZ * pre_div) ||
                    vco_numerator > ((uint64_t)DSPIC33AK_CLOCK_VCO_MAX_HZ * pre_div)) {
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

/* -------------------------------------------------------------------------- */
/* Configure one PLL through the internal register layer                       */
/* -------------------------------------------------------------------------- */
static dspic33ak_clock_status_t configure_pll(
    dspic33ak_clock_pll_t pll,
    const dspic33ak_clock_pll_config_t *config,
    uint32_t *actual_hz)
{
    dspic33ak_clock_pll_solution_t solution;
    dspic33ak_clock_reg_pll_config_t reg_config;
    dspic33ak_clock_status_t status;
    uint16_t source;

    if (!dspic33ak_clock_device_encode_pll_source(config->source, &source)) {
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }

    status = solve_pll(config, &solution);
    if (status != DSPIC33AK_CLOCK_OK) {
        return status;
    }

    reg_config.source = source;
    reg_config.feedback_div = solution.feedback_div;
    reg_config.pre_div = solution.pre_div;
    reg_config.post_div1 = solution.post_div1;
    reg_config.post_div2 = solution.post_div2;

    status = dspic33ak_clock_reg_pll_configure(pll, &reg_config);
    if (status != DSPIC33AK_CLOCK_OK) {
        return status;
    }

    if (actual_hz != NULL) {
        *actual_hz = solution.output_hz;
    }

    return DSPIC33AK_CLOCK_OK;
}

/* -------------------------------------------------------------------------- */
/* Configure one CLKGEN through the internal register layer                    */
/* -------------------------------------------------------------------------- */
static dspic33ak_clock_status_t configure_clkgen(
    dspic33ak_clock_clkgen_t clkgen,
    dspic33ak_clock_source_t source,
    uint16_t divide_by)
{
    dspic33ak_clock_reg_clkgen_config_t reg_config;
    uint16_t encoded_source;

    if (!dspic33ak_clock_device_encode_clkgen_source(source, &encoded_source)) {
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }

    reg_config.source = encoded_source;
    reg_config.intdiv = clkgen_integer_divider_intdiv(divide_by);
    reg_config.fracdiv = clkgen_integer_divider_fracdiv(divide_by);

    return dspic33ak_clock_reg_clkgen_configure(clkgen, &reg_config);
}
