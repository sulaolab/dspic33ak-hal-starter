/*
 * rgb_pot.c
 * ---------
 * Board component helper: potentiometer (ADC5) -> RGB LED (PWM1/2/3).
 * See rgb_pot.h.
 *
 * This is deliberately minimal, hand-written register code (no MCC, no HAL) so a
 * newcomer can see exactly what an ADC read and a PWM channel need on this
 * device. ADC5 channel 0 reads AD5AN0 (the pot on RA7); PWM generators PG1/2/3
 * dim the blue/green/red LED segments via PPS routed in board_rgb_pins_init().
 */

#include <xc.h>
#include <stdint.h>

#include "board.h"
#include "board_pins.h"
#include "dspic33ak_gpio.h"
#include "rgb_pot.h"

/* PWM generator period (Q4 format, as the PG period register expects). Off the
 * 200 MHz CLKGEN5 this is ~13 kHz - well above flicker, fine for an LED. */
#define RGB_PWM_PER        (15360u << 4)   /* = 245760 */
#define RGB_DEAD_TIME      (8u)
/* Brightness cap (~30%) so the LED is not uncomfortably bright: DC = PER*duty% * 3/10. */
#define RGB_BRIGHT_NUM     (3u)
#define RGB_BRIGHT_DEN     (10u)

#define POT_OFF_THRESHOLD  (50u)           /* below this -> show plain green */

/* ---- PWM (PG1/2/3) ---- */

#define PG_INIT(n)                                              \
    do {                                                        \
        PG##n##CON = 0x00000000UL;                              \
        PG##n##CONbits.CLKSEL  = 1;     /* PWM master clock   */ \
        PG##n##CONbits.MDCSEL  = 0;     /* use PGxDC          */ \
        PG##n##CONbits.MPERSEL = 0;     /* use PGxPER         */ \
        PG##n##CONbits.SOCS    = 0;     /* start-of-cycle=EOC */ \
        PG##n##PERbits.PER = RGB_PWM_PER - 1u;                  \
        PG##n##DCbits.DC   = 1u;        /* LED off            */ \
        PG##n##DTbits.DTH  = RGB_DEAD_TIME;                     \
        PG##n##DTbits.DTL  = RGB_DEAD_TIME;                     \
        PG##n##EVT1bits.UPDTRG   = 0b01; /* DC write auto-updates */ \
        PG##n##EVT1bits.PGTRGSEL = 0b000;                       \
        PG##n##IOCON1 = 0x00000000UL;                           \
        PG##n##IOCON1bits.PPSEN = 1;    /* output via PPS     */ \
        PG##n##IOCON1bits.PMOD  = 0;    /* complementary      */ \
        PG##n##IOCON1bits.PENH  = 1;                            \
        PG##n##IOCON1bits.PENL  = 1;                            \
        PG##n##CONbits.MSTEN = 1;                               \
        PG##n##CONbits.ON    = 1;                               \
    } while (0)

static void pwm_rgb_init(void)
{
    PCLKCONbits.MCLKSEL = 1;   /* PWM master clock = CLKGEN5 (PLL1) */
    MPER = 0;
    MDC  = 0;

    PG_INIT(1);   /* blue  (PWM1H) */
    PG_INIT(2);   /* green (PWM2H) */
    PG_INIT(3);   /* red   (PWM3H) */
}

/* duty: 0..100 (%). Scaled by the brightness cap and written to the channel. */
static void pwm_rgb_set(uint8_t ch, uint8_t duty)
{
    if (duty == 0u) {
        duty = 1u;   /* avoid a zero period-compare corner case */
    }
    uint32_t dc = ((uint32_t)RGB_PWM_PER * duty) / 100u * RGB_BRIGHT_NUM / RGB_BRIGHT_DEN;

    switch (ch) {
    case 1: PG1DCbits.DC = dc; break;   /* blue  */
    case 2: PG2DCbits.DC = dc; break;   /* green */
    case 3: PG3DCbits.DC = dc; break;   /* red   */
    default: break;
    }
}

/* ---- ADC5 (pot on channel 0 / AD5AN0 / RA7) ---- */

static void adc_pot_init(void)
{
    /* RA7 is analog for the pot. */
    static const dspic33ak_gpio_config_t pot_cfg = {
        .dir = DSPIC33AK_GPIO_DIR_INPUT, .pull = DSPIC33AK_GPIO_PULL_NONE,
        .analog = true, .open_drain = false, .initial_high = false,
    };
    (void)dspic33ak_gpio_config(BOARD_POT_PIN, &pot_cfg);

    AD5CON       = 0x00480000UL;   /* ADC clock/config (module off) */
    AD5CONbits.ON = 1;
    AD5DATAOVR   = 0x0UL;
    AD5STAT      = 0x0UL;
    AD5RSTAT     = 0x0UL;
    AD5CMPSTAT   = 0x0UL;
    AD5SWTRG     = 0x0UL;

    /* Channel 0: input AD5AN0, software trigger, single sample, integer result. */
    AD5CH0CON1   = 0x00200001UL;
    AD5CH0CON2   = 0x20000000UL;

    while (AD5CONbits.ADRDY == 0u) { }   /* wait for ADC ready */
    AD5CONbits.CALREQ = 1;
    while (AD5CONbits.CALRDY == 0u) { }  /* wait for self-calibration */
}

static uint16_t adc_pot_read(void)
{
    AD5SWTRGbits.CH0TRG = 1;                       /* software-trigger channel 0 */
    while (AD5STATbits.CH0RDY == 0u) { }           /* wait for conversion        */
    return (uint16_t)AD5CH0DATA;                   /* 12-bit result (0..4095)    */
}

/* ---- public ---- */

void rgb_pot_init(void)
{
    /* ADC (pot input) is independent of the RGB PWM output pins; always init
     * so rgb_pot_update() can read the pot without blocking on CH0RDY. */
    adc_pot_init();

    /* Skip PWM init if RGB pin config failed -- output would be unrouted,
     * but the pot ADC is already up so rgb_pot_update() will not hang. */
    if (board_rgb_pins_init()) {
        pwm_rgb_init();
    }
}

void rgb_pot_update(void)
{
    uint16_t pot  = adc_pot_read();          /* 0..4095          */
    int16_t  rate = (int16_t)((uint32_t)pot * 100u / 4096u);   /* 0..100 */

    uint8_t blue, green, red;

    if (pot < POT_OFF_THRESHOLD) {
        red = 0u; green = 60u; blue = 0u;    /* idle: plain green */
    } else {
        int16_t g = 50 - ((rate > 50) ? (rate - 50) : (50 - rate));
        red   = (uint8_t)rate;
        blue  = (uint8_t)(100 - rate);
        green = (uint8_t)((g < 0) ? 0 : g);
    }

    pwm_rgb_set(1, blue);
    pwm_rgb_set(2, green);
    pwm_rgb_set(3, red);
}
