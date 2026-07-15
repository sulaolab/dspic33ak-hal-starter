# dsPIC33AK I3C HAL foundation

This folder is the initial development scaffold for a reusable dsPIC33AK I3C
HAL.  It intentionally sits beside, rather than inside, `hal_i2c` because the
I3C peripheral is a different block with command/response queues, target tables,
and timing registers.

## Current scope

The first useful target is **I3C used as an I2C-compatible controller** for
simple 7-bit legacy I2C targets.

For dsPIC33AK256MPS306 DIM bring-up, the important board fact is:

| Signal | DIM / header use | MCU pin |
|---|---|---|
| `I3CASDA1` | MikroBUS-B SDA | `RP24` / `RB7` |
| `I3CASCL1` | MikroBUS-B SCL | `RP23` / `RB6` |

The primary `I3CSDA1` / `I3CSCL1` pair is routed to other board nets on that
DIM, so the alternate I3C route is the practical MikroBUS path.

## I2C compatibility policy

The I3C peripheral can share a bus with many legacy I2C target devices, but this
is not a perfect drop-in replacement for every I2C use case.  The HAL API names
the compatibility path explicitly so callers do not accidentally assume full I2C
feature coverage.

Initial compatibility assumptions:

- 7-bit legacy I2C target addresses only.
- No target clock stretching.
- No 10-bit addressing.
- No I2C high-speed mode.
- Start with Fast-mode / Fast-mode Plus class rates before attempting I3C SDR.

## Scaffold status

The current files provide:

- public status/instance types,
- an I3C-as-I2C compatibility API shape,
- a device/register adaptation layer for `I3C1`,
- safe presence/deinit/query helpers,
- MPLAB X project integration.

The transfer functions deliberately return `DSPIC33AK_I3C_ERR_UNSUPPORTED`
until the command queue, response queue, timing programming, and error mapping
are implemented from the family data sheet.  This is a buildable landing pad,
not a pretend-complete driver.

## Implementation path

Recommended next steps:

1. Confirm the peripheral clock source used by I3C on the active dsPIC33AK
   device and add timing calculation for `I3C1I2CFMTIM` / `I3C1I2CFMPTIM`.
2. Implement a single private-direct I2C-compatible write transaction through
   `I3C1CMDQUE`, `I3C1TXRXDATA`, and `I3C1RESPQUE`.
3. Add read and write-read/repeated-start style helpers.
4. Add an optional scan/demo app that can be enabled from `app_config.h`.
5. Only after the compatibility path is stable, add native I3C dynamic-address
   assignment and CCC helpers.
