# Source Layout

This starter keeps reusable HAL code and board-specific integration code in
separate folders. The goal is that each public HAL can be compared with, copied
from, or updated from its source repository without rediscovering which files
are part of the HAL and which files are starter glue.

## Vendored HAL folders

Reusable HAL snapshots live in module-specific `src/hal_xxx/` folders:

| Path | Ownership |
|---|---|
| `src/hal_clock/` | Generic dsPIC33AK Clock HAL: logical PLL / CLKGEN programming through core, device, and register-adaptation layers. |
| `src/hal_gpio/` | GPIO HAL family: `dspic33ak_gpio.*` (electrical attributes), `dspic33ak_pps.*` (generic PPS signal routing), and `dspic33ak_gpio_event.*` (CN change-notification events). |
| `src/hal_uart/` | UART HAL core, device table, and optional RX ISR ring backend. |
| `src/hal_spi/` | SPI HAL core. |
| `src/hal_i2c/` | I2C HAL common, master, slave, and device abstraction. |
| `src/hal_i3c/` | I3C HAL foundation: common types, I3C-as-I2C-compatible controller API shape, and device/register adaptation layer. |
| `src/hal_can/` | CAN FD HAL node/device/common code and optional ISR layer. |
| `src/hal_timer/` | Timer1 tick and Timer2 high-resolution timer HAL. |
| `src/hal_dma/` | DMA HAL for low-level channel setup and small IRQ helpers used by higher-level drivers. |
| `src/hal_spi_i2s_tdm/` | SPI framed-mode I2S/TDM transport HAL candidate using DMA ping-pong buffers and a project-supplied config header. |

These folders may contain generic, reusable HAL implementations. The distinction
for `src/hal_gpio/` specifically: `dspic33ak_pps.*` provides generic PPS register
routing (signal-to-RPn mapping, IOLOCK management) and belongs here; board-specific
RP assignments ("which signal goes to which physical pin on this PCB") stay in
`src/board.c` and `src/board_pins.h`. The same split applies to clocking:
`src/hal_clock/` owns reusable PLL / CLKGEN mechanics, while `src/clock/` owns
this starter's selected sources, frequencies, and bring-up order. HAL folders
should not contain board pin maps, application demos, `printf()` retargeting, or
one-off component drivers.

## Starter-specific folders

Code that binds the HALs to this board stays outside the HAL folders:

| Path | Ownership |
|---|---|
| `src/board.c`, `src/board.h`, `src/board_pins.h` | Curiosity board pin names, PPS wiring, and board bring-up entry points. |
| `src/board_components/` | Board-specific component helpers built on HALs or minimal device-level code, such as `led_sw.*`, `rgb_pot.*`, and `sst26_min.*`. |
| `src/console/` | Starter UART integration glue: `printf()` retargeting and application-owned UART1 RX/TX interrupt-vector forwarding to the UART HAL handlers. |
| `src/clock/` | Starter-specific clock policy: FRC 8 MHz -> PLL1 200 MHz, application CLKGEN routing, and CLKGEN10 /10 for 20 MHz CAN FD FCAN. |
| `src/app/` | Bus validation demos and application-level orchestration. |
| `src/hal_udid/` | Local UDID boot-banner helper; not currently a standalone public HAL. |
| `src/dspic33ak_spi_i2s_tdm_conf.h` | Project-supplied TDM HAL configuration, intentionally kept near the top of `src/` for MPLAB X visibility. |

The root-level `board.*` files are intentionally not folded into
`src/board_components/`: they name the board's fixed wiring and PPS routes,
while `board_components/` contains helpers for specific devices attached to that
wiring.

The `board_components` directory name is intentional: these files are
board-specific component helpers, not reusable HAL implementations.

The `hal_i3c` folder is currently a development scaffold.  It is listed in the
project so the API and device/register mapping stay build-visible while the
command queue and timing implementation are filled in.

## MPLAB X project updates

When moving files between folders, update `firmware.X/nbproject/configurations.xml`
for both the source file list and the include directory list. The generated
`firmware.X/nbproject/Makefile-*.mk` files are ignored local artifacts; regenerate
them by opening/building the project in MPLAB X, or with the local makefile
generator if it is known to work in the current environment.

After pulling a change that moves `.c` files or adds a source folder, regenerate
the MPLAB X makefiles before building. Stale generated makefiles commonly fail
with either `No rule to make target ...` for a moved/deleted source or missing
include/undefined-symbol errors for a newly added source folder.
