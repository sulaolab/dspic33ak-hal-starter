# dspic33ak-hal-starter

## dsPIC33AK HAL Starter with Dual-Partition Firmware Updater

A ready-to-run MPLAB X starter project for the **dsPIC33AK512MPS512**.

Flash this project, open a serial terminal, and you should immediately see the
board bring-up log: clock setup, Timer1-based millisecond timing, Timer2
high-resolution counter self-check, UART
`printf()`, SPI flash verification, I2C scan, and then an alternating
I2C-loopback / CAN FD bus demo with RGB LED control and heartbeat output.

<img src="docs/images/serial-console.png" alt="Serial console from dspic33ak-hal-starter: full startup sequence including HRT self-check, SST26, I2C scan, TDM smoke demo, I2C loopback, and CAN FD with ACK partner" width="900">

Captured from a live session using Tera Term. The output shows the complete
startup sequence: boot banner (including UDID), HRT self-check, SST26 verify,
I2C scan, I2C loopback, CAN1 FD with an ACK partner (`state=active`), and the
TDM8 smoke demo running on MikroBUS-A — all running together. On a single board
with no CAN partner, the CAN1 controller instead goes `error-passive` and
retransmits (a burst you can see on a scope/CAN analyzer).

<img src="docs/images/tdm8-scope-mikrobus-a.png" alt="Oscilloscope capture of MikroBUS-A SPI pins during the TDM8 master smoke demo: BCLK (~12.5 MHz, yellow), FS (~49 kHz, blue), and DataOut carrying the TDM8 slot data (red)" width="900">

Oscilloscope capture of the MikroBUS-A SPI pins with the TDM8 master smoke demo
running: BCLK (~12.5 MHz, yellow), frame sync FS (~49 kHz, blue), and DataOut
carrying the eight-slot TDM8 pattern (red). No codec is required; the demo
exercises framed SPI timing and DMA transfer on the MikroBUS-A pins.

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

The starter application target is intentionally board- and device-specific:
EV74H48A + EV80L65A with a dsPIC33AK512MPS512 DIM. Some vendored HAL candidate
folders also contain dsPIC33AK512 / dsPIC33AK128 silicon facts so the HAL code
can remain reusable, but the starter board path shown here is the AK512 setup.

## What runs after programming?

After programming the board, the two Windows serial ports are:

* **"USB Serial Port"**   — UART1 / MCP2221A, read/write command and XMODEM port
* **"USB Serial Device"** — UART2 / PKOB4, output mirror only

Console output is mirrored to both ports. Connect Tera Term directly to
**USB Serial Port (UART1)** at **230400 8N1, no flow control** for commands and
dual-partition XMODEM updates. UART2 RX is intentionally disabled so an update cannot
be armed on one COM port and accidentally sent to the other.

The firmware demonstrates:

1. **Clock bring-up**
   Generic Clock HAL mechanics plus Starter policy: FRC -> PLL1, SYSCLK =
   200 MHz; CLKGEN1/5/6/8/9 divide-by-1; CLKGEN10 divide-by-10 for 20 MHz CAN
   FD FCAN.

2. **Timer HAL self-check**
   A Timer1-based 1 ms monotonic time base drives heartbeat timing and the
   timeout callbacks used by the I2C and CAN HALs. The application owns the
   `_T1Interrupt()` vector and forwards it to the Timer HAL handler. The
   starter uses the Timer HAL's public default IRQ-priority macro for this tick.
   Timer2 is also initialized as a free-running high-resolution counter and
   checked after the boot banner.

3. **UART console and dual-partition command path**
   `printf()` output at 230400 8N1 is mirrored to both Windows ports. UART1 uses
   an ISR-ring RX backend and owns the beginner-facing `*fua5` / `*fca5`
   commands plus XMODEM-CRC. UART2 is an output-only mirror. An opt-in boot
   self-test validates UART1 async TX/RX, completion callbacks, counts, abort,
   and recovery.

4. **SPI flash access**
   Reads the SST26 JEDEC ID and verifies sector erase/write/read-back

5. **I2C bus scan**
   Probes 7-bit I2C addresses and prints devices that ACK

