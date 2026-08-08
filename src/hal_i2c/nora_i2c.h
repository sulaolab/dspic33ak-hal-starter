#ifndef NORA_I2C_H
#define NORA_I2C_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NORA I2C public types and lifecycle API.
 *
 * This header carries only what the master and slave roles share:
 *   - the instance and status enumerations,
 *   - the millisecond-tick callback type,
 *   - presence / initialized queries and deinit.
 *
 * For the bus-master API include nora_i2c_master.h; for the slave API
 * include nora_i2c_slave.h. A program may include either or both. Both
 * pull in this header, so the shared types are always available.
 *
 * Application and board code choose the instance passed to this API. The
 * enum identifies a logical controller slot; it does not promise that an AK
 * and a CK board use the same physical peripheral for a given slot.
 *
 * This header intentionally does not expose XC-DSC/DFP bitfield types.
 */

typedef enum {
    NORA_I2C_INST_1 = 0,
    NORA_I2C_INST_2,
    NORA_I2C_INST_3,
    NORA_I2C_INST_4,
    NORA_I2C_INST_COUNT
} nora_i2c_instance_t;

typedef enum {
    NORA_I2C_OK = 0,
    NORA_I2C_ERR_INVALID_ARG,
    NORA_I2C_ERR_NOT_PRESENT,
    NORA_I2C_ERR_NOT_INITIALIZED,
    NORA_I2C_ERR_BUSY,
    NORA_I2C_ERR_TIMEOUT,
    NORA_I2C_ERR_NACK,
    NORA_I2C_ERR_BUS,
    NORA_I2C_ERR_COLLISION,
    NORA_I2C_ERR_UNSUPPORTED,
    NORA_I2C_ERR_SEQUENCE
} nora_i2c_status_t;

typedef uint32_t (*nora_i2c_get_ms_fn)(void);

/* Shared lifecycle / query API -------------------------------------------- */

/*
 * Deinitialize the selected I2C instance (master or slave).
 *
 * If deinit recovers a stale pending transaction, it may return the recovery
 * status while still forcing the peripheral off and clearing HAL state.
 */
nora_i2c_status_t nora_i2c_deinit(
    nora_i2c_instance_t inst);

bool nora_i2c_is_present(
    nora_i2c_instance_t inst);

/*
 * Set the CPU interrupt priority for the selected I2C instance's event line,
 * and any dedicated RX/TX lines supported by the selected backend. Platform
 * startup still owns the interrupt-vector bindings.
 */
nora_i2c_status_t nora_i2c_set_interrupt_priority(
    nora_i2c_instance_t inst,
    uint8_t priority);

bool nora_i2c_is_initialized(
    nora_i2c_instance_t inst);

#ifdef __cplusplus
}
#endif

#endif /* NORA_I2C_H */
