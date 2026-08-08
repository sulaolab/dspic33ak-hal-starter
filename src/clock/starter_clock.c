#include "starter_clock.h"

#include "nora_clock.h"
#include "nora_clock_dspic33a.h"   /* CLKGEN: AK-specific, board bring-up only */

#define STARTER_CLOCK_FRC_HZ (8000000UL)

static bool configure_app_clkgen(nora_clock_dspic33a_clkgen_t clkgen);

bool starter_clock_init(void)
{
    uint32_t actual_hz = 0u;
    const nora_clock_pll_config_t pll1 = {
        .source = NORA_CLOCK_SOURCE_FRC,
        .input_hz = STARTER_CLOCK_FRC_HZ,
        .target_hz = STARTER_CLOCK_SYS_HZ,
    };

    if (nora_clock_pll_configure(
            NORA_CLOCK_PLL_1,
            &pll1,
            &actual_hz) != NORA_CLOCK_OK) {
        return false;
    }
    if (actual_hz != STARTER_CLOCK_SYS_HZ) {
        return false;
    }

    return configure_app_clkgen(NORA_CLOCK_DSPIC33A_CLKGEN_1) &&
           configure_app_clkgen(NORA_CLOCK_DSPIC33A_CLKGEN_5) &&
           configure_app_clkgen(NORA_CLOCK_DSPIC33A_CLKGEN_6) &&
           configure_app_clkgen(NORA_CLOCK_DSPIC33A_CLKGEN_8) &&
           configure_app_clkgen(NORA_CLOCK_DSPIC33A_CLKGEN_9);
}

bool starter_clock_can_init(void)
{
    const nora_clock_dspic33a_clkgen_config_t can_clk = {
        .source = NORA_CLOCK_SOURCE_PLL1,
        .divide_by = 10u,
    };

    return nora_clock_dspic33a_clkgen_configure(
               NORA_CLOCK_DSPIC33A_CLKGEN_10,
               &can_clk) == NORA_CLOCK_OK;
}

static bool configure_app_clkgen(nora_clock_dspic33a_clkgen_t clkgen)
{
    const nora_clock_dspic33a_clkgen_config_t app_clk = {
        .source = NORA_CLOCK_SOURCE_PLL1,
        .divide_by = 1u,
    };

    return nora_clock_dspic33a_clkgen_configure(clkgen, &app_clk) == NORA_CLOCK_OK;
}