6. **I2C loopback**
   Runs an I2C2 master <-> I2C3 slave round-trip test

7. **CAN FD on the bus**
   A quick internal-loopback HAL self-check, then CAN1 transmits a CAN FD frame
   on the real bus each beat (NORMAL FD, 500k/2M). A lone board has no ACK
   partner, so it goes error-passive and retransmits — a visible burst on
   CANH/CANL — and the TX queue fills (`queue full / timeout`); connect a CAN
   node/analyzer (or a 2nd board in echo config) to ACK it and see steady CAN
   traffic. A dedicated two-board bus test is also available at build time (see
   `CAN_BUS_TEST` in `main.c`)

8. **GPIO / ADC / PWM demo**
   LEDs, switches, potentiometer input, RGB LED output, and heartbeat blinking

9. **SPI1 TDM8 smoke demo (codec-less, MikroBUS-A)** — default ON
   SPI1 runs as a self-clocked framed-mode (TDM8) master on MikroBUS-A
   (FS=RP70, BCLK=RP75, DataOut=RP101, DataIn=RP106), driving all 8 slots with a
   ~800 Hz-class sine, so a scope on DataOut shows a TDM8 frame (BCLK ~12.5 MHz,
   FS ~48.8 kHz — these are expected design values, not measurements). Jumper
   DataOut->DataIn to read the RX level near `0 dB rel`; with no jumper it floors near
   `-140 dB rel`. The stream runs on DMA/ISR and prints one status line every ~5 s:
   `[TDM1] TDM8 master exp_fs~48.8kHz exp_bclk~12.5MHz block=... miss=0 rx=... dB rel`.
   The public demo path is focused on framed SPI timing, DMA continuity, and
   the observable TDM8 data stream. Demo switches live in `src/app/app_config.h`,
   collected into the `APP_BUILD` variation catalog in
   [`src/app/app_build_config.h`](src/app/app_build_config.h) (select with
   `buildtools/switch_config.ps1`; see [buildtools/README.md](buildtools/README.md)).
   **This demo holds the MikroBUS-A SPI pins** — to use a real SPI Click board there, run
   `.\buildtools\switch_config.ps1 -Preset APP_BUILD_TDM_SMOKE_OFF` (or set
   `HAL_STARTER_ENABLE_TDM_SMOKE_DEMO 0` directly in `src/app/app_config.h`). A start
   failure is reported but does not stop the other demos. (The MikroBUS-A I2C SDA/SCL
   pins are different and are unaffected either way.)

### TDM8 smoke demo on mikroBUS-A

The starter firmware enables the TDM smoke demo by default. When the board is
running, the mikroBUS-A SPI pins output a TDM8-style framed serial waveform.
This is intended as a quick oscilloscope-visible bring-up check and a showcase
that the dsPIC33AK SPI framed-mode path can generate TDM-style audio timing
without a codec attached.

To use mikroBUS-A as a normal SPI Click interface, run
`.\buildtools\switch_config.ps1 -Preset APP_BUILD_TDM_SMOKE_OFF` (or set
`HAL_STARTER_ENABLE_TDM_SMOKE_DEMO` to `0` directly in `src/app/app_config.h`).
That frees the mikroBUS-A SPI pins; the I2C pins on the same mikroBUS header are
separate and remain usable either way.

In short: this is a known-good hardware starter project for checking that the
board, toolchain, programmer, UART console, and basic HAL drivers are working
together.

This pairs with the standalone HALs:

