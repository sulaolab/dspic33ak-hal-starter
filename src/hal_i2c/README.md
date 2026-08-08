# NORA I2C HAL

## Scope

The NORA I2C HAL exposes a common master/slave API to NORA applications. It
does not choose an I2C controller for an application, define a PCB bus map, or
require AK and CK boards to use matching physical controller numbers.

Application or board code selects a `nora_i2c_instance_t` and passes it to the
HAL. The selected backend resolves that logical instance to its controller
registers. `nora_i2c_is_present()` reports whether the selected backend provides
the requested instance.

## Public API

Include only the role headers required by the caller:

- `nora_i2c.h` — shared instance, status, lifecycle, and interrupt-priority API
- `nora_i2c_master.h` — blocking master transactions, controlled no-STOP
  sequences, low-level primitives, and interrupt helpers
- `nora_i2c_slave.h` — 7-bit callback-driven slave API and ISR delegates

These headers use only NORA types. They do not include compiler device headers,
SFR names, or dsPIC33A register definitions.

## Backend boundary

The current implementation is the dsPIC33A backend:

- `nora_i2c_dspic33a_master.c`
- `nora_i2c_dspic33a_slave.c`
- `nora_i2c_dspic33a_common.c`
- `nora_i2c_dspic33a_device.c`
- `nora_i2c_dspic33a_reg.h`
- `nora_i2c_dspic33a_internal.h`
- `nora_i2c_dspic33a_device.h`

The last two headers and the register header are backend-internal. They are not
application include files. A CK backend may implement the same public API while
using a different controller inventory, register map, and interrupt binding.

## Board and application policy

The application or board owns policy such as which controller reaches Codec A,
Codec B, or an expansion connector. For example, an AK Sonora board may choose
one pair of instances while a CK board chooses another. That selection belongs
outside this HAL and is supplied as a `nora_i2c_instance_t` argument.

The optional CMSIS I2C adapter is separate from this HAL and remains a
dsPIC33AK-specific adapter until it is swept in its own NORA migration.
