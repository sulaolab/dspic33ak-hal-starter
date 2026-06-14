#ifndef SYSTICK_H
#define SYSTICK_H

/*
 * systick.h
 * ---------
 * Minimal 1 ms time base on Timer1. Provides a monotonic millisecond counter,
 * used for non-blocking timing (heartbeat) and as the I2C HAL's timeout clock so
 * a stuck/empty bus can never hang the scan.
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

#endif /* SYSTICK_H */