- [dspic33ak-hal-gpio](https://github.com/sulaolab/dspic33ak-hal-gpio)
- [dspic33ak-hal-uart](https://github.com/sulaolab/dspic33ak-hal-uart)
- [dspic33ak-hal-spi](https://github.com/sulaolab/dspic33ak-hal-spi)
- [dspic33ak-hal-i2c](https://github.com/sulaolab/dspic33ak-hal-i2c)
- [dspic33ak-hal-can](https://github.com/sulaolab/dspic33ak-hal-can)
- [dspic33ak-hal-timer](https://github.com/sulaolab/dspic33ak-hal-timer)
- [dspic33ak-hal-dma](https://github.com/sulaolab/dspic33ak-hal-dma)
- [dspic33ak-hal-spi-i2s-tdm](https://github.com/sulaolab/dspic33ak-hal-spi-i2s-tdm)

This starter also currently vendors integration HAL folders such as `hal_clock`,
`hal_dma`, and the `hal_gpio_event` change-notification layer inside
`hal_gpio`. The starter's active HAL surface therefore includes Clock,
GPIO/PPS/CN events, UART, SPI, I2C, CAN FD, Timer, DMA, and SPI framed-mode
I2S/TDM.

Reusable HAL implementations are vendored as validated snapshots under matching
`src/hal_xxx/` directories so the complete project builds without external
source dependencies. Where present, `src/hal_xxx/UPSTREAM.md` records the exact
standalone revision used by the snapshot. Starter-only glue intentionally stays outside those HAL
folders: clock policy lives in `src/clock/`, board pin/PPS wiring lives in
`src/board.*` and `src/board_pins.h`, board component helpers live in
`src/board_components/`, and starter UART glue (`printf()` retargeting plus
application-owned UART1 RX/TX interrupt-vector forwarding) lives in
`src/console/`.

This repository serves as the hardware integration and regression-validation
project for the HAL set: Clock, GPIO, UART, SPI, I2C, CAN FD, and Timer are
exercised together on the target board.

## Toolchain

* MPLAB X IDE (v6.30 or compatible)
* XC-DSC compiler v3.31.01
* DFP: Microchip dsPIC33AK-MP DFP 1.3.185

This repository does **not** include Microchip DFP/header files; install the DFP
through MPLAB X.

## Build & run

### MPLAB X IDE

Use the IDE for editing and building. Hardware programming and debugging from the
IDE sit outside the verified dual-partition provisioning workflow — see the note
below.

1. Open `firmware.X` in MPLAB X (this regenerates the per-machine makefiles).
2. Build (single configuration `dsPIC33AK512`, device dsPIC33AK512MPS512).
3. Open a serial terminal on either Windows port ("USB Serial Port" / "USB
   Serial Device") at **230400 8N1** — output is mirrored to both.

> [!IMPORTANT]
> Do **not** use the IDE's **Program Device** for initial provisioning. The
> project deliberately leaves `useAlternateLoadableFile` off, so Program Device
> writes the raw `firmware.X.production.hex`, which carries the P1 program and P1
> config words but **not** the cloned P2 UCA. The board runs fine immediately,
> but the first `*fca5` commit is then refused (`inactive partition config`)
> because P2's config words were never programmed.
>
> Provision with the verified bundle instead — `buildtools/build.ps1` then
> `buildtools/flashauto.ps1` (see below). The same applies after changing any
> `#pragma config`: a serial update carries program memory only, so the new
> config words reach the device only via a fresh bundle flash.
>
> **Debug sessions have the same caveat.** Starting a debug session programs a
> debug image, not the verified bundle, so the P2 UCA state afterwards is whatever
> the debugger left behind. Debugging the application logic is fine; just repeat
> the verified bundle flash before exercising the dual-partition update path again.

Only `firmware.X/nbproject/{configurations,project}.xml` and the top-level
`firmware.X/Makefile` are tracked; build output and the per-machine generated
makefiles are git-ignored and recreated by MPLAB X.

When a change adds, removes, renames, or moves `.c` files or source folders,
open the project in MPLAB X or otherwise regenerate the per-configuration
makefiles before building. `configurations.xml` is the tracked source of truth;
`nbproject/Makefile-*.mk` files are local generated artifacts.

### Command-line (PowerShell)

The `buildtools/` scripts provide a command-line build and flash workflow without
opening MPLAB X. MPLAB X and XC-DSC must be installed. The scripts auto-detect the MPLAB X
make and project-generator tools; the generated project makefiles invoke XC-DSC.

```powershell
# Choose what the next build targets (interactive menu; see buildtools/README.md)
.\buildtools\switch_config.ps1

# Incremental build (auto-detects MPLAB X version and firmware.X project;
# follows the switch_config.ps1 selection above)
.\buildtools\build.ps1

# Full clean-build: regenerate makefiles, clean outputs, rebuild
.\buildtools\build.ps1 -Full

# Clean outputs only
.\buildtools\build.ps1 -Clean

# Regenerate MPLAB X makefiles only (use after adding/moving source files)
.\buildtools\build.ps1 -Generate

# Flash the verified P1+P2 UCA bundle (auto-detects PKOB4 serial)
.\buildtools\flashauto.ps1

# Reset the board without flashing
.\buildtools\flashauto.ps1 -Reset

# Clean Boost Java state before the reset step, then flash + reset
.\buildtools\flashauto.ps1 -CleanJava

# List connected PKOB4 serials
.\buildtools\flashauto.ps1 -List

# Specify target serial and device explicitly
.\buildtools\flashauto.ps1 -Serial 'YOUR_PKOB4_SERIAL' -Device dsPIC33AK512MPS512
```

`flashauto.ps1` uses the `flash_pkob4.exe` and `reset_pkob4.exe` tools vendored
under `buildtools/_flash_reset_tools`, so a fresh clone can flash and reset
without a separate tool install. To use a different copy, set `FLASH_RESET_TOOLS`
or pass `-ToolsDir`; the legacy root `./_flash_reset_tools` and sibling
`../_flash_reset_tools` locations are also checked as fallbacks. The script
auto-detects the serial number if only one PKOB4 is connected; pass
`-Serial <PKOB4_SERIAL>` when multiple boards are attached.

The build requires Python 3 for the dependency-free Intel HEX tools. Every
successful build automatically creates and validates these artifacts under
`firmware.X/dist/dsPIC33AK512/production/`:

* `firmware.X.production.hex` — compiler output containing P1 program + config
* `firmware.X.production.bundle.hex` — initial PKOB4 image with matching P1/P2 UCA
* `firmware.X.production.bundle.verify_report.txt` — must say `PASS`
* `reflash_image.bin` — partition-independent, DBFW-manifested XMODEM payload

`flashauto.ps1` refuses to flash by default unless the verified bundle and PASS
report are both present and their SHA-256 values match. An explicit advanced
`-Hex` path remains available for diagnostics.

## Dual-partition firmware update

The running application can receive `reflash_image.bin` through UART1 and write
only the inactive 256 KB Flash partition. It validates the package and read-back
of every programmed row before `*fca5` is allowed to change BTSEQ and reset into
the updated partition. The same image filename works in both directions; there is
no separate P1 or P2 update image.

A serial update rewrites **program memory only**: `reflash_image.bin` is a slice
of `[0x800000, 0x840000)` and contains no UCA, FBOOT, or configuration words, so
it can never change a partition's fuses. Those are provisioned once by the
verified bundle over PKOB4.

<img src="docs/images/dual-partition-xmodem-1k-transfer.png" alt="Tera Term on COM12 sending reflash_image.bin to the inactive partition over XMODEM-1K. The console shows *fua5 accepted, the armed instructions including the LED progress note, and the receiver's repeated C handshake characters; the XMODEM Send dialog reports protocol XMODEM (1k), packet 62, 63488 bytes transferred at 12.28 KB/s, 75.6 percent complete" width="640">

A transfer in progress. The console has accepted `*fua5` and is emitting the
XMODEM-CRC handshake character `C` until the sender starts; the dialog confirms
the receiver works with **XMODEM (1k)** as well as the classic 128-byte blocks.
Progress also appears on the board itself, as a bar on LED7..LED0.

Quick serial-update workflow:

1. Connect Tera Term directly to **USB Serial Port (UART1)** at `230400 8N1`,
   no flow control, local echo off.
2. Type `*fua5` and press Enter.
3. Select **File > Transfer > XMODEM > Send** and choose
   `firmware.X/dist/dsPIC33AK512/production/reflash_image.bin`.
4. **1K** may be checked or unchecked; both work.
5. Wait for both `Firmware receive: PASS` and `Validation: PASS`.
6. Type `*fca5`; the device validates UCA and BTSEQ, then resets into the update.

See the **[Dual-Partition Firmware Update Guide](docs/dual_partition_update.md)**
for first-time PKOB4 provisioning, exact Tera Term settings, artifact naming,
verified-bundle selection, expected output, recovery steps, and security scope.

> [!WARNING]
> Use **File > Transfer > XMODEM > Send**, not **File > Send file**.

## Expected serial output

```
==============================================
 dspic33ak-hal-starter
 build  : ...
 device : dsPIC33AK512MPS512
 udid   : ...
 sysclk : 200000000 Hz (FRC -> PLL1)
 uart   : UART1 @ 230400 8N1, RX ISR-ring echo active
 bank   : P1 active, BTSEQ=0xFFF
 config : active UCA OK (ALTI2C2=ON, BOOTSWP=ENABLED)
 update : type *fua5 on UART1, then send reflash_image.bin via XMODEM
==============================================
 HRT: init=0 present=1 initialized=1 clk=100000000 Hz
 HRT: count0=... count1=... count2=... d1=... d10=...
 HRT: conv 100cnt=1 us, 10cnt=1 x0.1us, 100cnt=10 x0.1us
 HRT self-check: PASS
==============================================
 LEDs: SW1/SW2 poll LED7/LED6; SW3 CN event drives LED5.
==============================================
 SST26 JEDEC: MFR=0xBF TYPE=0x26 DEV=0x12 (good)
 SST26 sector verify @0x000000: OK
==============================================
 I2C scan (MikroBUS A/B, I2C2): probing 0x08..0x77 ...
   no devices found
==============================================
 I2C loopback: I2C2 master <-> I2C3 slave @0x55 (ready); per beat below.
==============================================
 CAN1 FD @500k/2M live on the bus (HAL self-check: PASS).
   No ACK partner -> error-passive + retransmit burst; add a CAN node
   (analyzer / 2nd board echo) to ACK it and see steady CAN H/L.
==============================================
 RGB LED follows the potentiometer; LED0 blinks with the heartbeat.
==============================================
 [TDM1] SPI1 TDM8 master smoke demo started (MikroBUS-A; jumper DataOut->DataIn for loopback).

 [TDM1] TDM8 master exp_fs~48.8kHz exp_bclk~12.5MHz block=... miss=0 rx=... dB rel
 <[I2C2 Wr] size=8 1122334455667788
 >[I2C3 Rd] size=8 1122334455667788
 >[I2C2 Rd] size=8 1122334455667788
 <[I2C3 Wr] size=8 1122334455667788

 <[CAN1 Tx] id=0x123 len=64 data=05060708...4344
 [CAN1] state=active TEC=0 REC=0

 <[I2C2 Wr] size=8 ...
 ...
```

The two peripheral demos alternate once per second (CAN FD on one beat, the I2C
master/slave round trip on the next), separated by a blank line. The TDM smoke
demo runs in the background and prints one status line every ~5 s.

(I2C scan results depend on what is attached; the loopback slave itself runs at
`0x55`. Turning the potentiometer sweeps the RGB LED. With no CAN ACK partner
the CAN1 controller is `error-passive` and retransmits — the TX queue fills
(`queue full / timeout`); connect another CAN node to ACK it and it returns to
`state=active`.)

## Layout

```
firmware.X/             MPLAB X Flash Dual Partition project
                        (single config, dsPIC33AK512MPS512)
buildtools/             command-line build, clean, flash, and reset scripts
tools/                  dependency-free HEX provisioning, verification, and
                        reflash-image extraction tools
.vscode/clean.ps1       robust MPLAB X output cleanup helper (used by build.ps1)
src/
  main.c                boot sequence + main loop
  board.c/.h            board bring-up: GPIO electrical config + PPS routing
  board_pins.h          board pin names (RP numbers for PPS pins; packed pin
                        for non-PPS GPIO-only pins)
  clock/                Starter-specific clock policy:
                        FRC -> PLL1 200 MHz, application CLKGENs,
                        and CLKGEN10 /10 for CAN FD FCAN
  board_components/     board-specific component helpers built on HALs
                        or minimal device-level code
                        (LED/SW, RGB/POT, SST26 SPI-NOR)
  console/              UART printf/interrupt glue plus the minimal dual-partition
                        command processor and wrong-file-send guard
  fw_update/            DBFW + XMODEM-CRC receive, inactive-partition programming/
                        read-back, per-partition UCA validation, and BTSEQ commit/reset
  hal_clock/            vendored generic dsPIC33AK Clock HAL:
                        logical PLL / CLKGEN programming through core,
                        device, and register-adaptation layers
  hal_gpio/             vendored GPIO+PPS HAL family:
                        nora_gpio.*    GPIO electrical attributes
                        nora_pps.*     peripheral signal routing (PPS)
                        nora_gpio_event.*  CN change-notification events
  hal_uart/             vendored UART HAL
  hal_spi/              vendored SPI HAL (blocking master; SST26 flash on SPI4)
  hal_i2c/              vendored I2C HAL
  hal_nvm/              dsPIC33AK RTSP Flash erase/program/read primitives
  hal_can/              vendored CAN FD HAL: dspic33ak_canfd_* (node + optional ISR layer)
  hal_timer/            vendored Timer HAL
                        (Timer1 1 ms tick, default IRQ priority macro,
                        and Timer2 high-resolution counter)
  hal_udid/             local UDID helper used by the boot banner
  hal_dma/              vendored DMA HAL (low-level channel config; used by the TDM HAL)
  hal_spi_i2s_tdm/      vendored SPI framed-mode I2S/TDM transport HAL
                        (DMA ping-pong + per-instance block callback; board-free
                        via a port hook). See its own README.md.
  nora_spi_i2s_tdm_conf.h  project-supplied (self-contained) config for the
                        TDM HAL: single SPI1 TDM8 stream, DMA0/1. It is kept
                        near the top of src/ so it is easy to find in MPLAB X.
  app/                  bus validation samples: i2c_scan, i2c_loopback,
                        can_loopback, can_bus_test (two-board); app_config.h
                        (demo toggles); app_build_config.h (APP_BUILD variation
                        catalog, selected via buildtools/switch_config.ps1);
                        tdm_smoke (SPI1 TDM8 smoke demo)
docs/
  dual_partition_update.md complete first-flash and serial-update user guide
  images/
    serial-console.png        live full startup serial-console screenshot
    tdm8-scope-mikrobus-a.png oscilloscope capture of the MikroBUS-A TDM8 smoke demo
  source_layout.md       source-tree ownership and vendored-HAL layout notes
  hal_gpio_event_design.md
                         GPIO CN event usage and current limitations
  hal_udid.md           UDID helper notes
  touch-addon.md        optional capacitive-touch add-on (QTM; not bundled)
```

Design split: **Clock / GPIO / UART / SPI / I2C / CAN FD / Timer are the HALs**.
Validated snapshots are vendored into matching `src/hal_xxx/` folders for
hardware integration and regression testing. Generic clock mechanics live in
`src/hal_clock/`; Starter clock policy lives in `src/clock/`. Board pin/PPS
wiring, board-specific component helpers, console retargeting, and the bus
validation demos remain starter-specific code, kept deliberately small and
hand-written. See `docs/source_layout.md` for the ownership rules.

The standalone Timer HAL is maintained at
[dspic33ak-hal-timer](https://github.com/sulaolab/dspic33ak-hal-timer). The
copy under `src/hal_timer/` is the version integrated and validated by this
starter project.

**GPIO / PPS architecture:** the board layer owns the board-specific pin
assignments (which signal goes to which RP on this PCB). GPIO electrical
configuration (direction, pull, analog, open-drain) is handled by
`nora_gpio.*`; generic PPS signal routing is handled by the companion
`nora_pps.*` layer. Both are in `src/hal_gpio/`. The board layer
(`board.c` / `board_pins.h`) wires them together, using the RP number as the
single identifier for both GPIO config and PPS routing on PPS-capable pins.

## Capacitive touch

The Curiosity board's touch pads are supported via Microchip's QTM library, which
is proprietary and tool-generated, so it is **not** part of this MIT-0 starter.
See [docs/touch-addon.md](docs/touch-addon.md) for how to add it yourself.

## License

MIT No Attribution License (MIT-0). See [LICENSE](LICENSE).

Attribution is appreciated but not required.
