# dsPIC33AK Tick Timer HAL

## Overview

This directory contains a small Timer1-based 1 ms monotonic tick HAL for
`dspic33ak-hal-starter`.

The HAL is being shaped here before it is split into a standalone
`dspic33ak-timer-hal` repository.

## Status

- Device validation target: dsPIC33AK512MPS512.
- Compiler: XC-DSC v3.31.01.
- DFP: Microchip dsPIC33AK-MP DFP 1.3.185 or compatible.
- dsPIC33AK128MC106 is expected to be compatible by Timer1 register symmetry,
  but should be listed as hardware-validated only after a final HAL build is
  tested on that target.

## Design Policy

- Timer clock frequency is supplied by the caller.
- Clock tree and PLL setup are outside this HAL.
- The application owns the Timer1 interrupt vector.
- The HAL owns Timer1 registers after successful init.
- No busy-wait delay API is provided.
- No RTOS or scheduler dependency exists.

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

## Files

- `dspic33ak_tick_timer.h`
  - Public API and standard C types only.
- `dspic33ak_tick_timer.c`
  - Timer1 register configuration, period calculation, state, and IRQ handler
    body.
- `src/main.c`
  - Owns `_T1Interrupt()` in this starter project and forwards to the HAL.

## Basic Usage

```c
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

## API Summary

- `dspic33ak_tick_timer_init()`
  - Validates the config, selects a prescaler, configures Timer1, clears the
    counter, enables the interrupt, and starts Timer1.
- `dspic33ak_tick_timer_deinit()`
  - Disables the interrupt, stops Timer1, clears Timer1 registers, and marks the
    HAL uninitialized. If called before init, returns
    `DSPIC33AK_TICK_TIMER_ERR_NOT_INITIALIZED`.
- `dspic33ak_tick_timer_is_present()`
  - Returns whether Timer1 SFRs are present at compile time.
- `dspic33ak_tick_timer_is_initialized()`
  - Returns the internal initialized state.
- `dspic33ak_tick_timer_get_ms()`
  - Returns the current millisecond counter. After deinit, it returns the last
    value, but the value no longer advances.
- `dspic33ak_tick_timer_irq_handler()`
  - Clears the Timer1 interrupt flag and increments the millisecond counter.

## Timer Ownership

Timer1 is reserved by this HAL after successful init.

- Other modules must not configure or use Timer1 unless ownership is explicitly
  changed.
- The HAL does not define `_T1Interrupt()`.
- The application must define `_T1Interrupt()` and call
  `dspic33ak_tick_timer_irq_handler()`.

## Clock Requirements

The caller must pass the actual clock frequency seen by Timer1 in
`timer_clk_hz`.

For the current hal-starter clock tree:

```c
.timer_clk_hz = DSPIC33AK_CLOCK_SYS_HZ / 2u
```

The HAL does not include `dspic33ak_clock.h` and does not configure PLLs or
CLKGENs.

## Prescaler and Period Calculation

The HAL targets a fixed 1 kHz interrupt rate so that
`dspic33ak_tick_timer_get_ms()` is always a true millisecond source.

Prescaler candidates are checked in this order:

```text
1:1, 1:8, 1:64, 1:256
```

The first candidate whose rounded count fits in the 32-bit PR1 range is used:

```text
counts = round(timer_clk_hz / (prescaler * 1000))
PR1 = counts - 1
```

At 100 MHz Timer1 input:

```text
prescaler = 1:1
TCKPS = 0b00
PR1 = 99999
```

## Interrupt Integration

This starter keeps the vector in `src/main.c`:

```c
void __attribute__((interrupt, context)) _T1Interrupt(void)
{
    dspic33ak_tick_timer_irq_handler();
}
```

Projects that need additional Timer1 interrupt work can add it in the
application-owned vector before or after forwarding to the HAL.

## Wraparound Behavior

The tick counter is `uint32_t` and wraps after about 49.7 days. Users should
continue to use unsigned-difference timing:

```c
if ((uint32_t)(now - previous) >= interval_ms) {
    previous = now;
}
```

For XC-DSC v3.31.01 with `-O2` on dsPIC33AK512MPS512,
`dspic33ak_tick_timer_get_ms()` was checked as generated assembly and reads the
counter with a single `mov.l`.

## Known Limitations

- Only Timer1 is supported.
- The tick rate is fixed at 1000 Hz.
- No delay API is provided.
- No Timer2 high-resolution helper is included in this HAL phase.

## License

MIT No Attribution License (MIT-0). See the repository `LICENSE`.

Attribution is appreciated but not required.
