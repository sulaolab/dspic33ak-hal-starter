/*
 * dspic33ak_tick_timer.c
 * ----------------------
 * 1 ms time base on Timer1. See dspic33ak_tick_timer.h.
 *
 * Timer1 uses a caller-supplied input clock. The HAL selects the smallest
 * available prescaler that can generate an exact or nearest 1 ms period within
 * the 32-bit PR1 range, then the interrupt handler increments a 32-bit counter.
 */

#include "dspic33ak_tick_timer.h"

#include <xc.h>

#define DSPIC33AK_TICK_TIMER_HZ             1000u
#define DSPIC33AK_TICK_TIMER_MAX_PRIORITY   7u

#if defined(T1CON) && defined(TMR1) && defined(PR1) && \
    defined(_T1IF) && defined(_T1IE) && defined(_T1IP)
#define DSPIC33AK_TICK_TIMER_PRESENT         1
#else
#define DSPIC33AK_TICK_TIMER_PRESENT         0
#endif

typedef struct {
    uint16_t divisor;
    uint8_t tckps;
} prescaler_option_t;

static const prescaler_option_t prescaler_options[] = {
    { 1u,   0b00u },
    { 8u,   0b01u },
    { 64u,  0b10u },
    { 256u, 0b11u },
};

static volatile uint32_t tick_ms = 0u;
static volatile bool tick_initialized = false;

static dspic33ak_tick_timer_status_t calc_period_reg(
    const dspic33ak_tick_timer_config_t *config,
    uint32_t *period_reg,
    uint8_t *tckps);

dspic33ak_tick_timer_status_t dspic33ak_tick_timer_init(
    const dspic33ak_tick_timer_config_t *config)
{
    uint32_t period_reg;
    uint8_t tckps;
    dspic33ak_tick_timer_status_t status;

    status = calc_period_reg(config, &period_reg, &tckps);
    if (status != DSPIC33AK_TICK_TIMER_OK) {
        return status;
    }

#if DSPIC33AK_TICK_TIMER_PRESENT
    _T1IE = 0;
    T1CONbits.ON = 0;
    _T1IF = 0;
    tick_initialized = false;

    T1CON = 0u;
    TMR1 = 0;
    PR1 = period_reg;
    T1CONbits.TCS = 0;
    T1CONbits.TCKPS = tckps;
    T1CONbits.SIDL = config->run_in_idle ? 0u : 1u;
    _T1IP = config->irq_priority;
    tick_ms = 0u;
    tick_initialized = true;
    _T1IF = 0;
    _T1IE = 1;
    T1CONbits.ON = 1;

    return DSPIC33AK_TICK_TIMER_OK;
#else
    return DSPIC33AK_TICK_TIMER_ERR_NOT_PRESENT;
#endif
}

dspic33ak_tick_timer_status_t dspic33ak_tick_timer_deinit(void)
{
#if DSPIC33AK_TICK_TIMER_PRESENT
    if (!tick_initialized) {
        return DSPIC33AK_TICK_TIMER_ERR_NOT_INITIALIZED;
    }

    _T1IE = 0;
    T1CONbits.ON = 0;
    _T1IF = 0;
    TMR1 = 0;
    PR1 = 0;
    T1CON = 0u;
    tick_ms = 0u;
    tick_initialized = false;

    return DSPIC33AK_TICK_TIMER_OK;
#else
    return DSPIC33AK_TICK_TIMER_ERR_NOT_PRESENT;
#endif
}

bool dspic33ak_tick_timer_is_present(void)
{
#if DSPIC33AK_TICK_TIMER_PRESENT
    return true;
#else
    return false;
#endif
}

uint32_t dspic33ak_tick_timer_get_ms(void)
{
    if (!tick_initialized) {
        return 0u;
    }

    return tick_ms;
}

bool dspic33ak_tick_timer_is_initialized(void)
{
    return tick_initialized;
}

void dspic33ak_tick_timer_irq_handler(void)
{
#if DSPIC33AK_TICK_TIMER_PRESENT
    _T1IF = 0;

    if (tick_initialized) {
        tick_ms++;
    }
#endif
}

static dspic33ak_tick_timer_status_t calc_period_reg(
    const dspic33ak_tick_timer_config_t *config,
    uint32_t *period_reg,
    uint8_t *tckps)
{
    uint8_t i;

    if (!dspic33ak_tick_timer_is_present()) {
        return DSPIC33AK_TICK_TIMER_ERR_NOT_PRESENT;
    }

    if ((config == 0) || (period_reg == 0) || (tckps == 0) ||
        (config->timer_clk_hz == 0u) ||
        (config->irq_priority == 0u) ||
        (config->irq_priority > DSPIC33AK_TICK_TIMER_MAX_PRIORITY)) {
        return DSPIC33AK_TICK_TIMER_ERR_INVALID_ARG;
    }

    for (i = 0u; i < (uint8_t)(sizeof(prescaler_options) / sizeof(prescaler_options[0])); i++) {
        const uint64_t denominator =
            (uint64_t)prescaler_options[i].divisor * DSPIC33AK_TICK_TIMER_HZ;
        const uint64_t counts =
            ((uint64_t)config->timer_clk_hz + (denominator / 2u)) / denominator;

        if (counts == 0u) {
            continue;
        }

        if ((counts - 1u) <= UINT32_MAX) {
            *period_reg = (uint32_t)(counts - 1u);
            *tckps = prescaler_options[i].tckps;
            return DSPIC33AK_TICK_TIMER_OK;
        }
    }

    return DSPIC33AK_TICK_TIMER_ERR_OUT_OF_RANGE;
}
