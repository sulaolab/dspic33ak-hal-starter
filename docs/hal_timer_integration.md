# Starter Timer HAL Integration Notes

This file records `dspic33ak-hal-starter` timer wiring that intentionally lives
outside `src/hal_timer/`. The HAL directory is kept project-neutral so it can be
copied as a whole from the validated common HAL source.

## Target

- Integrated target: dsPIC33AK512MPS512
- Compiler used during validation: XC-DSC v3.31.01
- DFP used during validation: Microchip dsPIC33AK-MP DFP 1.3.185

## Timer1 Tick

- Timer1 is the 1 ms system tick source.
- Timer1 input clock in this starter: `DSPIC33AK_CLOCK_SYS_HZ / 2u` = 100 MHz.
- Timer1 interrupt priority:
  `DSPIC33AK_TICK_TIMER_DEFAULT_IRQ_PRIORITY` (`4u`).
- `src/main.c` owns `_T1Interrupt()` and forwards to
  `dspic33ak_tick_timer_irq_handler()`.
- `dspic33ak_tick_timer_get_ms()` is used as the timeout source for I2C, CAN,
  heartbeat, and application timing.

## Timer2 High-Resolution Counter

- Timer2 input clock in this starter: `DSPIC33AK_CLOCK_SYS_HZ / 2u` = 100 MHz.
- Timer2 is initialized as a free-running high-resolution counter.
- A boot-time self-check prints Timer2 count progression and 100 MHz conversion
  sanity checks after the startup banner.

## Integration Regression Checks

The Timer HAL has been exercised together with:

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
