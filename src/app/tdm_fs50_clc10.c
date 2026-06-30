/*
 * tdm_fs50_clc10.c  --  EXPERIMENT: 50%-duty FS via CLC10 J-K flip-flop (see header)
 *
 * All register/PPS facts below are transcribed from the dsPIC33AK512MPS512 Family Data
 * Sheet (DS70005591C) and the device header p33AK512MPS512.h -- nothing is guessed:
 *
 *   - CLC: 10 instances, 8 inputs each, 4 data-selection MUXes feeding 4 gates, MODE<2:0>
 *     selects the logic function.                                  (DS section 28, Fig 28-1..28-3)
 *   - MODE<2:0> = 0b110 -> "J-K Flip-Flop with R".                 (DS Figure 28-3)
 *     Gate roles for THIS silicon (DS Fig 28-3, verified on HW): even gates = data inputs,
 *     odd gates = controls -> Gate1 = CLK, Gate2 = J, Gate4 = K, Gate3 = R (async reset).
 *   - CLC10SEL.DS1 = 0b110 -> "Virtual Pin 8 Output" (= RPV8).     (DS Table 28-2)
 *   - RPV8 virtual output is written via RP137R[6:0] (RPOR34).     (DS Table 12-20)
 *   - PPS output codes: _RPOUT_SS1 = 27, _RPOUT_CLC10OUT = 78.     (device header)
 *   - External FS pin RP70 (RE5) output reg = _RP70R (RPOR17).     (device header / board_pins.h)
 *
 * Toggle trick: a CLC gate with NO data selected outputs logic 0; setting that gate's
 * GxPOL inverts it to a constant 1. So J and K are forced to 1 with G2POL/G3POL (no input
 * consumed), and R is momentarily forced to 1 (G4POL) at init to reset the FF to a known
 * state, then released to 0. The flip-flop then toggles on every rising edge of Gate1
 * (the SPI FSYNC half-frame marker on RPV8).
 *
 * FS phase note: after the init reset the CLC output starts Low (or inverted by LCPOL).
 * With FRMPOL=active-high TDM framing, the first marker (start of frame, slot 0) drives
 * the first toggle, so the FS-Low half nominally spans slots 0..3 and the FS-High half
 * spans slots 4..7 (one half-frame phase offset from SS1 is expected and is corrected, if
 * needed, with APP_TDM_MASTER_FS50_CLC10_INVERT -> LCPOL).
 */

#include "tdm_fs50_clc10.h"

#if (HAL_STARTER_ENABLE_TDM_SMOKE_DEMO && APP_TDM_MASTER_FS50_BY_CLC10)

#include <xc.h>
#include <stdio.h>
#include <stdint.h>

#include "dspic33ak_pps.h"     // dspic33ak_pps_unlock()/lock() (public)
#include "board_pins.h"        // BOARD_TDM_SPI1_FS_RP (== 70, the external FS pin)

//===========================================================
// Device facts (transcribed -- see file header for sources)
//===========================================================
#define CLC10_MODE_JK_FF_WITH_R   (0b110u)  // CLC10CON.MODE : J-K flip-flop with reset (Fig 28-3)
#define CLC10_DS1_VIRTUAL_PIN_8   (0b110u)  // CLC10SEL.DS1  : Virtual Pin 8 Output = RPV8 (Table 28-2)

// RPV8 virtual output register (RP137R[6:0]) -- carries the SS1 FSYNC marker to CLC input.
// The shared PPS HAL intentionally excludes virtual outputs (RP129..RP144), so the
// experiment writes RP137R directly under the PPS unlock/lock bracket.
#define CLC10_FS_MARKER_VIRTUAL_RP_REG   _RP137R     // RPV8  (= CLC "Virtual Pin 8" input)

