#ifndef NORA_I2C_DSPIC33AK_INTERNAL_H
#define NORA_I2C_DSPIC33AK_INTERNAL_H

#include "nora_i2c.h"
#include "nora_i2c_dspic33ak_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal dsPIC33A helpers shared by the master and slave engines.
 *
 * This header is not part of the NORA I2C public API. It resolves a logical
 * instance to the dsPIC33A register table, computes the backend timing
 * divider, and owns shared role/lifecycle state. Application and board code
 * must include only nora_i2c.h, nora_i2c_master.h, and/or nora_i2c_slave.h.
 */

bool nora_i2c_inst_is_valid(nora_i2c_instance_t inst);

nora_i2c_status_t nora_i2c_get_regs(
    nora_i2c_instance_t inst,
    const nora_i2c_regs_t **regs);

uint32_t nora_i2c_calc_brg(uint32_t fcy_hz, uint32_t bus_hz);

/*
 * Shared per-instance role/lifecycle state. The master and slave engines set
 * this on init/deinit so the public nora_i2c_is_initialized() reports the
 * truth for either role. The engines keep their own state separately and do
 * not reference each other.
 */
typedef enum {
    NORA_I2C_ROLE_NONE = 0,
    NORA_I2C_ROLE_MASTER,
    NORA_I2C_ROLE_SLAVE
} nora_i2c_role_t;

void nora_i2c_set_role(nora_i2c_instance_t inst,
                       nora_i2c_role_t role);
nora_i2c_role_t nora_i2c_get_role(nora_i2c_instance_t inst);

#ifdef __cplusplus
}
#endif

#endif /* NORA_I2C_DSPIC33AK_INTERNAL_H */
