#include "nora_clock_dspic33ak_reg.h"

#include <xc.h>

#define DSPIC33AK_CLOCK_POLL_LIMIT (1000000UL)

#define DSPIC33AK_CLOCK_WAIT_CLEAR(EXPR, TIMEOUT_STATUS)  \
    do {                                                  \
        uint32_t poll_count = DSPIC33AK_CLOCK_POLL_LIMIT; \
        while ((EXPR) != 0u) {                            \
            if (--poll_count == 0u) {                     \
                return (TIMEOUT_STATUS);                  \
            }                                             \
        }                                                 \
    } while (0)

#define DSPIC33AK_CLOCK_WAIT_SET(EXPR, TIMEOUT_STATUS)    \
    do {                                                  \
        uint32_t poll_count = DSPIC33AK_CLOCK_POLL_LIMIT; \
        while ((EXPR) == 0u) {                            \
            if (--poll_count == 0u) {                     \
                return (TIMEOUT_STATUS);                  \
            }                                             \
        }                                                 \
    } while (0)

#define DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CONBITS, DIVBITS, CONFIG) \
    do {                                                           \
        (CONBITS).ON = 0;                                          \
        (CONBITS).NOSC = (CONFIG)->source;                         \
        (CONBITS).ON = 1;                                          \
        (CONBITS).OE = 1;                                          \
        (DIVBITS).INTDIV = (CONFIG)->intdiv;                       \
        (DIVBITS).FRACDIV = (CONFIG)->fracdiv;                     \
        (CONBITS).DIVSWEN = 1;                                     \
        DSPIC33AK_CLOCK_WAIT_CLEAR((CONBITS).DIVSWEN,              \
            NORA_CLOCK_ERR_DIV_SWITCH_TIMEOUT);               \
        (CONBITS).OSWEN = 1;                                       \
        DSPIC33AK_CLOCK_WAIT_CLEAR((CONBITS).OSWEN,                \
            NORA_CLOCK_ERR_SOURCE_SWITCH_TIMEOUT);            \
        DSPIC33AK_CLOCK_WAIT_SET((CONBITS).CLKRDY,                 \
            NORA_CLOCK_ERR_READY_TIMEOUT);                    \
    } while (0)

/* --------------------------------------------------------------------------
 * Local helper prototypes
 * -------------------------------------------------------------------------- */

static nora_clock_status_t configure_pll1(
    const dspic33ak_clock_reg_pll_config_t *config);
static nora_clock_status_t configure_pll2(
    const dspic33ak_clock_reg_pll_config_t *config);

/* -------------------------------------------------------------------------- */
/* Configure PLL register block                                               */
/* -------------------------------------------------------------------------- */
nora_clock_status_t dspic33ak_clock_reg_pll_configure(
    nora_clock_pll_t pll,
    const dspic33ak_clock_reg_pll_config_t *config)
{
    if (config == 0) {
        return NORA_CLOCK_ERR_INVALID_ARG;
    }

    switch (pll) {
    case NORA_CLOCK_PLL_1:
        return configure_pll1(config);
    case NORA_CLOCK_PLL_2:
        return configure_pll2(config);
    default:
        return NORA_CLOCK_ERR_INVALID_ARG;
    }
}

/* -------------------------------------------------------------------------- */
/* Configure CLKGEN register block                                            */
/* -------------------------------------------------------------------------- */
nora_clock_status_t dspic33ak_clock_reg_clkgen_configure(
    nora_clock_dspic33ak_clkgen_t clkgen,
    const dspic33ak_clock_reg_clkgen_config_t *config)
{
    if (config == 0) {
        return NORA_CLOCK_ERR_INVALID_ARG;
    }

    switch (clkgen) {
    case NORA_CLOCK_DSPIC33AK_CLKGEN_1:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK1CONbits, CLK1DIVbits, config);
        return NORA_CLOCK_OK;
    case NORA_CLOCK_DSPIC33AK_CLKGEN_5:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK5CONbits, CLK5DIVbits, config);
        return NORA_CLOCK_OK;
    case NORA_CLOCK_DSPIC33AK_CLKGEN_6:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK6CONbits, CLK6DIVbits, config);
        return NORA_CLOCK_OK;
    case NORA_CLOCK_DSPIC33AK_CLKGEN_8:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK8CONbits, CLK8DIVbits, config);
        return NORA_CLOCK_OK;
    case NORA_CLOCK_DSPIC33AK_CLKGEN_9:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK9CONbits, CLK9DIVbits, config);
        return NORA_CLOCK_OK;
    case NORA_CLOCK_DSPIC33AK_CLKGEN_10:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK10CONbits, CLK10DIVbits, config);
        return NORA_CLOCK_OK;
    case NORA_CLOCK_DSPIC33AK_CLKGEN_12:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK12CONbits, CLK12DIVbits, config);
        return NORA_CLOCK_OK;
    case NORA_CLOCK_DSPIC33AK_CLKGEN_13:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK13CONbits, CLK13DIVbits, config);
        return NORA_CLOCK_OK;
    default:
        return NORA_CLOCK_ERR_INVALID_ARG;
    }
}

