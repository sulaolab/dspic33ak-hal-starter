/*
 * tdm_fs50_clc10.h  --  EXPERIMENT: 50%-duty TDM/LRCK frame sync via CLC10
 * ------------------------------------------------------------------------
 * Branch: exp/tdm-master-clc10-fs50
 *
 * Goal (waveform observation only, not a final spec):
 *   The SPI1 TDM8 master normally outputs a short FRMSYNC pulse as the external FS.
 *   This experiment instead makes SPI1 emit a half-frame marker (every 4 words = 128
 *   BCLK) and feeds it INTERNALLY (virtual pin RPV8, no external jumper) into CLC10,
 *   configured as a J-K flip-flop (J=K=1) so it TOGGLES on every marker. The CLC10
 *   output is a ~50%-duty FS with a 256-BCLK period (one toggle per half-frame), routed
 *   to the external FS pin RP70 in place of SS1.
 *
 *   SPI internal FSYNC marker:  ^.......^.......^.......   (every 128 BCLK)
 *   CLC10 output FS:            ____--------____--------   (256 BCLK period, ~50% duty)
 *
 * This module owns ONLY the FS-generation routing/CLC config. BCLK (SCK1->RP75),
 * DATA out (SDO1->RP101), DATA in (SDI1<-RP106) and the DMA/audio path are untouched.
 *
 * Enabled by APP_TDM_MASTER_FS50_BY_CLC10 (app_config.h). When that macro is 0 this whole
 * translation unit compiles to nothing.
 */
#ifndef TDM_FS50_CLC10_H
#define TDM_FS50_CLC10_H

#include <stdbool.h>
#include "app_config.h"

#if (HAL_STARTER_ENABLE_TDM_SMOKE_DEMO && APP_TDM_MASTER_FS50_BY_CLC10)

/*
 * Set up the CLC10 50%-FS chain:
 *   1. Route SS1 (SPI1 FSYNC marker) to virtual pin RPV8 (internal, no pad).
 *   2. Re-point the external FS pin RP70 from SS1 to CLC10OUT.
 *   3. Configure CLC10 as a J-K flip-flop toggled by RPV8, with a known initial state.
 *
 * Call this AFTER the SPI1 TDM pins are routed (board_spi1_tdm_smoke_pins_init) and the
 * port is opened, but BEFORE the SPI module is enabled, so CLC10 is armed before the
 * first FSYNC marker arrives. SS1 still generates the marker once the SPI turns on.
 *
 * Returns true on success. There is no peripheral "ready" gate to fail here; it returns
 * false only if it is called with the experiment disabled (defensive).
 */
bool tdm_fs50_clc10_apply(void);

/* One-line startup banner describing the active FS-generation settings (main-loop ctx). */
void tdm_fs50_clc10_print_status(void);

#endif /* HAL_STARTER_ENABLE_TDM_SMOKE_DEMO && APP_TDM_MASTER_FS50_BY_CLC10 */

#endif /* TDM_FS50_CLC10_H */
