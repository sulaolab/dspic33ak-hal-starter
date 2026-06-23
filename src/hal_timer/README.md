# dsPIC33AK Timer HAL

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
- Timer2 input clock in this starter: 100 MHz
- Timer1 interrupt vector owner: `src/main.c`
- Upstream repository:
  [dspic33ak-timer-hal](https://github.com/sulaolab/dspic33ak-timer-hal)

The standalone HAL documents additional register-level compatibility targets.
This starter remains intentionally specific to dsPIC33AK512MPS512.

## Starter Integration

This starter supplies:

- `timer_clk_hz = DSPIC33AK_CLOCK_SYS_HZ / 2u`
- Timer1 input clock = 100 MHz
- Timer2 input clock = 100 MHz
- Timer1 interrupt priority = 4
- `_T1Interrupt()` in `src/main.c`
- `dspic33ak_tick_timer_get_ms()` as the timeout source for I2C and CAN
- heartbeat and application timing from the same millisecond counter
- a boot-time Timer2 high-resolution counter self-check printed after the
  banner

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

The high-resolution counter is initialized separately:

```c
#include "dspic33ak_high_res_timer.h"

const dspic33ak_high_res_timer_config_t high_res_timer_config = {
    .timer_clk_hz = DSPIC33AK_CLOCK_SYS_HZ / 2u,
    .run_in_idle = false,
};

if (dspic33ak_high_res_timer_init(&high_res_timer_config) !=
    DSPIC33AK_HIGH_RES_TIMER_OK) {
    while (1) {
        ;
    }
}
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

`dspic33ak_tick_timer_get_ms()` returns 0 before successful initialization and
after `dspic33ak_tick_timer_deinit()`.

## Ownership

- The HAL owns Timer1 after successful initialization.
- The HAL owns Timer2 after successful high-resolution timer initialization.
- `src/main.c` owns `_T1Interrupt()`.
- The interrupt vector forwards to `dspic33ak_tick_timer_irq_handler()`.
- Clock-tree setup remains owned by the starter.
- No `_T2Interrupt()` vector is defined; Timer2 runs as an interrupt-free
  free-running counter.
- Other modules must not reconfigure Timer1 or Timer2 while the Timer HAL is
  active.

## High-Resolution Counter

The Timer2 path provides:

- 32-bit free-running raw count reads.
- wraparound-safe elapsed count calculation for intervals shorter than one full
  32-bit Timer2 cycle.
- integer microsecond conversion, saturating to `UINT32_MAX` on overflow.
- 0.1 us conversion via `*_us_x10()` helpers, saturating to `UINT32_MAX` on
  overflow.

At 100 MHz, one Timer2 count is 10 ns and a full 32-bit cycle is about
42.95 seconds. The conversion helpers use 64-bit division. Results are
truncated to the requested unit and saturate to `UINT32_MAX` if the converted
value does not fit in `uint32_t`, so interrupt code should normally store raw
counts and convert them later in foreground/status code.

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
- Timer2 high-resolution counter initialization, count progression, and 100 MHz
  conversion sanity check

Functional integration was validated. Absolute tick accuracy has not yet been
independently characterized with oscilloscope or logic-analyzer measurement.

## Scope

In scope:

- Timer1-based 1 ms monotonic tick.
- Timer2-based high-resolution free-running counter.
- Timeout callback source for I2C, UART, CAN, and app code.
- Configurable Timer1 input clock.
- Configurable Timer2 input clock.
- Configurable Timer1 interrupt priority.
- Init, deinit, presence, and initialized-state queries.
- Application-owned interrupt forwarding.

Out of scope:

- RTOS software timers.
- Busy-wait delay functions.
- Capture, compare, or PWM.
- External pulse counting.
- Gated timer mode.
- Callback scheduler.

## License

MIT No Attribution License (MIT-0). See the repository [LICENSE](../../LICENSE).

Attribution is appreciated but not required.
