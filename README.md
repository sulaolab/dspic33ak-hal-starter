# dspic33ak-hal-starter

A minimal, readable MPLAB X starter project for the **dsPIC33AK** HAL family.

It boots a board, brings up the clock, and exercises the published HALs end to
end so you have a known-good starting point to build on:

```
clock (FRC -> PLL1 200 MHz)
  -> UART  : 230400 8N1 console (printf)
  -> SPI   : SST26 external flash JEDEC ID + sector erase/write/read-back verify
  -> I2C   : bus scan (prints the addresses that ACK)
  -> ADC+PWM: potentiometer -> RGB LED color, plus a 1 Hz heartbeat
```

This pairs with the standalone HALs:
[dspic33ak-gpio-hal](https://github.com/sulaolab/dspic33ak-gpio-hal),
dspic33ak-spi-hal, dspic33ak-i2c-hal, dspic33ak-uart-hal. Those HALs are vendored
into `src/hal/` here so the project builds without any external dependency.

## Target hardware

* **Board:** Microchip Curiosity Platform Development Board + a
  **dsPIC33AK512MPS512** DIM, in its standard (unmodified) configuration.
* **Programmer/debugger:** on-board PKOB4.
* The SST26 SPI flash, the RGB LED, and the potentiometer are on the Curiosity
  motherboard. The I2C scan needs no specific device — it simply lists whatever
  acknowledges (and completes, printing "no devices found", on a bare bus).

This starter is intentionally board- and device-specific (single device, no
`__dsPIC33AK128MC106__` branches) to stay small and obvious.

## Toolchain

* MPLAB X IDE (v6.30 or compatible)
* XC-DSC compiler v3.31.01
* DFP: Microchip dsPIC33AK-MP DFP 1.3.185

This repository does **not** include Microchip DFP/header files; install the DFP
through MPLAB X.

## Build & run

1. Open `firmware.X` in MPLAB X (this regenerates the per-machine makefiles).
2. Build (single configuration `dsPIC33AK512`, device dsPIC33AK512MPS512).
3. Program to the board with the on-board PKOB4.
4. Open a serial terminal on the board's USB-serial port at **230400 8N1**.

Only `firmware.X/nbproject/{configurations,project}.xml` and the top-level
`firmware.X/Makefile` are tracked; build output and the per-machine generated
makefiles are git-ignored and recreated by MPLAB X.

## Expected serial output

```
==============================================
 dspic33ak-hal-starter
 device : dsPIC33AK512MPS512
 sysclk : 200000000 Hz (FRC -> PLL1)
 uart   : UART1 @ 230400 8N1
==============================================
 SST26 JEDEC: MFR=0xBF TYPE=0x26 DEV=0x12 (good)
 SST26 sector verify @0x000000: OK
==============================================
 I2C scan (I2C2): probing 0x08..0x77 ...
   device ACK at 0x1A
   1 device(s) found
==============================================
 RGB LED follows the potentiometer; heartbeat below.
==============================================
 heartbeat 0
 heartbeat 1
 ...
```

(The I2C addresses found depend on what is attached; turning the potentiometer
sweeps the RGB LED green -> blue -> white -> red.)

## Layout

```
firmware.X/             MPLAB X project (single config, dsPIC33AK512MPS512)
src/
  main.c                boot sequence + main loop
  clock/                dspic33ak_clock (PLL1 + CLKGEN routing), systick (1 ms)
  board/                board pin map + per-peripheral pin/PPS wiring
  hal/                  vendored HALs: dspic33ak_gpio / _uart / _spi / _i2c
                        (+ a tiny printf->UART retarget and a minimal SST26 driver)
  app/                  samples: i2c_scan, rgb_pot (ADC + PWM)
docs/
  touch-addon.md        optional capacitive-touch add-on (QTM; not bundled)
```

Design split: **GPIO / UART / SPI / I2C are the HALs**; the clock, board pin
wiring, and the ADC/PWM demo are starter-specific code, kept deliberately small
and hand-written. PPS routing lives in the board layer; the HALs never touch
pins.

## Capacitive touch

The Curiosity board's touch pads are supported via Microchip's QTM library, which
is proprietary and tool-generated, so it is **not** part of this MIT-0 starter.
See [docs/touch-addon.md](docs/touch-addon.md) for how to add it yourself.

## License

MIT No Attribution License (MIT-0). See [LICENSE](LICENSE).

Attribution is appreciated but not required.