//===========================================================
// Public
//===========================================================
bool tdm_fs50_clc10_apply(void)
{
    //--- 1/2. PPS routing: SS1 -> RPV8 (internal marker), CLC10OUT -> external FS pin RP70.
    // Both are output-side PPS writes, so unlock once, write, lock once. RP70 was already
    // configured as a digital output (idle low) by board_spi1_tdm_smoke_pins_init(); here
    // we only change WHAT drives it (CLC10OUT instead of SS1). RPV8 is internal -> no
    // TRIS/ANSEL.
    dspic33ak_pps_unlock();
    CLC10_FS_MARKER_VIRTUAL_RP_REG = (uint8_t)_RPOUT_SS1;       // SS1 FSYNC marker -> RPV8
    _RP70R                         = (uint8_t)_RPOUT_CLC10OUT;  // external FS pin <- CLC10OUT
    dspic33ak_pps_lock();

    //--- 3. CLC10 as a J-K flip-flop toggled by the RPV8 marker.
    CLC10CON = 0u;            // ON=0 (disabled while configuring) + all bits to a known 0

    // Data-select MUX: Data1 = RPV8 ("Virtual Pin 8"). DS2..DS4 unused (their data is not
    // selected by any enabled gate-logic-select bit below).
    CLC10SELbits.DS1 = CLC10_DS1_VIRTUAL_PIN_8;

    // Gate-logic select: Gate1 = Data1 (non-inverted) == the FSYNC marker.
    // All other GxDyT/GxDyN stay 0 -> Gates 2/3/4 have no data -> raw output 0 (then the
    // GxPOL bits below turn the ones we need into constant 1s).
    CLC10GLS = 0u;
    CLC10GLSbits.G1D1T = 1u;  // Gate1 passes Data1 true

#if APP_TDM_MASTER_FS50_CLC10_BYPASS
    // DIAGNOSTIC PASSTHROUGH: CLC10 = 4-input AND (MODE=0b010, DS Figure 28-3) with
    // Gate1 = Data1 and Gates 2/3/4 forced to 1 (AND identity). Output = Data1 = the SS1
    // marker, straight through -- NO flip-flop, NO reset. Proves SS1 -> RPV8 -> CLC10 ->
    // CLC10OUT -> RP70 end to end. RP70 should show the marker (~98 kHz narrow pulses).
    CLC10CONbits.MODE  = 0b010u;  // 4-input AND
    CLC10CONbits.G1POL = 0u;      // Gate1 = Data1 (non-inverted)
    CLC10CONbits.G2POL = 1u;      // empty gate 0 -> 1 (AND identity)
    CLC10CONbits.G3POL = 1u;      // empty gate 0 -> 1
    CLC10CONbits.G4POL = 1u;      // empty gate 0 -> 1
    CLC10CONbits.LCPOL = (APP_TDM_MASTER_FS50_CLC10_INVERT ? 1u : 0u);
    CLC10CONbits.LCOE  = 1u;      // drive CLC10OUT
    CLC10CONbits.ON    = 1u;
#else
    // J-K flip-flop with R. Gate roles for MODE=0b110, read from DS Figure 28-3 (table
    // extraction): even gates carry the data inputs, odd gates the controls --
    //   Gate1 = CLK,  Gate2 = J,  Gate4 = K,  Gate3 = R (async reset).
    // (NOT Gate3=K / Gate4=R: that mismapping leaves R asserted via G3POL=1, holding the
    //  FF permanently reset -> a static output. Verified on HW.)
    CLC10CONbits.MODE  = CLC10_MODE_JK_FF_WITH_R;  // 0b110 : J-K flip-flop with R
    CLC10CONbits.G1POL = 0u;  // Gate1 = CLK : non-inverted (toggle on marker rising edge)
    CLC10CONbits.G2POL = 1u;  // Gate2 = J = 1  (empty gate 0 -> inverted to 1)
    CLC10CONbits.G4POL = 1u;  // Gate4 = K = 1  (empty gate 0 -> inverted to 1)
    CLC10CONbits.G3POL = 1u;  // Gate3 = R = 1  (assert reset to force a known initial state)

    // Output: LCOE drives CLC10OUT onto the (PPS-routed) pin; LCPOL flips FS phase.
    CLC10CONbits.LCPOL = (APP_TDM_MASTER_FS50_CLC10_INVERT ? 1u : 0u);
    CLC10CONbits.LCOE  = 1u;

    CLC10CONbits.ON    = 1u;  // enable: R is still asserted -> FF held in reset (known Q)
    CLC10CONbits.G3POL = 0u;  // release reset (R=0): J=K=1, FF now TOGGLES on each CLK edge
#endif

    return true;
}

void tdm_fs50_clc10_print_status(void)
{
    // Snapshot the SPI1 framed-mode bits that define the FS marker, plus the CLC routing.
    const unsigned mode32  = (unsigned)SPI1CON1bits.MODE32;
    const unsigned frmen   = (unsigned)SPI1CON1bits.FRMEN;
    const unsigned frmcnt  = (unsigned)SPI1CON1bits.FRMCNT;   // 010 = FS every 4 words (128 BCLK)
    const unsigned frmsypw = (unsigned)SPI1CON1bits.FRMSYPW;  // 0 = one-clock-wide marker
    const unsigned auden   = (unsigned)SPI1CON1bits.AUDEN;    // 0 = framed SPI (not Audio mode)
    const unsigned clc_on  = (unsigned)CLC10CONbits.ON;
    const unsigned clc_pol = (unsigned)CLC10CONbits.LCPOL;

    const unsigned clc_mode = (unsigned)CLC10CONbits.MODE;

    printf(" [FS50] TDM Master FS mode: CLC10 50%% experiment%s\n",
           (APP_TDM_MASTER_FS50_CLC10_BYPASS ? " (BYPASS: passthrough diag)" : ""));
    printf("        SPI1: framed master, MODE32=%u FRMEN=%u FRMCNT=%u(=>FS/4words=128BCLK) FRMSYPW=%u AUDEN=%u\n",
           mode32, frmen, frmcnt, frmsypw, auden);
#if APP_TDM_MASTER_FS50_CLC10_BYPASS
    printf("        CLC10: PASSTHROUGH (MODE=%u 4-in AND) out=SS1 marker via RPV8(RP137R), ON=%u LCPOL=%u\n",
           clc_mode, clc_on, clc_pol);
#else
    printf("        CLC10: J-K FF toggle (MODE=%u) clk=SS1 marker via RPV8(RP137R), ON=%u LCPOL=%u\n",
           clc_mode, clc_on, clc_pol);
#endif
    printf("        FS source = CLC10OUT -> RP%u (was SS1); BCLK/FS ~256, FS duty ~50%%\n",
           (unsigned)BOARD_TDM_SPI1_FS_RP);
}

#endif /* HAL_STARTER_ENABLE_TDM_SMOKE_DEMO && APP_TDM_MASTER_FS50_BY_CLC10 */
