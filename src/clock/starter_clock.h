#ifndef STARTER_CLOCK_H
#define STARTER_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Starter-specific clock policy.
 *
 * The reusable Clock HAL under src/hal_clock owns PLL / CLKGEN mechanics. This
 * layer owns the board/demo policy: FRC -> PLL1 at 200 MHz, application CLKGENs
 * from PLL1, and CLKGEN10 /10 for CAN FD FCAN.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define STARTER_CLOCK_SYS_HZ       (200000000UL)
#define STARTER_CLOCK_FCY_HZ       (STARTER_CLOCK_SYS_HZ / 2UL)
#define STARTER_CLOCK_CAN_FCAN_HZ  (20000000UL)

bool starter_clock_init(void);
bool starter_clock_can_init(void);

#ifdef __cplusplus
}
#endif

#endif /* STARTER_CLOCK_H */
