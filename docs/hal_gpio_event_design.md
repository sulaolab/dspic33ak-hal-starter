# GPIO CN Event Design

This note describes the small GPIO Change Notification (CN) event layer used by
the hal-starter SW3 validation path. It is intentionally minimal hardware
integration code above the GPIO core, not a full production-grade GPIO event
framework.

## File Split

- `src/hal_gpio/dspic33ak_gpio.h` and `src/hal_gpio/dspic33ak_gpio.c` are the
  GPIO core. They handle ANSEL, TRIS, LAT, PORT, ODC, CNPU, and CNPD.
- `src/hal_gpio/dspic33ak_gpio_event.h` and
  `src/hal_gpio/dspic33ak_gpio_event.c` provide the CN event layer above the
  existing GPIO pin representation in the same vendored GPIO HAL snapshot.
- `src/board_components/led_sw.c` owns the validation example and the
  `_CNBInterrupt()` vector.
- `firmware.X/nbproject/configurations.xml` includes the GPIO core and event
  layer from `../src/hal_gpio/` in the MPLAB X build and adds
  `..\src\hal_gpio` to the C include path.

## API

The current event API is intentionally small:

```c
bool dspic33ak_gpio_event_attach(dspic33ak_gpio_pin_t pin,
                                 dspic33ak_gpio_event_edge_t trigger,
                                 dspic33ak_gpio_event_callback_t callback,
                                 void *user_data);

bool dspic33ak_gpio_event_detach(dspic33ak_gpio_pin_t pin);

bool dspic33ak_gpio_event_irq_enable(dspic33ak_gpio_pin_t pin,
                                     uint8_t priority);

bool dspic33ak_gpio_event_irq_disable(dspic33ak_gpio_pin_t pin);

bool dspic33ak_gpio_event_rp_attach(dspic33ak_gpio_rp_t rp,
                                    dspic33ak_gpio_event_edge_t trigger,
                                    dspic33ak_gpio_event_callback_t callback,
                                    void *user_data);

bool dspic33ak_gpio_event_rp_detach(dspic33ak_gpio_rp_t rp);

bool dspic33ak_gpio_event_rp_irq_enable(dspic33ak_gpio_rp_t rp,
                                        uint8_t priority);

bool dspic33ak_gpio_event_rp_irq_disable(dspic33ak_gpio_rp_t rp);

void dspic33ak_gpio_event_process_isr(void);
```

Phase 1 accepts `DSPIC33AK_GPIO_EVENT_EDGE_EITHER` for `attach()`. The enum also
contains `FALLING` and `RISING` because callbacks report the detected edge, but
single-edge attach policy is intentionally deferred until the dsPIC33AK
`CNEN0x`/`CNEN1x` polarity mapping is finalized.

## ISR Ownership

The HAL event layer does not define interrupt vectors. The application or
example owns the vector and forwards to the event dispatcher:

```c
void __attribute__((__interrupt__, __no_auto_psv__)) _CNBInterrupt(void)
{
    dspic33ak_gpio_event_process_isr();
}
```

This keeps the HAL reusable for different applications that may choose different
CN ports, nesting rules, or interrupt routing. The vector remains application
owned, while the event layer can optionally arm the matching CN port interrupt
priority, flag, and enable bits.

## CN Flag Ownership

`dspic33ak_gpio_event_process_isr()` owns cleanup for pins registered through
the event layer:

- It reads the per-port `CNFx` pending bits.
- It handles only bits registered by `dspic33ak_gpio_event_attach()`.
- It clears the handled `CNFx` bits.
- It clears the matching port interrupt flag, such as `CNBIF` through the port's
  `IFSx` register mask.

The board component owns the vector and policy for this validation path, but it
does not name the `_CNxIP`, `_CNxIF`, or `_CNxIE` symbols directly anymore.
`led_sw_init()` attaches SW3 by RP number and calls
`dspic33ak_gpio_event_rp_irq_enable(BOARD_SW3_RP, 4u)` after the switch is
configured.

## Edge Detection

`attach()` captures the current `PORTx` level as `previous_high` before enabling
CN for that pin. During `dspic33ak_gpio_event_process_isr()`:

