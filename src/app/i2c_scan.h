#ifndef I2C_SCAN_H
#define I2C_SCAN_H

/*
 * i2c_scan.h
 * ----------
 * Tiny I2C bus scanner sample: probes the 7-bit address space and prints which
 * addresses acknowledge. Master-only; works whether or not anything is attached
 * (prints "no devices found" on a bare bus). This is example code, not a HAL.
 */

#include "dspic33ak_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Probe addresses 0x08..0x77 on the given (already-initialized) I2C instance,
 * printing each address that ACKs (a 0-byte write = address + STOP).
 *
 * label is an optional board-supplied name for the bus (e.g. "MikroBUS A").
 * It is printed alongside the I2C instance number; pass NULL to omit it. The
 * mapping of an instance to a connector is board knowledge, so it is provided
 * by the caller rather than hard-coded into this generic sample. */
void i2c_scan_run(dspic33ak_i2c_instance_t inst, const char *label);

#ifdef __cplusplus
}
#endif

#endif /* I2C_SCAN_H */
