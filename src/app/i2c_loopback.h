#ifndef I2C_LOOPBACK_H
#define I2C_LOOPBACK_H

/*
 * i2c_loopback.h
 * --------------
 * I2C master<->slave loopback sample (example code, not a HAL).
 *
 * Configures I2C3 as a small "register file" slave at address 0x55. On each
 * call to i2c_loopback_tick() the given master instance writes a known byte
 * pattern to 0x55 and then reads it back, exercising both master Write and
 * master Read (and so both the slave receive and slave transmit paths). Each
 * direction is logged from both sides. This works on the standard board
 * because MikroBUS A (I2C2) and MikroBUS B (I2C3) sit on one shared I2C bus
 * through the on-board shorting resistors -- no jumper wire required.
 *
 * The three I2C3 interrupt vectors (_I2C3Interrupt / _I2C3RXInterrupt /
 * _I2C3TXInterrupt) are defined in i2c_loopback.c and delegate to the slave
 * HAL, demonstrating how an application wires the slave into its ISRs.
 */

#include <stdint.h>
#include <stdbool.h>

#include "dspic33ak_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bring up the I2C3 slave at 0x55 (call once). Returns true on success. */
bool i2c_loopback_init(void);

/* Run one master Write + master Read round trip against the slave and log all
 * four directions. master_inst is the bus-master to drive
 * (DSPIC33AK_I2C_INST_2 on this board); beat varies the payload so the pattern
 * visibly moves from line to line. Call repeatedly (e.g. per heartbeat once
 * i2c_loopback_init() has succeeded). */
void i2c_loopback_tick(dspic33ak_i2c_instance_t master_inst, uint32_t beat);

#ifdef __cplusplus
}
#endif

#endif /* I2C_LOOPBACK_H */
