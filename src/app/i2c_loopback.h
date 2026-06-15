#ifndef I2C_LOOPBACK_H
#define I2C_LOOPBACK_H

/*
 * i2c_loopback.h
 * --------------
 * I2C master<->slave loopback self-test sample (example code, not a HAL).
 *
 * Configures I2C3 as a slave at address 0x55, has the given master instance
 * write a known byte pattern to 0x55, and prints whether the slave received
 * exactly those bytes. This works on the standard board because MikroBUS A
 * (I2C2) and MikroBUS B (I2C3) sit on one shared I2C bus through the on-board
 * shorting resistors -- no jumper wire required.
 *
 * The three I2C3 interrupt vectors (_I2C3Interrupt / _I2C3RXInterrupt /
 * _I2C3TXInterrupt) are defined in i2c_loopback.c and delegate to the slave
 * HAL, demonstrating how an application wires the slave into its ISRs.
 */

#include "dspic33ak_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Run the loopback test once. master_inst is the bus-master instance to drive
 * (DSPIC33AK_I2C_INST_2 on this board); the slave is always I2C3 @ 0x55. */
void i2c_loopback_run(dspic33ak_i2c_instance_t master_inst);

#ifdef __cplusplus
}
#endif

#endif /* I2C_LOOPBACK_H */
