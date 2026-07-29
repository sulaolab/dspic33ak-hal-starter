#ifndef DSPIC33AK_CLOCK_DIAG_H
#define DSPIC33AK_CLOCK_DIAG_H

#include <stdint.h>

/*
 * dspic33ak_clock_diag.h
 * -----------------------
 * Read-only PLL1/PLL2/OSCCTRL/RCON register snapshot for the PLL2
 * forced-stop/restart experiment (exp/pll2-soft-reset-restart).
 *
 * This is intentionally a SEPARATE file from dspic33ak_clock.* /
 * dspic33ak_clock_reg.*: those six files are pinned byte-identical to the
 * upstream Clock HAL (see docs/clock_hal_integration.md), and this experiment
 * must not disturb that. Raw SFR access for this diagnostic stays confined to
 * dspic33ak_clock_diag.c, i.e. still inside src/hal_clock/, just not inside the
 * pinned files.
 *
 * Capture never writes to a register and never clears RCON status bits -- the
 * caller decides if/when RCON is cleared.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Raw SFR words, kept for a full dump on failure (see experiment doc). */
    uint32_t rcon;
    uint32_t oscctrl;
    uint32_t pll1con;
    uint32_t pll1div;
    uint32_t pll2con;
    uint32_t pll2div;
    uint32_t vco2div;
    uint32_t clkfail;
    uint32_t scsfail;
    uint32_t clkdiag;

    /* RCON decode. */
    uint8_t rcon_por;
    uint8_t rcon_bor;
    uint8_t rcon_extr;
    uint8_t rcon_swr;
    uint8_t rcon_wdto;
    uint8_t rcon_cm;

    /* OSCCTRL decode. */
    uint8_t frcen;
    uint8_t frcrdy;
    uint8_t pll1en;
    uint8_t pll1rdy;
    uint8_t pll2en;
    uint8_t pll2rdy;
    uint8_t clklock;

    /* PLL2CON decode. */
    uint8_t pll2_on;
    uint8_t pll2_nosc;
    uint8_t pll2_cosc;
    uint8_t pll2_clkrdy;
    uint8_t pll2_pllswen;
    uint8_t pll2_foutswen;
    uint8_t pll2_oswen;
    uint8_t pll2_divswen;

    /* PLL2DIV decode. */
    uint16_t pll2_pllpre;
    uint16_t pll2_pllfbdiv;
    uint16_t pll2_postdiv1;
    uint16_t pll2_postdiv2;

    /* CLKFAIL / SCSFAIL / CLKDIAG decode, PLL2-relevant bits only. */
    uint8_t pll2fail;
    uint8_t pll2scs;
    uint8_t stoppll2;
} dspic33ak_clock_diag_snapshot_t;

/* Fill *out from the live SFRs. Pure read: no register is written, no status
 * bit is cleared. Safe to call at any point, including before clock/timer/UART
 * bring-up. */
void dspic33ak_clock_diag_capture(dspic33ak_clock_diag_snapshot_t *out);

/* Clear RCON's sticky status bits (POR/BOR/IDLE/SLEEP/WDTO/SWR/EXTR/CM) after
 * they have been captured. RCON bits are set-only in hardware and otherwise
 * accumulate across resets within one power session, so a caller that latches
 * the reset cause once per boot (e.g. to decide whether a persistent-RAM
 * campaign may continue) must clear them here or a later software reset would
 * still read an earlier POR/BOR/etc. bit as still set. This is the only write
 * this diagnostics module performs. */
void dspic33ak_clock_diag_clear_rcon(void);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_CLOCK_DIAG_H */