1. The dispatcher reads the current `PORTx` level.
2. Each pending registered pin compares current level with `previous_high`.
3. A high-to-low transition reports `DSPIC33AK_GPIO_EVENT_EDGE_FALLING`.
4. A low-to-high transition reports `DSPIC33AK_GPIO_EVENT_EDGE_RISING`.
5. The saved `previous_high` level is updated before the callback runs.

For the board's active-low SW3 on RB2:

- falling edge = released high to pressed low = press = LED5 ON
- rising edge = pressed low to released high = release = LED5 OFF

The SW3 callback only updates volatile state. The main loop writes LED5, so the
callback stays small and does not directly touch LED outputs.

## What Attach Does Not Do

`dspic33ak_gpio_event_attach()` does not modify:

- ANSEL
- TRIS
- CNPU/CNPD
- PPS
- interrupt priority
- interrupt enable bits

The integration layer must configure the pin as a digital input before attaching
an event. In the current LED/SW board component, `led_sw_init()` calls
`dspic33ak_gpio_rp_config()` with a config struct (`dir=INPUT`, `pull=UP`,
`analog=false`) for SW1, SW2, and SW3 in a single glitch-aware call before
attaching the SW3 event. Interrupt priority and enable are a separate explicit
step through `dspic33ak_gpio_event_rp_irq_enable()`.

## Current Validation Behavior

- SW1 remains a polling input and drives LED7.
- SW2 remains a polling input and drives LED6.
- SW3 uses the CN event layer and drives LED5.
- SW3 is active-low.
- `_CNBInterrupt()` is defined in `src/board_components/led_sw.c`, not in the HAL.
- SW1, SW2, and SW3 are named as RP numbers in `board_pins.h`; SW3's CN line is
  derived from `BOARD_SW3_RP`.

Expected serial banner line:

```text
 LEDs: SW1/SW2 poll LED7/LED6; SW3 CN event drives LED5.
```

## Limitations

- Phase 1 supports `EDGE_EITHER` attach only.
- There is no debounce filtering.
- There is one callback slot per port bit.
- The event layer uses a single dispatcher that scans all supported CN ports.
- It does not arbitrate shared CN port ownership with unrelated code that also
  directly modifies `CNENx`, `CNFx`, or the same port interrupt flag.
- It does not check package-level pin availability.
- It does not configure analog protection or PPS ownership.

## Future Wrapper Mapping

This layer is a useful stepping stone for a future GPIO wrapper:

- `dspic33ak_gpio_pin_t` can remain the low-level pin handle.
- `dspic33ak_gpio_config()` maps naturally to pin initialization and direction.
- `dspic33ak_gpio_write()`, `toggle()`, `read()`, and `read_output()` map to
  output and input operations.
- `dspic33ak_gpio_set_pull()` and `set_open_drain()` map to optional pin
  attributes.
- `dspic33ak_gpio_event_attach()` can become the basis for event callbacks after
  edge policy, debounce, and ownership rules are defined.

Resolved decisions (no longer open):

- **PPS ownership:** `dspic33ak_pps.*` is now a companion layer in the same
  `src/hal_gpio/` HAL family. It provides generic PPS register routing without
  board-specific knowledge; board-specific RP assignments live in `board.c` /
  `board_pins.h`.
- **Pin handle style:** PPS-capable pins use the RP number (single identifier
  for both GPIO HAL and PPS HAL). Non-PPS GPIO-only pins use the packed
  `(port, bit)` handle via `DSPIC33AK_GPIO_PIN()`.
- **ANSEL management:** the caller is responsible for ANSEL. `dspic33ak_gpio_config()`
  and `dspic33ak_gpio_config_digital_output/input()` set `analog=false` explicitly.
- **CN interrupt setup:** the vector remains application-owned, but CN port
  priority / flag / enable symbols are hidden behind
  `dspic33ak_gpio_event_irq_enable()` and the RP wrapper.

Still open for a future wrapper layer:

- Whether single-edge attach (`FALLING` / `RISING`) should be exposed before
  fully documenting dsPIC33AK `CNEN0x`/`CNEN1x` polarity mapping.
- Debounce filtering and multiple-callback-slots per pin.
- Protection against shared CN port ownership with unrelated code.
