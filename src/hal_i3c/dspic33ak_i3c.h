#ifndef DSPIC33AK_I3C_H
#define DSPIC33AK_I3C_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * dsPIC33AK I3C HAL - common public types and lifecycle.
 *
 * The I3C peripheral is not register-compatible with the older dsPIC33AK I2C
 * block.  Keep this HAL separate from hal_i2c even when it is used to talk to
 * legacy I2C targets in compatibility mode.
 */

typedef enum {
    DSPIC33AK_I3C_INST_1 = 0,
    DSPIC33AK_I3C_INST_COUNT
} dspic33ak_i3c_instance_t;

typedef enum {
    DSPIC33AK_I3C_OK = 0,
    DSPIC33AK_I3C_ERR_INVALID_ARG,
    DSPIC33AK_I3C_ERR_NOT_PRESENT,
    DSPIC33AK_I3C_ERR_NOT_INITIALIZED,
    DSPIC33AK_I3C_ERR_BUSY,
    DSPIC33AK_I3C_ERR_TIMEOUT,
    DSPIC33AK_I3C_ERR_NACK,
    DSPIC33AK_I3C_ERR_BUS,
    DSPIC33AK_I3C_ERR_UNSUPPORTED,
    DSPIC33AK_I3C_ERR_SEQUENCE
} dspic33ak_i3c_status_t;

typedef uint32_t (*dspic33ak_i3c_get_ms_fn)(void);

typedef enum {
    DSPIC33AK_I3C_PIN_ROUTE_PRIMARY = 0,
    DSPIC33AK_I3C_PIN_ROUTE_ALTERNATE
} dspic33ak_i3c_pin_route_t;

bool dspic33ak_i3c_is_present(
    dspic33ak_i3c_instance_t inst);

bool dspic33ak_i3c_is_initialized(
    dspic33ak_i3c_instance_t inst);

dspic33ak_i3c_status_t dspic33ak_i3c_deinit(
    dspic33ak_i3c_instance_t inst);

/*
 * Set the CPU interrupt priority symbols available for the selected I3C
 * instance.  The application still owns the actual interrupt vectors.
 */
dspic33ak_i3c_status_t dspic33ak_i3c_set_interrupt_priority(
    dspic33ak_i3c_instance_t inst,
    uint8_t priority);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_I3C_H */
