#include "starter_clock.h"

#include "dspic33ak_clock.h"

#define STARTER_CLOCK_FRC_HZ (8000000UL)

static bool configure_app_clkgen(dspic33ak_clock_clkgen_t clkgen);

bool starter_clock_init(void)
{
    uint32_t actual_hz = 0u;
    const dspic33ak_clock_pll_config_t pll1 = {
        .source = DSPIC33AK_CLOCK_SOURCE_FRC,
        .input_hz = STARTER_CLOCK_FRC_HZ,
        .target_hz = STARTER_CLOCK_SYS_HZ,
    };

    if (dspic33ak_clock_pll_configure(
            DSPIC33AK_CLOCK_PLL_1,
            &pll1,
            &actual_hz) != DSPIC33AK_CLOCK_OK) {
        return false;
    }
    if (actual_hz != STARTER_CLOCK_SYS_HZ) {
        return false;
    }

    return configure_app_clkgen(DSPIC33AK_CLOCK_CLKGEN_1) &&
           configure_app_clkgen(DSPIC33AK_CLOCK_CLKGEN_5) &&
           configure_app_clkgen(DSPIC33AK_CLOCK_CLKGEN_6) &&
           configure_app_clkgen(DSPIC33AK_CLOCK_CLKGEN_8) &&
           configure_app_clkgen(DSPIC33AK_CLOCK_CLKGEN_9);
}

bool starter_clock_can_init(void)
{
    const dspic33ak_clock_clkgen_config_t can_clk = {
        .source = DSPIC33AK_CLOCK_SOURCE_PLL1,
        .divide_by = 10u,
    };

    return dspic33ak_clock_clkgen_configure(
               DSPIC33AK_CLOCK_CLKGEN_10,
               &can_clk) == DSPIC33AK_CLOCK_OK;
}

static bool configure_app_clkgen(dspic33ak_clock_clkgen_t clkgen)
{
    const dspic33ak_clock_clkgen_config_t app_clk = {
        .source = DSPIC33AK_CLOCK_SOURCE_PLL1,
        .divide_by = 1u,
    };

    return dspic33ak_clock_clkgen_configure(clkgen, &app_clk) == DSPIC33AK_CLOCK_OK;
}
