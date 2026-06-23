# Source Layout

This starter keeps reusable HAL code and board-specific integration code in
separate folders. The goal is that each public HAL can be compared with, copied
from, or updated from its standalone repository without rediscovering which
files are part of the HAL and which files are starter glue. The upstream
snapshot commits are recorded in `docs/vendor_manifest.md`.

## Vendored HAL folders

Reusable HAL snapshots live in module-specific `src/hal_xxx/` folders:

| Path | Ownership |
|---|---|
| `src/hal_gpio/` | GPIO HAL core plus the CN event layer used by this starter. |
| `src/hal_uart/` | UART HAL core, device table, and optional RX ISR ring backend. |
| `src/hal_spi/` | SPI HAL core. |
| `src/hal_i2c/` | I2C HAL common, master, slave, and device abstraction. |
| `src/hal_can/` | CAN FD HAL node/device/common code and optional ISR layer. |
| `src/hal_timer/` | Timer1 tick and Timer2 high-resolution timer HAL. |

These folders should not contain board pin maps, PPS routing, application demos,
`printf()` retargeting, or one-off component drivers.

## Starter-specific folders

Code that binds the HALs to this board stays outside the HAL folders:

| Path | Ownership |
|---|---|
| `src/board.c`, `src/board.h`, `src/board_pins.h` | Curiosity board pin names, PPS wiring, and board bring-up entry points. |
| `src/board_components/` | Board component drivers built on top of HALs, such as `sst26_min.*`. |
| `src/console/` | Starter glue that retargets `printf()` to UART1. |
| `src/clock/` | PLL and clock-generator setup for this board/demo. |
| `src/app/` | Validation demos and application-level interrupt vector ownership. |
| `src/hal_udid/` | Local UDID boot-banner helper; not currently a standalone public HAL. |

The root-level `board.*` files are intentionally not folded into
`src/board_components/`: they name the board's fixed wiring and PPS routes,
while `board_components/` contains drivers for specific devices attached to that
wiring.

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
