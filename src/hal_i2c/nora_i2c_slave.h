#ifndef NORA_I2C_SLAVE_H
#define NORA_I2C_SLAVE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "nora_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NORA I2C slave (device/client) interface.
 *
 * Include this header to answer as an I2C slave. The shared instance / status
 * types live in nora_i2c.h (included above); the bus-master role is in
 * nora_i2c_master.h. A program may include either or both.
 *
 * The slave is callback-driven and interrupt-based. Platform startup owns the
 * I2C vector bindings and delegates the event, receive, and transmit sources
 * to the matching nora_i2c_slave_*_irq() handler below.
 *
 * Scope: 7-bit addressing only. 10-bit and general-call are not handled yet.
 */

typedef struct {
    uint8_t addr7;          /* 7-bit own address (right-justified, e.g. 0x55) */
    uint8_t addr_mask;      /* I2CxMSK low 7 bits; 0 = exact match            */
    bool    clock_stretch;  /* STREN: hold SCL low to give callbacks time     */

    /* Address phase: the master addressed us. is_read is true when the master
     * wants to read from us (it will clock bytes out of on_tx_byte), false
     * when it will write to us (bytes arrive at on_rx_byte). May be NULL. */
    void (*on_addr_match)(bool is_read);

    /* Master-write: one received data byte. May be NULL (byte is dropped). */
    void (*on_rx_byte)(uint8_t b);

    /* Master-read: return the next byte to transmit. If NULL, 0xFF is sent. */
    uint8_t (*on_tx_byte)(void);

    /* STOP (or bus-release) ended the transaction. May be NULL. */
    void (*on_stop)(void);
} nora_i2c_slave_config_t;

/* Configure the instance as a slave at config->addr7 and enable it. */
nora_i2c_status_t nora_i2c_slave_init(
    nora_i2c_instance_t inst,
    const nora_i2c_slave_config_t *config);

/* Disable the slave: turn the peripheral off, mask its interrupts, drop state. */
nora_i2c_status_t nora_i2c_slave_deinit(
    nora_i2c_instance_t inst);

/* True once nora_i2c_slave_init() has configured this instance. */
bool nora_i2c_slave_is_active(nora_i2c_instance_t inst);

/* ISR delegates. Bind the platform's I2C event, receive, and transmit vectors
 * to the matching handler below. Each clears the backend flag it handles. */
void nora_i2c_slave_event_irq(nora_i2c_instance_t inst);
void nora_i2c_slave_rx_irq(nora_i2c_instance_t inst);
void nora_i2c_slave_tx_irq(nora_i2c_instance_t inst);

#ifdef __cplusplus
}
#endif

#endif /* NORA_I2C_SLAVE_H */
