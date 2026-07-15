# dspic33ak256mps306-hal-starter

A local HAL starter/workbench for the **dsPIC33AK256MPS306** DIM on the
Microchip Curiosity Platform board.

This branch was cloned from the AK512 HAL starter, then converted to an
AK256MPS306-specific starter.  The reusable HAL folders remain multi-device
where practical: AK256MPS306 support is added beside the existing AK512/AK128
paths instead of replacing them.

## Current migration status

Build target:

- MPLAB X configuration: `dsPIC33AK256MPS306`
- Device: `dsPIC33AK256MPS306`
- DFP: Microchip `dsPIC33AK-MP_DFP` 1.3.185

Currently enabled by default:

- clock bring-up: FRC -> PLL1, SYSCLK 200 MHz
- Timer1 millisecond tick
- Timer2 high-resolution timer self-check
- dual UART console wiring
  - UART1 -> MCP2221A “USB Serial Port”
  - UART2 -> PKOB4 “USB Serial Device”
- LED/switch GPIO demo using the AK256MPS306 DIM mapping
- SPI1 framed-mode TDM8 pulse-FS smoke demo on MikroBUS-A
- build-visible `src/hal_i3c/` scaffold for the later I3C HAL phase

Disabled by default during the AK256 migration:

- SST26 SPI flash demo: the original path used AK512/SPI4 wiring.
- I2C scan and I2C2/I2C3 loopback: AK256MPS306 DIM routing points the useful
  control bus toward I3C/I2C-compatible work, which is the next phase.
- CAN demo: left build-visible but not run until the AK256 board pin path is
  validated.
- RGB/POT demo: the AK512 demo used ADC5; AK256MPS306 DIM exposes the pot at
  `AD1AN2 / RA3`, so this needs a fresh board component path.

## AK256MPS306 board facts captured here

UART console pins:

| Use | DIM net | MCU pin |
|---|---|---|
| UART1 TX | P96 / UART_USB_RX | `RP37` / `RC4` |
| UART1 RX | P98 / UART_USB_TX | `RP43` / `RC10` |
| UART2 TX | P102 / UART_PKoB_TX | `RP49` / `RD0` |
| UART2 RX | P100 / UART_PKoB_RX | `RP44` / `RC11` |

LEDs and switches:

| Function | MCU pin |
|---|---|
| LED0..LED7 | `RP50..RP57` / `RD1..RD8` |
| SW1 | `RP41` / `RC8` |
| SW2 | `RP30` / `RB13` |
| SW3 | `RP29` / `RB12` |

MikroBUS-A SPI1/TDM smoke-demo pins:

| Signal | DIM pin | MCU pin |
|---|---:|---|
| FS / CS | P81 | `RP33` / `RC0` |
| BCLK / SCK | P83 | `RP42` / `RC9` |
| DataIn / MISO | P85 | `RP39` / `RC6` |
| DataOut / MOSI | P87 | `RP12` / `RA11` |

I3C-capable MikroBUS-B pins for the next phase:

| Signal | DIM pin | MCU pin |
|---|---:|---|
| `I3CASDA1` / SDA | P4 | `RP24` / `RB7` |
| `I3CASCL1` / SCL | P6 | `RP23` / `RB6` |

## Build

PowerShell:

```powershell
.\buildtools\build.ps1 -Full
```

Expected output artifact:

```text
firmware.X\dist\dsPIC33AK256MPS306\production\firmware.X.production.hex
```

Flash, if the connected board and programmer are ready:

```powershell
.\buildtools\flashauto.ps1
```

## I3C HAL note

`src/hal_i3c/` is only a scaffold at this stage.  It has public types, an
I3C-as-I2C-compatible API shape, and a device/register adaptation layer for
`I3C1`, but transfer functions intentionally return
`DSPIC33AK_I3C_ERR_UNSUPPORTED`.

The intended order is:

1. Finish the AK256MPS306 starter migration.
2. Validate the board-level pin map and currently enabled demos.
3. Implement the I3C HAL compatibility path.

## Source layout

Reusable HAL snapshots live in `src/hal_xxx/` folders.  Starter-only board and
demo code lives outside those folders:

- `src/board.c`, `src/board.h`, `src/board_pins.h`: AK256MPS306 board wiring.
- `src/clock/`: starter clock policy.
- `src/console/`: stdio/UART integration glue.
- `src/board_components/`: board-specific LED/switch and disabled demo helpers.
- `src/app/`: optional demo orchestration.
- `src/hal_i3c/`: I3C HAL scaffold for the next phase.
