#ifndef DSPIC33AK_CLOCK_DEVICE_H
#define DSPIC33AK_CLOCK_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

#include "dspic33ak_clock.h"

/*
 * Internal dsPIC33AK device adaptation.
 *
 * The public Clock HAL uses logical source names such as FRC, PLL1, and
 * PRIMARY.  This layer owns the family/device-specific NOSC encodings used by
 * PLLxCON and CLKxCON, keeping raw numeric values out of the generic core and
 * out of public headers.  If a future AK device changes encodings or source
 * availability, that mapping belongs here.
 */

#ifdef __cplusplus
extern "C" {
#endif

bool dspic33ak_clock_device_encode_pll_source(
    dspic33ak_clock_source_t source,
    uint16_t *value);

bool dspic33ak_clock_device_encode_clkgen_source(
    dspic33ak_clock_source_t source,
    uint16_t *value);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_CLOCK_DEVICE_H */
