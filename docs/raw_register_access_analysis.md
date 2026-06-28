# Raw Register Access Analysis

Analysis date: 2026-06-28

Working branch: `reduce-raw-register-access`

Reference repository inspected: `sulaolab/perseus_512_96K`

## Goal

Move board and application code away from direct GPIO/PPS register access where
the existing `src/hal_gpio/` APIs can express the operation. The preferred style
for PPS-capable pins is RP-first:

```c
dspic33ak_gpio_rp_config_digital_output(rp, initial_high);
dspic33ak_pps_route_output(signal, rp);
```

or:

```c
dspic33ak_gpio_rp_config_digital_input(rp);
dspic33ak_pps_route_input(signal, rp);
```

Packed GPIO pins via `DSPIC33AK_GPIO_PIN(port, bit)` remain appropriate for
non-PPS GPIO-only pins.

## Current State

The starter is already mostly converted for normal board pinmux work.
`src/board.c` uses the RP-first GPIO API plus the PPS HAL for:

- UART1 TX/RX
- CAN1 TX/RX
- SPI4 SDO/SCK/SDI
- RGB PWM output pins

Non-PPS GPIO controls, such as SST26 CS/WP/RST and CAN STBY, are configured
through the packed-pin GPIO API. `src/board_components/led_sw.c` and
`src/board_components/sst26_min.c` also use the GPIO HAL for LED, switch, CS,
and reset operations.

The important distinction is that `src/hal_gpio/` intentionally contains direct
SFR access. That is the HAL boundary. The cleanup target is direct access that
has leaked into board, component, or application code when a public HAL API
already exists or should exist.

## Findings In This Repository

### GPIO/PPS-Related Direct Access Outside `src/hal_gpio/`

The first app-only cleanup pass converted the direct ANSEL users that can be
expressed with the existing GPIO HAL, without changing any `src/hal_xxx/`
implementation files:

| Location | Current direct access | Notes |
|---|---|---|
| `src/board.c` | formerly `ANSELA = 0`, `ANSELB = 0`, `ANSELC = 0`, `ANSELD = 0` | Now calls `dspic33ak_gpio_set_analog(..., false)` across ports A..D. |
| `src/board_pins.h` | formerly `BOARD_POT_ANSEL_PORT ANSELA`, `BOARD_POT_ANSEL_BIT` | Now names the pot as `BOARD_POT_PIN` using `DSPIC33AK_GPIO_PIN()`. |
| `src/board_components/rgb_pot.c` | formerly `BOARD_POT_ANSEL_PORT |= ...` | Now configures RA7 through `dspic33ak_gpio_config()` with `analog = true`. |
| `src/board_components/led_sw.c` | `_CNBIP`, `_CNBIF`, `_CNBIE` | CN vector setup is intentionally still owned by the component per `docs/hal_gpio_event_design.md`. A small event IRQ API could move this into `hal_gpio_event`. |

No active `_RPnnR` / `_U1RXR` / `_SSxR` style PPS writes were found outside
`src/hal_gpio/` in this starter. The comments in `dspic33ak_pps.h` are examples,
not live register writes.

### Non-GPIO Direct Access

These are direct SFR uses, but they are not directly replaceable by `hal_gpio`:

| Location | Current direct access | Likely ownership |
|---|---|---|
| `src/board.c` | `PMD3bits.C1MD = 0` | CAN module power/PMD ownership. Could move into CAN board bring-up helper or CAN HAL policy later. |
| `src/board_components/rgb_pot.c` | PWM generator registers, ADC5 registers | Minimal board component/device code. Not a GPIO cleanup unless a PWM/ADC HAL is introduced. |
| `src/app/i2c_loopback.c` | `_I2C3IP`, `_I2C3RXIP`, `_I2C3TXIP` | I2C interrupt priority setup. Could be covered by a future I2C IRQ helper, not by `hal_gpio`. |
| `src/clock/dspic33ak_clock.c` | CLK/PLL registers | Clock HAL owns these direct accesses. |
| `src/hal_timer/`, `src/hal_can/`, `src/hal_uart/`, `src/hal_i2c/` | Peripheral registers and interrupt bits | HAL-internal direct access; expected. |

## Reference Repository Pattern

`sulaolab/perseus_512_96K` shows the intended RP-first style clearly. Its
board-facing SPI/TDM pin setup configures electrical attributes first, then
routes PPS:

```c
if (!dspic33ak_gpio_rp_config_digital_input(25u)) return false;
if (!dspic33ak_pps_route_input(DSPIC33AK_PPS_INPUT_SS1, 25u)) return false;

if (!dspic33ak_gpio_rp_config_digital_output(101u, false)) return false;
if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_SDO1, 101u)) return false;
```

The useful rules to copy are:

- Use the RP number as the single identifier for PPS-capable pins.
- Configure GPIO electrical state before routing PPS output.
- Fail fast when a GPIO or PPS step fails.
- Keep raw `_RPOUT_*`, `_RPnnR`, and RPINRx details inside the PPS layer.
- Use packed-pin GPIO handles only for pins that are not naturally represented
  as RP numbers.

The reference repository also confirms that some raw access may remain at
well-defined boundaries. It still has examples such as:

- `ANSELA..D = 0` for a local digital-default step.
- A documented `_ADTRG31R` ADC trigger PPS write that is not yet covered by the
  PPS HAL.
- CLC pass-through setup that currently needs special token-pasted PPS writes
  and CLC register programming.
- Direct CN interrupt setup where the app owns vector/priority policy.

That suggests the practical target is not "no SFR names anywhere"; it is "no
avoidable GPIO/PPS SFR access in board/application code when the HAL can express
the operation."

## Recommended Cleanup Order

### 1. Convert Pot ANSEL Handling

Done in the first app-only cleanup pass. This was low risk and directly aligned
with the existing GPIO HAL.

Implemented shape:

- Replace `BOARD_POT_ANSEL_PORT` and `BOARD_POT_ANSEL_BIT` with a board pin
  handle such as:

```c
#define BOARD_POT_PIN DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_A, 7)
```

- In `rgb_pot.c`, configure RA7 through `dspic33ak_gpio_config()`:

```c
static const dspic33ak_gpio_config_t pot_cfg = {
    .dir = DSPIC33AK_GPIO_DIR_INPUT,
    .pull = DSPIC33AK_GPIO_PULL_NONE,
    .analog = true,
    .open_drain = false,
    .initial_high = false,
};
(void)dspic33ak_gpio_config(BOARD_POT_PIN, &pot_cfg);
```

This removes the board-level ANSEL register name and keeps analog selection
inside the GPIO HAL.

### 2. Handle Bulk Digital Default

Done in the first app-only cleanup pass for ports A..D, using the existing
`dspic33ak_gpio_set_analog()` API. A future GPIO HAL helper could make this less
verbose, but that would require changing `src/hal_gpio/`.


### 3. Consider A CN IRQ Enable Helper

`docs/hal_gpio_event_design.md` currently says the application owns vector,
priority, and interrupt enable bits. If the cleanup should also cover `_CNBIP`,
`_CNBIF`, and `_CNBIE`, add a small API in `dspic33ak_gpio_event.*`, for example:

```c
bool dspic33ak_gpio_event_irq_enable(dspic33ak_gpio_pin_t pin, uint8_t priority);
bool dspic33ak_gpio_event_irq_disable(dspic33ak_gpio_pin_t pin);
```

This would keep the actual vector in `led_sw.c`, while moving priority/flag/IEC
details into the event layer.

### 4. Leave Peripheral HAL Internals Alone

Direct register access inside `src/hal_uart/`, `src/hal_i2c/`, `src/hal_can/`,
`src/hal_timer/`, `src/hal_gpio/`, and `src/clock/` is currently the intended
implementation boundary. Refactoring those should be treated as separate HAL
work, not part of the GPIO/PPS cleanup.

## Proposed Acceptance Criteria

A first cleanup pass can be considered successful when:

- Board/component GPIO electrical configuration uses `dspic33ak_gpio_*()` or
  `dspic33ak_gpio_rp_*()` instead of direct `ANSEL`, `TRIS`, `LAT`, `PORT`,
  `ODC`, `CNPU`, or `CNPD` writes.
- Board/component PPS routing uses `dspic33ak_pps_route_*()` instead of direct
  `_RPnnR` or RPINRx writes.
- Remaining direct SFR access outside HAL folders is either non-GPIO
  peripheral setup or a documented exception.
- `board_pins.h` names pins as RP numbers or `DSPIC33AK_GPIO_PIN()` handles,
  not as raw register names.
