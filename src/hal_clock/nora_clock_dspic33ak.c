#include "nora_clock.h"
#include "nora_clock_dspic33ak.h"
#include "nora_clock_device.h"
#include "nora_clock_dspic33ak_reg.h"

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

/* Fosc -> Fcy on this family. PLL1 at 200 MHz gives 100 MIPS, which is what the
 * project's FCY == PLL1_CLK_HZ / 2 has always asserted; this is that same fact,
 * now owned by the HAL that programmed the clock instead of by a build header. */
#define DSPIC33AK_CLOCK_FOSC_TO_FCY_DIV  (2U)

/* Operating point recorded until a configure/switch call updates it: the family
 * comes out of reset on the FRC with no PLL. The boot path replaces this within
 * microseconds, so it matters only to code that runs before sonora_clock_boot_init().
 * Documented assumption from the family reset state, not a measured value. */
#define DSPIC33AK_CLOCK_DEFAULT_FOSC_HZ  (NORA_CLOCK_FRC_HZ)

typedef struct {
    uint32_t feedback_div;
    uint16_t post_div1;
    uint16_t post_div2;
    uint16_t pre_div;
    uint32_t output_hz;
} dspic33ak_clock_pll_solution_t;

/*
 * What this HAL has programmed, so nora_clock_get_fcy_hz() can be authoritative
 * rather than a compile-time guess.
 *
 * s_pll_output_hz is indexed by (pll - 1) and is 0 for a PLL nobody configured.
 * It exists so that configuring CLKGEN1 -- the CPU's own generator, i.e. Fosc on
 * this family -- can derive the resulting system frequency without asking the
 * caller to state it a second time and risk the two disagreeing.
 */
static uint32_t s_pll_output_hz[2] = { 0u, 0u };
static uint32_t s_fosc_hz          = DSPIC33AK_CLOCK_DEFAULT_FOSC_HZ;

/* --------------------------------------------------------------------------
 * Local helper prototypes
 *
 * Public Clock HAL entry points stay at the top of this file.  Static helpers
 * below perform validation, clock-math solving, and internal register-layer
 * request construction; raw SFR access lives in nora_clock_dspic33ak_reg.c.
 * -------------------------------------------------------------------------- */

static uint16_t clkgen_integer_divider_intdiv(uint16_t divide_by);
static uint16_t clkgen_integer_divider_fracdiv(uint16_t divide_by);
static uint32_t source_hz_if_known(nora_clock_source_t source);
static nora_clock_status_t solve_pll(
    const nora_clock_pll_config_t *config,
    dspic33ak_clock_pll_solution_t *solution);
static nora_clock_status_t configure_pll(
    nora_clock_pll_t pll,
    const nora_clock_pll_config_t *config,
    uint32_t *actual_hz);
static nora_clock_status_t configure_clkgen(
    nora_clock_dspic33ak_clkgen_t clkgen,
    nora_clock_source_t source,
    uint16_t divide_by);

/* ========================================================================== */
/* 1. Public API                                                              */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* Configure PLL from a logical clock request                                 */
/* -------------------------------------------------------------------------- */
nora_clock_status_t
nora_clock_pll_configure(
    nora_clock_pll_t pll,
    const nora_clock_pll_config_t *config,
    uint32_t *actual_hz)
{
    if (config == NULL) {
        return NORA_CLOCK_ERR_INVALID_ARG;
    }

    switch (pll) {
    case NORA_CLOCK_PLL_1:
    case NORA_CLOCK_PLL_2:
        return configure_pll(pll, config, actual_hz);
    default:
        return NORA_CLOCK_ERR_INVALID_ARG;
    }
}

/* -------------------------------------------------------------------------- */
/* Switch the system clock to a plain oscillator source (portable face)        */
/* -------------------------------------------------------------------------- */
/*
 * On this family the system clock is CLKGEN1's output, so "switch the system
 * clock" is "point CLKGEN1 at that oscillator, undivided". CK reaches the same
 * end through OSCCON NOSC + OSWEN; the caller sees one call either way.
 *
 * Only the four oscillators every family has are accepted. A PLL output is
 * reached through nora_clock_pll_configure() plus the AK CLKGEN calls, and the
 * AK-tree-only sources (the VCO fractional taps, the REFI input pins) are not
 * system-clock sources on any face this HAL wants to keep portable.
 */
nora_clock_status_t
nora_clock_switch_source(
    nora_clock_source_t source,
    uint32_t input_hz)
{
    nora_clock_status_t status;

    if (input_hz == 0u) {
        return NORA_CLOCK_ERR_INVALID_ARG;
    }

    switch (source) {
    case NORA_CLOCK_SOURCE_FRC:
    case NORA_CLOCK_SOURCE_BFRC:
    case NORA_CLOCK_SOURCE_PRIMARY:
    case NORA_CLOCK_SOURCE_LPRC:
        break;
    default:
        return NORA_CLOCK_ERR_NOT_SUPPORTED;
    }

    status = configure_clkgen(NORA_CLOCK_DSPIC33AK_CLKGEN_1, source, 1u);
    if (status != NORA_CLOCK_OK) {
        return status;
    }

    /* Only after the hardware switch completed: an aborted switch must not leave
     * a frequency recorded that nothing is running at. */
    s_fosc_hz = input_hz;
    return NORA_CLOCK_OK;
}

/* -------------------------------------------------------------------------- */
/* Authoritative current frequencies (portable face)                          */
/* -------------------------------------------------------------------------- */
uint32_t nora_clock_get_fosc_hz(void)
{
    return s_fosc_hz;
}

