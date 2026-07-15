#ifndef DSPIC33AK_I3C_MASTER_H
#define DSPIC33AK_I3C_MASTER_H

#include <stddef.h>
#include <stdint.h>

#include "dspic33ak_i3c.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * I3C controller API for legacy I2C-compatible targets.
 *
 * This intentionally uses I3C naming even when the bus transaction is legacy
 * I2C-shaped.  The peripheral setup, timing registers, command queue, response
 * queue, and error model are I3C-specific.
 */

typedef struct {
    uint32_t peripheral_clk_hz;
    uint32_t bus_hz;
    uint32_t timeout_ms;
    dspic33ak_i3c_get_ms_fn get_ms;
    dspic33ak_i3c_pin_route_t pin_route;
    bool enable_internal_pullups;
} dspic33ak_i3c_i2c_compat_config_t;

/*
 * Prepare I3C1 as an I2C-compatible controller.
 *
 * Scaffold note: this API is present now so users and demos can be written
 * against the intended shape, but the implementation returns
 * DSPIC33AK_I3C_ERR_UNSUPPORTED until timing and queue programming are added.
 */
dspic33ak_i3c_status_t dspic33ak_i3c_i2c_compat_init(
    dspic33ak_i3c_instance_t inst,
    const dspic33ak_i3c_i2c_compat_config_t *config);

dspic33ak_i3c_status_t dspic33ak_i3c_i2c_write(
    dspic33ak_i3c_instance_t inst,
    uint8_t addr7,
    const uint8_t *tx,
    size_t tx_len);

dspic33ak_i3c_status_t dspic33ak_i3c_i2c_read(
    dspic33ak_i3c_instance_t inst,
    uint8_t addr7,
    uint8_t *rx,
    size_t rx_len);

dspic33ak_i3c_status_t dspic33ak_i3c_i2c_write_read(
    dspic33ak_i3c_instance_t inst,
    uint8_t addr7,
    const uint8_t *tx,
    size_t tx_len,
    uint8_t *rx,
    size_t rx_len);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_I3C_MASTER_H */