/* -------------------------------------------------------------------------- */
/* Configure PLL1                                                             */
/* -------------------------------------------------------------------------- */
static nora_clock_status_t configure_pll1(
    const dspic33ak_clock_reg_pll_config_t *config)
{
    uint32_t start_poll;

    PLL1CONbits.ON = 0;

    /* POSTDIV1/2 must not be changed while the PLL is operating. Stage and
     * commit all normal PLL dividers while ON is clear, then enable the analog
     * generator only after the inherited ASRC configuration is replaced. */
    PLL1DIVbits.PLLFBDIV = config->feedback_div;
    PLL1DIVbits.PLLPRE = config->pre_div;
    PLL1DIVbits.POSTDIV1 = config->post_div1;
    PLL1DIVbits.POSTDIV2 = config->post_div2;

    /* ON starts an internal DIVSWEN asynchronously on this silicon. Do not
     * issue PLLSWEN behind that transfer: first allow the ON-started divider
     * cycle to become visible and finish. A cold path may not assert DIVSWEN,
     * so absence during the short observation window is also valid. */
    PLL1CONbits.ON = 1u;
    PLL1CONbits.OE = 1u;
    start_poll = 10000u;
    while ((PLL1CONbits.DIVSWEN == 0u) && (--start_poll != 0u)) {
    }
    if (PLL1CONbits.DIVSWEN != 0u) {
        DSPIC33AK_CLOCK_WAIT_CLEAR(PLL1CONbits.DIVSWEN,
            NORA_CLOCK_ERR_DIV_SWITCH_TIMEOUT);
    }

    PLL1CONbits.PLLSWEN = 1u;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL1CONbits.PLLSWEN,
        NORA_CLOCK_ERR_PLL_SWITCH_TIMEOUT);

    PLL1CONbits.FOUTSWEN = 1u;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL1CONbits.FOUTSWEN,
        NORA_CLOCK_ERR_FOUT_SWITCH_TIMEOUT);

    PLL1CONbits.NOSC = config->source;
    PLL1CONbits.OSWEN = 1u;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL1CONbits.OSWEN,
        NORA_CLOCK_ERR_SOURCE_SWITCH_TIMEOUT);
    DSPIC33AK_CLOCK_WAIT_SET(OSCCTRLbits.PLL1RDY,
        NORA_CLOCK_ERR_READY_TIMEOUT);

    return NORA_CLOCK_OK;
}

/* -------------------------------------------------------------------------- */
/* Configure PLL2                                                             */
/* -------------------------------------------------------------------------- */
static nora_clock_status_t configure_pll2(
    const dspic33ak_clock_reg_pll_config_t *config)
{
    PLL2CONbits.ON = 1;
    PLL2CONbits.OE = 1;

    PLL2DIVbits.PLLFBDIV = config->feedback_div;
    PLL2DIVbits.PLLPRE = config->pre_div;
    PLL2DIVbits.POSTDIV1 = config->post_div1;
    PLL2DIVbits.POSTDIV2 = config->post_div2;

    PLL2CONbits.PLLSWEN = 1;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL2CONbits.PLLSWEN,
        NORA_CLOCK_ERR_PLL_SWITCH_TIMEOUT);

    PLL2CONbits.FOUTSWEN = 1;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL2CONbits.FOUTSWEN,
        NORA_CLOCK_ERR_FOUT_SWITCH_TIMEOUT);

    PLL2CONbits.NOSC = config->source;
    PLL2CONbits.OSWEN = 1;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL2CONbits.OSWEN,
        NORA_CLOCK_ERR_SOURCE_SWITCH_TIMEOUT);
    DSPIC33AK_CLOCK_WAIT_SET(OSCCTRLbits.PLL2RDY,
        NORA_CLOCK_ERR_READY_TIMEOUT);

    return NORA_CLOCK_OK;
}
