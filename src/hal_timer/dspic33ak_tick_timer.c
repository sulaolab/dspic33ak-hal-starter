/*
 * dspic33ak_tick_timer.c
 * ----------------------
 * 1 ms time base on Timer1. See dspic33ak_tick_timer.h.
 *
 * Timer1 runs from the instruction clock (Fcy = system clock / 2) with a 1:256
 * prescaler; PR1 is set so the timer rolls over every millisecond and the ISR
 * increments a 32-bit counter.
 */

#include <xc.h>
#include <stdint.h>

#include "dspic33ak_clock.h"        /* DSPIC33AK_CLOCK_SYS_HZ */
#include "dspic33ak_tick_timer.h"

/* Instruction clock: Fcy = system clock / 2 on dsPIC33A. */
#define SYSTICK_FCY_HZ   (DSPIC33AK_CLOCK_SYS_HZ / 2u)

static volatile uint32_t s_ms = 0u;

void systick_init(void)
{
    T1CONbits.ON    = 0;          /* stop while configuring          */
    T1CONbits.TCS   = 0;          /* clock source = Fcy              */
    T1CONbits.TCKPS = 0b11;       /* prescaler 1:256                 */

    TMR1 = 0;
    PR1  = (uint16_t)((SYSTICK_FCY_HZ / 256u / 1000u) - 1u);   /* 1 ms period */

    IFS1bits.T1IF = 0;
    IEC1bits.T1IE = 1;
    T1CONbits.ON  = 1;
}

void __attribute__((interrupt, context)) _T1Interrupt(void)
{
    IFS1bits.T1IF = 0;
    s_ms++;
}

uint32_t systick_ms(void)
{
    return s_ms;
}
