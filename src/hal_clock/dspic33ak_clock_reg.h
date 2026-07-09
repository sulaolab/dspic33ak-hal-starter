#ifndef DSPIC33AK_CLOCK_REG_H
#define DSPIC33AK_CLOCK_REG_H

#include <stdint.h>

#include "dspic33ak_clock.h"

/*
 * Internal dsPIC33AK register programming layer.
 *
 * This file is intentionally below the public Clock HAL API.  It owns the raw
 * PLLx / CLKx SFR access, XC-DSC bitfield names, DFP-provided register layout,
 * switch sequencing, ready polling, and timeout reporting.  Callers provide
 * already-encoded register fields; board policy and logical source names do not
 * belong in this layer.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t source;
    uint32_t feedback_div;
    uint16_t pre_div;
    uint16_t post_div1;
    uint16_t post_div2;
} dspic33ak_clock_reg_pll_config_t;

typedef struct {
    uint16_t source;
    uint16_t intdiv;
    uint16_t fracdiv;
} dspic33ak_clock_reg_clkgen_config_t;

dspic33ak_clock_status_t dspic33ak_clock_reg_pll_configure(
    dspic33ak_clock_pll_t pll,
    const dspic33ak_clock_reg_pll_config_t *config);

dspic33ak_clock_status_t dspic33ak_clock_reg_clkgen_configure(
    dspic33ak_clock_clkgen_t clkgen,
    const dspic33ak_clock_reg_clkgen_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_CLOCK_REG_H */
