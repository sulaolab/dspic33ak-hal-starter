# dspic33ak-hal-starter

A ready-to-run MPLAB X starter project for the **dsPIC33AK512MPS512**.

Flash this project, open a serial terminal, and you should immediately see the
board bring-up log: clock setup, UART `printf()`, SPI flash verification, I2C
scan, I2C loopback, RGB LED control, and heartbeat output.

<img src="docs/images/serial-console.png" alt="Serial console output from dspic33ak-hal-starter running on dsPIC33AK hardware" width="900">

Captured from a live board session using Tera Term.

## Required hardware

This project targets the following Microchip hardware combination:

* **[EV74H48A](https://www.microchip.com/en-us/development-tool/EV74H48A)** -
  Curiosity Platform Development Board
* **[EV80L65A](https://www.microchip.com/en-us/development-tool/EV80L65A)** -
  dsPIC33AK512MPS512 DIM
* On-board **PKOB4** programmer/debugger
* USB connection for programming and UART console output

No external hardware is required for the basic bring-up sequence.

The SST26 SPI flash, RGB LED, and potentiometer used by the demo are on the
Curiosity motherboard. The I2C scan also runs on a bare bus; if an I2C device is
connected, its ACK address is printed.

For the I2C master/slave loopback demo, I2C2 and I2C3 are used as a shared-bus
loopback on the starter board setup.

This starter is intentionally board- and device-specific (single device, no
`__dsPIC33AK128MC106__` branches) to stay small and obvious.

## What runs after programming?

After programming the board, open a serial terminal at **230400 8N1**.

The firmware demonstrates:

1. **Clock bring-up**  
   FRC -> PLL1, SYSCLK = 200 MHz

2. **UART console output**  
   `printf()` through UART1 at 230400 8N1

3. **SPI flash access**  
   Reads the SST26 JEDEC ID and verifies sector erase/write/read-back

4. **I2C bus scan**  
   Probes 7-bit I2C addresses and prints devices that ACK

5. **I2C loopback**  
   Runs an I2C2 master <-> I2C3 slave round-trip test

6. **GPIO / ADC / PWM demo**  
   LEDs, switches, potentiometer input, RGB LED output, and heartbeat blinking

In short: this is a known-good hardware starter project for checking that the
board, toolchain, programmer, UART console, and basic HAL drivers are working
together.

This pairs with the standalone HALs:
[dspic33ak-gpio-hal](https://github.com/sulaolab/dspic33ak-gpio-hal),
dspic33ak-spi-hal, dspic33ak-i2c-hal, dspic33ak-uart-hal. Those HALs are vendored
into `src/hal/` here so the project builds without any external dependency.

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
 build  : ...
 device : dsPIC33AK512MPS512
 sysclk : 200000000 Hz (FRC -> PLL1)
 uart   : UART1 @ 230400 8N1
==============================================
 LEDs: SW1/SW2 poll LED7/LED6; SW3 CN event drives LED5.
==============================================
 SST26 JEDEC: MFR=0xBF TYPE=0x26 DEV=0x12 (good)
 SST26 sector verify @0x000000: OK
==============================================
 I2C scan (MikroBUS A/B, I2C2): probing 0x08..0x77 ...
   device ACK at 0x1A
   1 device(s) found
==============================================
 I2C loopback: I2C2 master <-> I2C3 slave @0x55 (ready); per beat below.
==============================================
 RGB LED follows the potentiometer; LED0 blinks with the heartbeat.
==============================================
 heartbeat 0
 >I2C2 WR: size=8 ...
 <I2C2 RD: size=8 ...

 heartbeat 1
 ...
```

(The I2C addresses found depend on what is attached. In the screenshot above,
the scan finds a device at `0x1A`; the I2C loopback slave itself runs at
`0x55`. Turning the potentiometer sweeps the RGB LED green -> blue -> white ->
red.)

## Layout

```
firmware.X/             MPLAB X project (single config, dsPIC33AK512MPS512)
src/
  main.c                boot sequence + main loop
  clock/                dspic33ak_clock (PLL1 + CLKGEN routing), systick (1 ms)
  board/                board pin map + per-peripheral pin/PPS wiring
  hal/                  vendored HALs: dspic33ak_gpio / _uart / _spi / _i2c
                        (+ a tiny printf->UART retarget and a minimal SST26 driver)
  hal_gpio/             GPIO CN event validation layer built above dspic33ak_gpio
  app/                  samples: i2c_scan, rgb_pot (ADC + PWM)
docs/
  images/
    serial-console.png   live UART/PRINTF startup log screenshot
  hal_gpio_event_design.md
                         GPIO CN event design notes and CMSIS mapping sketch
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
