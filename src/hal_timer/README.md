# dsPIC33AK Timer HAL Notes

This directory contains the hal-starter Timer1 tick service after the physical
move into `src/hal_timer/`.

This first integration is intentionally behavior-preserving:

- Public API names, arguments, and return values are unchanged.
- Timer1 register settings are unchanged.
- Timer1 ISR ownership is unchanged.
- Existing `systick_init()` / `systick_ms()` call sites are retained.
- No Timer2 high-resolution timer is introduced in hal-starter.
- No `dspic33ak_timer_reg.h` is introduced in this phase.

## Files

- `dspic33ak_tick_timer.c/.h`
  - Existing 1 ms system tick service.
  - Keeps `systick_init()` and `systick_ms()`.
  - Owns Timer1 and `_T1Interrupt()`.

## Timer1 Ownership

Timer1 is reserved by this HAL as the 1 ms system tick source.

- Timer1 is occupied by `dspic33ak_tick_timer.c`.
- The Timer1 interrupt vector `_T1Interrupt()` is owned by this HAL.
- Other modules must not use Timer1 or define another `_T1Interrupt()`.

## Known Limitations

### Timer1 tick accuracy

Timer1 currently uses a fixed 1:256 prescaler and integer PR1 calculation:

```c
PR1 = (SYSTICK_FCY_HZ / 256u / 1000u) - 1u;
```

Depending on `SYSTICK_FCY_HZ`, this can introduce tick error due to integer
truncation. This phase does not change it.

### Timer1 interrupt priority

`systick_init()` currently does not explicitly set Timer1 interrupt priority.
This phase preserves the existing behavior.

### 32-bit tick read

The millisecond counter is a 32-bit volatile value accessed by both
`_T1Interrupt()` and normal code. This phase preserves the existing
implementation.

## Follow-up Candidates

- Decide whether to rename the public API to `dspic33ak_tick_timer_*`.
- Keep `systick_*` as compatibility wrappers if a rename is done later.
- Add config structures for clock frequency, tick frequency, and IRQ priority.
- Consider a future optional Timer2 high-resolution profiling timer.
- Consider `dspic33ak_timer_reg.h` if Timer register access grows.
