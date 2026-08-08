#ifndef NORA_TICK_TIMER_H
#define NORA_TICK_TIMER_H

/*
 * nora_tick_timer.h
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
    NORA_TICK_TIMER_OK = 0,
    NORA_TICK_TIMER_ERR_INVALID_ARG,
    NORA_TICK_TIMER_ERR_NOT_PRESENT,
    NORA_TICK_TIMER_ERR_NOT_INITIALIZED,
    NORA_TICK_TIMER_ERR_OUT_OF_RANGE
} nora_tick_timer_status_t;

/* Recommended CPU interrupt priority for the 1 ms Timer1 tick. Applications may
 * supply a different non-zero priority when their interrupt ordering requires
 * it. */
#define NORA_TICK_TIMER_DEFAULT_IRQ_PRIORITY   4u

typedef struct {
    /* Actual input clock supplied to Timer1, in Hz. */
    uint32_t timer_clk_hz;

    /* CPU interrupt priority for the Timer1 tick. Valid range: 1..7. */
    uint8_t irq_priority;

    /* Keep Timer1 running while the CPU is in Idle mode. */
    bool run_in_idle;
} nora_tick_timer_config_t;

/* Start Timer1 with a periodic interrupt. Call once after the clock is up. */
nora_tick_timer_status_t nora_tick_timer_init(
    const nora_tick_timer_config_t *config);

nora_tick_timer_status_t nora_tick_timer_deinit(void);

bool nora_tick_timer_is_present(void);

/* Milliseconds elapsed since init. Monotonic; wraps after ~49 days at 1 kHz.
 * Returns 0 before successful init and after deinit. */
uint32_t nora_tick_timer_get_ms(void);

/* True after successful initialization. */
bool nora_tick_timer_is_initialized(void);

void nora_tick_timer_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* NORA_TICK_TIMER_H */
