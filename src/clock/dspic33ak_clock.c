/*
 * dspic33ak_clock.c
 * -----------------
 * See dspic33ak_clock.h for the clock tree diagram and the public contract.
 *
 * This is a slimmed, dependency-free clock bring-up: PLL1 from the 8 MHz FRC up
 * to 200 MHz, then CLKGEN1/5/6/8/9 routed to PLL1 (divide-by-1). PLL2 and the
 * audio clock paths are intentionally omitted.
 */

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#include "dspic33ak_clock.h"

/* FRC nominal frequency (PLL input source for this starter). */
#define CLOCK_FRC_HZ   (8000000UL)

/*
 * Route one CLKGENn block to a source at divide-by-1, then wait for the divider
 * and oscillator switches to complete. Each block waits on its OWN switch bits
 * (xCONbits.DIVSWEN / xCONbits.OSWEN), so there is no cross-block dependency.
 */
#define CLOCK_ROUTE_CLKGEN(n, src)                       \
    do {                                                 \
        CLK##n##CONbits.ON      = 0;                     \
        CLK##n##CONbits.NOSC    = (uint8_t)(src);        \
        CLK##n##CONbits.ON      = 1;                     \
        CLK##n##CONbits.OE      = 1;                     \
        CLK##n##DIVbits.INTDIV  = 0;   /* divide by 1 */ \
        CLK##n##CONbits.DIVSWEN = 1;                     \
        while (CLK##n##CONbits.DIVSWEN) { }              \
        CLK##n##CONbits.OSWEN   = 1;                     \
        while (CLK##n##CONbits.OSWEN) { }                \
    } while (0)

/*
 * Search PLL1 dividers (PRE / FBDIV / POST1 / POST2) for a setting that turns
 * in_hz into req_hz while respecting the device PLL constraints:
 *   FPLLI = Fin / PRE                 in [5 MHz, 64 MHz]
 *   FBDIV                             in [16, 200]
 *   FVCO  = FPLLI * FBDIV             in [500 MHz, 2000 MHz]
 *   Fout  = FVCO / (POST1 * POST2)    <= 2000 MHz, with POST1 >= POST2
 * Returns false if no solution is found.
 */
static bool clock_solve_pll(uint32_t  in_hz,
                            uint32_t  req_hz,
                            uint32_t *fbdiv,
                            uint16_t *post1,
                            uint16_t *post2,
                            uint16_t *pre)
{
    for (uint16_t pre_i = 1u; pre_i < 9u; pre_i++)
    {
        uint32_t fplli = in_hz / pre_i;
        if (fplli < 5000000UL || fplli > 64000000UL) {
            continue;
        }

        for (uint16_t p2 = 1u; p2 < 8u; p2++)
        {
            for (uint16_t p1 = 1u; p1 < 8u; p1++)
            {
                if (p1 < p2) {
                    continue;   /* require POST1 >= POST2 */
                }

                uint32_t fbd  = (req_hz * pre_i) * ((uint32_t)p1 * p2) / in_hz;
                if (fbd < 16u || fbd > 200u) {
                    continue;
                }

                uint32_t fvco = fplli * fbd;
                if (fvco < 500000000UL || fvco > 2000000000UL) {
                    continue;
                }

                *fbdiv = fbd;
                *post1 = p1;
                *post2 = p2;
                *pre   = pre_i;
                return true;
            }
        }
    }
    return false;
}

static bool clock_config_pll1(dspic33ak_clock_src_t src, uint32_t in_hz, uint32_t req_hz)
{
    uint32_t fbdiv;
    uint16_t post1, post2, pre;

    if (!clock_solve_pll(in_hz, req_hz, &fbdiv, &post1, &post2, &pre)) {
        return false;
    }

    PLL1CONbits.NOSC = (uint8_t)src;
    PLL1CONbits.ON   = 1;
    PLL1CONbits.OE   = 1;

    PLL1CONbits.OSWEN = 1;
    while (PLL1CONbits.OSWEN) { }

    PLL1DIVbits.PLLFBDIV = fbdiv;
    PLL1DIVbits.PLLPRE   = pre;
    PLL1DIVbits.POSTDIV1 = post1;
    PLL1DIVbits.POSTDIV2 = post2;

    /* Apply input/feedback divider update, then the output divider update. */
    PLL1CONbits.PLLSWEN = 1;
    while (PLL1CONbits.PLLSWEN) { }

    PLL1CONbits.FOUTSWEN = 1;
    while (PLL1CONbits.FOUTSWEN) { }

    return true;
}

bool dspic33ak_clock_init(void)
{
    /* FRC (8 MHz) -> PLL1 -> 200 MHz */
    if (!clock_config_pll1(DSPIC33AK_CLKSRC_FRC, CLOCK_FRC_HZ, DSPIC33AK_CLOCK_SYS_HZ)) {
        return false;
    }

    /* Route each peripheral-domain CLKGEN to PLL1 (divide-by-1). */
    CLOCK_ROUTE_CLKGEN(1, DSPIC33AK_CLKSRC_PLL1);   /* System + peripheral (I2C) */
    CLOCK_ROUTE_CLKGEN(5, DSPIC33AK_CLKSRC_PLL1);   /* PWM  (RGB LED)            */
    CLOCK_ROUTE_CLKGEN(6, DSPIC33AK_CLKSRC_PLL1);   /* ADC  (potentiometer)      */
    CLOCK_ROUTE_CLKGEN(8, DSPIC33AK_CLKSRC_PLL1);   /* UART (printf)             */
    CLOCK_ROUTE_CLKGEN(9, DSPIC33AK_CLKSRC_PLL1);   /* SPI  (SST26 flash)        */

    return true;
}
