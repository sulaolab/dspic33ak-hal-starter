#include "dspic33ak_clock_diag.h"

#include <xc.h>

void dspic33ak_clock_diag_capture(dspic33ak_clock_diag_snapshot_t *out)
{
    if (out == 0) {
        return;
    }

    out->rcon    = RCON;
    out->oscctrl = OSCCTRL;
    out->pll1con = PLL1CON;
    out->pll1div = PLL1DIV;
    out->pll2con = PLL2CON;
    out->pll2div = PLL2DIV;
    out->vco2div = VCO2DIV;
    out->clkfail = CLKFAIL;
    out->scsfail = SCSFAIL;
    out->clkdiag = CLKDIAG;

    out->rcon_por  = (uint8_t)RCONbits.POR;
    out->rcon_bor  = (uint8_t)RCONbits.BOR;
    out->rcon_extr = (uint8_t)RCONbits.EXTR;
    out->rcon_swr  = (uint8_t)RCONbits.SWR;
    out->rcon_wdto = (uint8_t)RCONbits.WDTO;
    out->rcon_cm   = (uint8_t)RCONbits.CM;

    out->frcen   = (uint8_t)OSCCTRLbits.FRCEN;
    out->frcrdy  = (uint8_t)OSCCTRLbits.FRCRDY;
    out->pll1en  = (uint8_t)OSCCTRLbits.PLL1EN;
    out->pll1rdy = (uint8_t)OSCCTRLbits.PLL1RDY;
    out->pll2en  = (uint8_t)OSCCTRLbits.PLL2EN;
    out->pll2rdy = (uint8_t)OSCCTRLbits.PLL2RDY;
    out->clklock = (uint8_t)OSCCTRLbits.CLKLOCK;

    out->pll2_on       = (uint8_t)PLL2CONbits.ON;
    out->pll2_nosc     = (uint8_t)PLL2CONbits.NOSC;
    out->pll2_cosc     = (uint8_t)PLL2CONbits.COSC;
    out->pll2_clkrdy   = (uint8_t)PLL2CONbits.CLKRDY;
    out->pll2_pllswen  = (uint8_t)PLL2CONbits.PLLSWEN;
    out->pll2_foutswen = (uint8_t)PLL2CONbits.FOUTSWEN;
    out->pll2_oswen    = (uint8_t)PLL2CONbits.OSWEN;
    out->pll2_divswen  = (uint8_t)PLL2CONbits.DIVSWEN;

    out->pll2_pllpre   = (uint16_t)PLL2DIVbits.PLLPRE;
    out->pll2_pllfbdiv = (uint16_t)PLL2DIVbits.PLLFBDIV;
    out->pll2_postdiv1 = (uint16_t)PLL2DIVbits.POSTDIV1;
    out->pll2_postdiv2 = (uint16_t)PLL2DIVbits.POSTDIV2;

    out->pll2fail = (uint8_t)CLKFAILbits.PLL2FAIL;
    out->pll2scs  = (uint8_t)SCSFAILbits.PLL2SCS;
    out->stoppll2 = (uint8_t)CLKDIAGbits.STOPPLL2;
}

void dspic33ak_clock_diag_clear_rcon(void)
{
    RCONbits.POR   = 0;
    RCONbits.BOR   = 0;
    RCONbits.IDLE  = 0;
    RCONbits.SLEEP = 0;
    RCONbits.WDTO  = 0;
    RCONbits.SWR   = 0;
    RCONbits.EXTR  = 0;
    RCONbits.CM    = 0;
}
