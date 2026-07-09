#include "dspic33ak_clock_reg.h"

#include <xc.h>

#define DSPIC33AK_CLOCK_POLL_LIMIT (1000000UL)

#define DSPIC33AK_CLOCK_WAIT_CLEAR(EXPR)                  \
    do {                                                  \
        uint32_t poll_count = DSPIC33AK_CLOCK_POLL_LIMIT; \
        while ((EXPR) != 0u) {                            \
            if (--poll_count == 0u) {                     \
                return DSPIC33AK_CLOCK_ERR_TIMEOUT;       \
            }                                             \
        }                                                 \
    } while (0)

#define DSPIC33AK_CLOCK_WAIT_SET(EXPR)                    \
    do {                                                  \
        uint32_t poll_count = DSPIC33AK_CLOCK_POLL_LIMIT; \
        while ((EXPR) == 0u) {                            \
            if (--poll_count == 0u) {                     \
                return DSPIC33AK_CLOCK_ERR_TIMEOUT;       \
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
        DSPIC33AK_CLOCK_WAIT_CLEAR((CONBITS).DIVSWEN);             \
        (CONBITS).OSWEN = 1;                                       \
        DSPIC33AK_CLOCK_WAIT_CLEAR((CONBITS).OSWEN);               \
        DSPIC33AK_CLOCK_WAIT_SET((CONBITS).CLKRDY);                \
    } while (0)

/* --------------------------------------------------------------------------
 * Local helper prototypes
 * -------------------------------------------------------------------------- */

static dspic33ak_clock_status_t configure_pll1(
    const dspic33ak_clock_reg_pll_config_t *config);
static dspic33ak_clock_status_t configure_pll2(
    const dspic33ak_clock_reg_pll_config_t *config);

/* -------------------------------------------------------------------------- */
/* Configure PLL register block                                               */
/* -------------------------------------------------------------------------- */
dspic33ak_clock_status_t dspic33ak_clock_reg_pll_configure(
    dspic33ak_clock_pll_t pll,
    const dspic33ak_clock_reg_pll_config_t *config)
{
    if (config == 0) {
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }

    switch (pll) {
    case DSPIC33AK_CLOCK_PLL_1:
        return configure_pll1(config);
    case DSPIC33AK_CLOCK_PLL_2:
        return configure_pll2(config);
    default:
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }
}

/* -------------------------------------------------------------------------- */
/* Configure CLKGEN register block                                            */
/* -------------------------------------------------------------------------- */
dspic33ak_clock_status_t dspic33ak_clock_reg_clkgen_configure(
    dspic33ak_clock_clkgen_t clkgen,
    const dspic33ak_clock_reg_clkgen_config_t *config)
{
    if (config == 0) {
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }

    switch (clkgen) {
    case DSPIC33AK_CLOCK_CLKGEN_1:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK1CONbits, CLK1DIVbits, config);
        return DSPIC33AK_CLOCK_OK;
    case DSPIC33AK_CLOCK_CLKGEN_5:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK5CONbits, CLK5DIVbits, config);
        return DSPIC33AK_CLOCK_OK;
    case DSPIC33AK_CLOCK_CLKGEN_6:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK6CONbits, CLK6DIVbits, config);
        return DSPIC33AK_CLOCK_OK;
    case DSPIC33AK_CLOCK_CLKGEN_8:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK8CONbits, CLK8DIVbits, config);
        return DSPIC33AK_CLOCK_OK;
    case DSPIC33AK_CLOCK_CLKGEN_9:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK9CONbits, CLK9DIVbits, config);
        return DSPIC33AK_CLOCK_OK;
    case DSPIC33AK_CLOCK_CLKGEN_10:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK10CONbits, CLK10DIVbits, config);
        return DSPIC33AK_CLOCK_OK;
    case DSPIC33AK_CLOCK_CLKGEN_12:
        DSPIC33AK_CLOCK_CONFIGURE_CLKGEN(CLK12CONbits, CLK12DIVbits, config);
        return DSPIC33AK_CLOCK_OK;
    default:
        return DSPIC33AK_CLOCK_ERR_INVALID_ARG;
    }
}

/* -------------------------------------------------------------------------- */
/* Configure PLL1                                                             */
/* -------------------------------------------------------------------------- */
static dspic33ak_clock_status_t configure_pll1(
    const dspic33ak_clock_reg_pll_config_t *config)
{
    PLL1CONbits.ON = 1;
    PLL1CONbits.OE = 1;

    PLL1DIVbits.PLLFBDIV = config->feedback_div;
    PLL1DIVbits.PLLPRE = config->pre_div;
    PLL1DIVbits.POSTDIV1 = config->post_div1;
    PLL1DIVbits.POSTDIV2 = config->post_div2;

    PLL1CONbits.PLLSWEN = 1;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL1CONbits.PLLSWEN);

    PLL1CONbits.FOUTSWEN = 1;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL1CONbits.FOUTSWEN);

    PLL1CONbits.NOSC = config->source;
    PLL1CONbits.OSWEN = 1;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL1CONbits.OSWEN);
    DSPIC33AK_CLOCK_WAIT_SET(OSCCTRLbits.PLL1RDY);

    return DSPIC33AK_CLOCK_OK;
}

/* -------------------------------------------------------------------------- */
/* Configure PLL2                                                             */
/* -------------------------------------------------------------------------- */
static dspic33ak_clock_status_t configure_pll2(
    const dspic33ak_clock_reg_pll_config_t *config)
{
    PLL2CONbits.ON = 1;
    PLL2CONbits.OE = 1;

    PLL2DIVbits.PLLFBDIV = config->feedback_div;
    PLL2DIVbits.PLLPRE = config->pre_div;
    PLL2DIVbits.POSTDIV1 = config->post_div1;
    PLL2DIVbits.POSTDIV2 = config->post_div2;

    PLL2CONbits.PLLSWEN = 1;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL2CONbits.PLLSWEN);

    PLL2CONbits.FOUTSWEN = 1;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL2CONbits.FOUTSWEN);

    PLL2CONbits.NOSC = config->source;
    PLL2CONbits.OSWEN = 1;
    DSPIC33AK_CLOCK_WAIT_CLEAR(PLL2CONbits.OSWEN);
    DSPIC33AK_CLOCK_WAIT_SET(OSCCTRLbits.PLL2RDY);

    return DSPIC33AK_CLOCK_OK;
}
