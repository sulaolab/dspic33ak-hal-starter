#ifndef DSPIC33AK_TICK_TIMER_H
#define DSPIC33AK_TICK_TIMER_H

/*
 * dspic33ak_tick_timer.h
 * ----------------------
 * Minimal 1 ms time base on Timer1. Provides a monotonic millisecond counter,
 * used for non-blocking timing (heartbeat) and as the I2C/CAN HAL timeout clock
 * so a stuck/empty bus can never hang the demo.
 *
 * Phase 1 keeps the original hal-starter public API names:
 *   - systick_init()
 *   - systick_ms()
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start Timer1 with a 1 ms interrupt. Call once after the clock is up. */
void systick_init(void);

/* Milliseconds elapsed since systick_init(). Monotonic; wraps after ~49 days. */
uint32_t systick_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_TICK_TIMER_H */
