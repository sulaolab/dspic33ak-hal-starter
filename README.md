# dspic33ak-hal-starter

A ready-to-run MPLAB X starter project for the **dsPIC33AK512MPS512**.

Flash this project, open a serial terminal, and you should immediately see the
board bring-up log: clock setup, UART `printf()`, SPI flash verification, I2C
scan, and then an alternating I2C-loopback / CAN FD bus demo with RGB LED control
and heartbeat output.

<img src="docs/images/serial-console.png" alt="Serial console from dspic33ak-hal-starter: a two-board CAN FD session with the I2C loopback and CAN FD frames interleaving" width="900">

Captured from a live **two-board** session using Tera Term. This board (A) runs
the starter; a second board (B) flashed in echo config is wired to it over CAN
(J21&harr;J21, CANH/CANL/GND + termination). So the CAN1 frames are ACKed and
echoed back — the log shows the `<[CAN1 Tx] id=0x123` transmits and the
`>[CAN1 Rx] id=0x0B0` echoes from board B, interleaved with the I2C loopback. On
a single board with no CAN partner, the CAN1 controller instead goes
`error-passive` and retransmits (a burst you can see on a scope/CAN analyzer).

## Required hardware

This project targets the following Microchip hardware - just two parts:

* **[EV74H48A](https://www.microchip.com/en-us/development-tool/EV74H48A)** -
  Curiosity Platform Development Board. It already carries an **on-board PKOB4**
  programmer/debugger and a USB connector, so a single USB cable handles both
  programming and the UART console - no separate programmer or adapter needed.
* **[EV80L65A](https://www.microchip.com/en-us/development-tool/EV80L65A)** -
  dsPIC33AK512MPS512 DIM, which plugs into the Curiosity board.

Plus a USB cable. No external hardware is required for the basic bring-up sequence.

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

6. **CAN FD on the bus**  
   A quick internal-loopback HAL self-check, then CAN1 transmits a CAN FD frame
   on the real bus each beat (NORMAL FD, 500k/2M). A lone board has no ACK
   partner, so it goes error-passive and retransmits — a visible burst on
   CANH/CANL — and the TX queue fills (`queue full / timeout`); connect a CAN
   node/analyzer (or a 2nd board in echo config) to ACK it and see steady CAN
   traffic. A dedicated two-board bus test is also available at build time (see
   `CAN_BUS_TEST` in `main.c`)

7. **GPIO / ADC / PWM demo**  
   LEDs, switches, potentiometer input, RGB LED output, and heartbeat blinking

In short: this is a known-good hardware starter project for checking that the
board, toolchain, programmer, UART console, and basic HAL drivers are working
together.

This pairs with the standalone HALs:
[dspic33ak-gpio-hal](https://github.com/sulaolab/dspic33ak-gpio-hal),
dspic33ak-spi-hal, dspic33ak-i2c-hal, dspic33ak-uart-hal, dspic33ak-can-hal.
Those HALs are vendored into `src/hal/` (and `src/hal_can/`) here so the project
builds without any external dependency.

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
 CAN1 FD @500k/2M live on the bus (HAL self-check: PASS).
   No ACK partner -> error-passive + retransmit burst; add a CAN node
   (analyzer / 2nd board echo) to ACK it and see steady CAN H/L.
==============================================
 RGB LED follows the potentiometer; LED0 blinks with the heartbeat.
==============================================
 <[CAN1 Tx] id=0x123 len=64 data=05060708...4344
 [CAN1] state=error-passive TEC=128 REC=0

 <[I2C2 Wr] size=8 1122334455667788
 >[I2C3 Rd] size=8 1122334455667788
 >[I2C2 Rd] size=8 1122334455667788
 <[I2C3 Wr] size=8 1122334455667788

 <[CAN1 Tx] transmit queue full / timeout
 [CAN1] state=error-passive TEC=128 REC=0
 ...
```

The two peripheral demos alternate once per second (CAN FD on one beat, the I2C
master/slave round trip on the next), separated by a blank line.

(The I2C addresses found depend on what is attached. In the screenshot above,
the scan finds a device at `0x1A`; the I2C loopback slave itself runs at `0x55`.
Turning the potentiometer sweeps the RGB LED green -> blue -> white -> red. With
no CAN ACK partner the CAN1 controller is `error-passive` and retransmits — the
TX queue fills (`queue full / timeout`); connect another CAN node to ACK it and
it returns to `state=active`.)

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
  hal_can/              vendored CAN FD HAL: dspic33ak_canfd_* (node + optional ISR layer)
  app/                  samples: i2c_scan, i2c_loopback, can_loopback,
                        can_bus_test (two-board), rgb_pot (ADC + PWM)
docs/
  images/
    serial-console.png   live two-board CAN FD + I2C session screenshot
  hal_gpio_event_design.md
                         GPIO CN event design notes
  touch-addon.md        optional capacitive-touch add-on (QTM; not bundled)
```

Design split: **GPIO / UART / SPI / I2C / CAN FD are the HALs**; the clock, board pin
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
