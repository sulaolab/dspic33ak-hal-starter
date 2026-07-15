#ifndef DSPIC33AK_I3C_DEVICE_H
#define DSPIC33AK_I3C_DEVICE_H

#include <stdbool.h>

#include "dspic33ak_i3c.h"
#include "dspic33ak_i3c_reg.h"

typedef struct {
    bool present;
    dspic33ak_i3c_regs_t regs;
} dspic33ak_i3c_device_t;

const dspic33ak_i3c_device_t *dspic33ak_i3c_get_device(
    dspic33ak_i3c_instance_t inst);

bool dspic33ak_i3c_instance_is_present(
    dspic33ak_i3c_instance_t inst);

#endif /* DSPIC33AK_I3C_DEVICE_H */
