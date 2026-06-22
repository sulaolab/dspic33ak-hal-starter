#ifndef DSPIC33AK_TICK_TIMER_H
#define DSPIC33AK_TICK_TIMER_H

/*
 * dspic33ak_tick_timer.h
 * ----------------------
 * Minimal Timer1-based 1 ms monotonic tick source for non-blocking timing
 * and timeout callbacks.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DSPIC33AK_TICK_TIMER_OK = 0,
    DSPIC33AK_TICK_TIMER_ERR_INVALID_ARG,
    DSPIC33AK_TICK_TIMER_ERR_NOT_PRESENT,
    DSPIC33AK_TICK_TIMER_ERR_NOT_INITIALIZED,
    DSPIC33AK_TICK_TIMER_ERR_OUT_OF_RANGE
} dspic33ak_tick_timer_status_t;

typedef struct {
    uint32_t timer_clk_hz;
    uint8_t irq_priority;
    bool run_in_idle;
} dspic33ak_tick_timer_config_t;

/* Start Timer1 with a periodic interrupt. Call once after the clock is up. */
dspic33ak_tick_timer_status_t dspic33ak_tick_timer_init(
    const dspic33ak_tick_timer_config_t *config);

dspic33ak_tick_timer_status_t dspic33ak_tick_timer_deinit(void);

bool dspic33ak_tick_timer_is_present(void);

/* Milliseconds elapsed since init. Monotonic; wraps after ~49 days at 1 kHz. */
uint32_t dspic33ak_tick_timer_get_ms(void);

/* True after successful initialization. */
bool dspic33ak_tick_timer_is_initialized(void);

void dspic33ak_tick_timer_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_TICK_TIMER_H */