uint32_t nora_clock_get_fcy_hz(void)
{
    return s_fosc_hz / DSPIC33AK_CLOCK_FOSC_TO_FCY_DIV;
}

/* -------------------------------------------------------------------------- */
/* Configure CLKGEN from a logical clock request (dsPIC33A-only face)         */
/* -------------------------------------------------------------------------- */
nora_clock_status_t
nora_clock_dspic33ak_clkgen_configure(
    nora_clock_dspic33ak_clkgen_t clkgen,
    const nora_clock_dspic33ak_clkgen_config_t *config)
{
    nora_clock_status_t status;

    if (config == NULL || config->divide_by == 0u) {
        return NORA_CLOCK_ERR_INVALID_ARG;
    }

    status = configure_clkgen(clkgen, config->source, config->divide_by);
    if (status != NORA_CLOCK_OK) {
        return status;
    }

    /* CLKGEN1 IS the system clock on this family, so programming it moves Fosc.
     * Record the new value when the source frequency is derivable in-HAL; leave
     * it UNKNOWN (0) otherwise rather than reporting a stale frequency that the
     * CPU is demonstrably no longer running at. */
    if (clkgen == NORA_CLOCK_DSPIC33AK_CLKGEN_1) {
        const uint32_t src_hz = source_hz_if_known(config->source);

        s_fosc_hz = (src_hz == 0u) ? 0u : (src_hz / config->divide_by);
    }

    return NORA_CLOCK_OK;
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
/* Frequency of a logical source, if this HAL can know it                     */
/* -------------------------------------------------------------------------- */
/*
 * FRC is a device fact. The two PLLs are known only once this HAL programmed
 * them. Everything else -- BFRC/LPRC (nominal, and never a system clock here),
 * the VCO fractional taps, and above all REFI1/REFI2, which are input PINS fed by
 * whatever the board wired -- is genuinely unknowable from inside the HAL, and 0
 * says so. Do not "improve" this by substituting a plausible number: a wrong Fcy
 * silently mis-programs every baud rate and bit clock derived from it.
 */
static uint32_t source_hz_if_known(nora_clock_source_t source)
{
    switch (source) {
    case NORA_CLOCK_SOURCE_FRC:
        return NORA_CLOCK_FRC_HZ;
    case NORA_CLOCK_SOURCE_PLL1:
        return s_pll_output_hz[0];
    case NORA_CLOCK_SOURCE_PLL2:
        return s_pll_output_hz[1];
    default:
        return 0u;
    }
}

/* -------------------------------------------------------------------------- */
/* Solve PLL divider fields for an exact target frequency                     */
/* -------------------------------------------------------------------------- */
static nora_clock_status_t solve_pll(
    const nora_clock_pll_config_t *config,
    dspic33ak_clock_pll_solution_t *solution)
{
    uint16_t pre_div;
    uint16_t post_div2;
    uint16_t post_div1;

    if (config == NULL || solution == NULL ||
        config->input_hz == 0u || config->target_hz == 0u) {
        return NORA_CLOCK_ERR_INVALID_ARG;
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
                return NORA_CLOCK_OK;
            }
        }
    }

    return NORA_CLOCK_ERR_UNREPRESENTABLE;
}

/* -------------------------------------------------------------------------- */
/* Configure one PLL through the internal register layer                       */
/* -------------------------------------------------------------------------- */
static nora_clock_status_t configure_pll(
    nora_clock_pll_t pll,
    const nora_clock_pll_config_t *config,
    uint32_t *actual_hz)
{
    dspic33ak_clock_pll_solution_t solution;
    dspic33ak_clock_reg_pll_config_t reg_config;
    nora_clock_status_t status;
    uint16_t source;

    if (!nora_clock_device_encode_pll_source(config->source, &source)) {
        return NORA_CLOCK_ERR_INVALID_ARG;
    }

    status = solve_pll(config, &solution);
    if (status != NORA_CLOCK_OK) {
        return status;
    }

    reg_config.source = source;
    reg_config.feedback_div = solution.feedback_div;
    reg_config.pre_div = solution.pre_div;
    reg_config.post_div1 = solution.post_div1;
    reg_config.post_div2 = solution.post_div2;

    status = dspic33ak_clock_reg_pll_configure(pll, &reg_config);
    if (status != NORA_CLOCK_OK) {
        return status;
    }

    /* Remember what this PLL now runs at, so a later CLKGEN1 request can derive
     * Fosc from it instead of the caller having to restate the frequency. */
    s_pll_output_hz[(uint8_t)pll - 1u] = solution.output_hz;

    if (actual_hz != NULL) {
        *actual_hz = solution.output_hz;
    }

    return NORA_CLOCK_OK;
}

/* -------------------------------------------------------------------------- */
/* Configure one CLKGEN through the internal register layer                    */
/* -------------------------------------------------------------------------- */
static nora_clock_status_t configure_clkgen(
    nora_clock_dspic33ak_clkgen_t clkgen,
    nora_clock_source_t source,
    uint16_t divide_by)
{
    dspic33ak_clock_reg_clkgen_config_t reg_config;
    uint16_t encoded_source;

    if (!nora_clock_device_encode_clkgen_source(source, &encoded_source)) {
        return NORA_CLOCK_ERR_INVALID_ARG;
    }

    reg_config.source = encoded_source;
    reg_config.intdiv = clkgen_integer_divider_intdiv(divide_by);
    reg_config.fracdiv = clkgen_integer_divider_fracdiv(divide_by);

    return dspic33ak_clock_reg_clkgen_configure(clkgen, &reg_config);
}
