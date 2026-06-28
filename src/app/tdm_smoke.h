#ifndef TDM_SMOKE_H
#define TDM_SMOKE_H

/*
 * tdm_smoke.h
 * -----------
 * Codec-less SPI1 framed-mode (TDM8) smoke demo for the dspic33ak-hal-starter.
 *
 * Brings up SPI1 as a self-clocked TDM8 master on MikroBUS-A and drives all 8 slots with
 * the same ~800 Hz-class sine, so a scope on DataOut shows a TDM8 frame. If DataOut is
 * jumpered to DataIn, the RX level reads near 0 dB relative to the TX sine; with no jumper
 * it floors near -140 dB rel.
 *
 * Responsibilities live here (module): sine table, the block callback (TX fill + a light
 * RX accumulator -- NO heavy math in the callback), the port-hook binding to the board pin
 * routine, lifecycle (set_port -> configure -> open -> start), and the 5 s status line.
 * Board pin/PPS routing stays in board.c (reached via the HAL port hook).
 */

#include <stdbool.h>

/*
 * Bring up + start the demo: register the board port + block callback, configure the SPI1
 * instance as a TDM8 master, init the DMA HAL, open the port, and start the stream. After
 * this returns true the stream runs on DMA/ISR with no further main-loop work needed.
 * Returns false on any step failure (the caller should keep running the rest of the
 * starter); tdm_smoke_last_error_str() then describes the failing HAL reason.
 */
bool tdm_smoke_init(void);

/*
 * Print ONE TDM status line. Call from the main loop (e.g. every ~5 s) -- the heavy math
 * (RMS -> relative dB, formatting) happens here, NOT in the block callback. No-op if the
 * demo did not start.
 */
void tdm_smoke_status_print(void);

/* Human-readable reason for the most recent tdm_smoke_init() failure (or "none"). */
const char *tdm_smoke_last_error_str(void);

#endif /* TDM_SMOKE_H */
