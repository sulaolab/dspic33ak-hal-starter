# dsPIC33AK Tick Timer HAL

## Overview

This directory contains the vendored Timer HAL snapshot used by
`dspic33ak-hal-starter`.

The reusable standalone implementation is maintained at:

- [dspic33ak-timer-hal](https://github.com/sulaolab/dspic33ak-timer-hal)

This starter project provides the board clock configuration, application-owned
Timer1 interrupt vector, and hardware integration/regression validation.

## Status

- Integrated target: dsPIC33AK512MPS512
- Compiler: XC-DSC v3.31.01
- DFP: Microchip dsPIC33AK-MP DFP 1.3.185 or compatible
- Timer1 input clock in this starter: 100 MHz
- Timer1 interrupt vector owner: `src/main.c`
- Upstream repository:
  [dspic33ak-timer-hal](https://github.com/sulaolab/dspic33ak-timer-hal)

The standalone HAL documents additional register-level compatibility targets.
This starter remains intentionally specific to dsPIC33AK512MPS512.

## Starter Integration

This starter supplies:

- `timer_clk_hz = DSPIC33AK_CLOCK_SYS_HZ / 2u`
- Timer1 input clock = 100 MHz
- Timer1 interrupt priority = 4
- `_T1Interrupt()` in `src/main.c`
- `dspic33ak_tick_timer_get_ms()` as the timeout source for I2C and CAN
- heartbeat and application timing from the same millisecond counter

## Basic Usage

```c
#include <xc.h>

#include "dspic33ak_clock.h"
#include "dspic33ak_tick_timer.h"

const dspic33ak_tick_timer_config_t tick_timer_config = {
    .timer_clk_hz = DSPIC33AK_CLOCK_SYS_HZ / 2u,
    .irq_priority = 4u,
    .run_in_idle = false,
};

if (dspic33ak_tick_timer_init(&tick_timer_config) != DSPIC33AK_TICK_TIMER_OK) {
    while (1) {
        Nop();
    }
}

void __attribute__((interrupt, context)) _T1Interrupt(void)
{
    dspic33ak_tick_timer_irq_handler();
}
```

Timeout users pass the millisecond getter directly:

```c
.get_ms = dspic33ak_tick_timer_get_ms,
```

## Tick Accuracy

The HAL targets a fixed 1 kHz interrupt rate, so each counter increment
represents approximately 1 ms.

In this starter, Timer1 receives an exact 100 MHz clock:

```text
prescaler = 1:1
TCKPS = 0b00
PR1 = 99999
```

Therefore, the configured period is exactly 1 ms for this project.

## Ownership

- The HAL owns Timer1 after successful initialization.
- `src/main.c` owns `_T1Interrupt()`.
- The interrupt vector forwards to `dspic33ak_tick_timer_irq_handler()`.
- Clock-tree setup remains owned by the starter.
- Other modules must not reconfigure Timer1 while the tick HAL is active.

## Integration Regression Checks

The Timer HAL was exercised together with:

- 1 Hz heartbeat
- UART startup output
- I2C scan
- I2C2 master / I2C3 slave loopback
- CAN internal loopback
- CAN live-bus timeout path
- RGB LED and switch demo
- application-owned Timer1 interrupt forwarding

Functional integration was validated. Absolute tick accuracy has not yet been
independently characterized with oscilloscope or logic-analyzer measurement.

## Scope

In scope:

- Timer1-based 1 ms monotonic tick.
- Timeout callback source for I2C, UART, CAN, and app code.
- Configurable Timer1 input clock.
- Configurable Timer1 interrupt priority.
- Init, deinit, presence, and initialized-state queries.
- Application-owned interrupt forwarding.

Out of scope:

- Timer2 high-resolution profiling.
- RTOS software timers.
- Busy-wait delay functions.
- Capture, compare, or PWM.
- External pulse counting.
- Gated timer mode.
- Callback scheduler.

## License

MIT No Attribution License (MIT-0). See the repository [LICENSE](../../LICENSE).

Attribution is appreciated but not required.
