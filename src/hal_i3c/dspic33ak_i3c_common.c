#include "dspic33ak_i3c_device.h"

bool dspic33ak_i3c_is_present(dspic33ak_i3c_instance_t inst)
{
    return dspic33ak_i3c_instance_is_present(inst);
}

bool dspic33ak_i3c_is_initialized(dspic33ak_i3c_instance_t inst)
{
    const dspic33ak_i3c_device_t *dev = dspic33ak_i3c_get_device(inst);

    if (dev == 0) {
        return false;
    }

    return dspic33ak_i3c_reg_is_set(dev->regs.CON, DSPIC33AK_I3C_CON_ON);
}

dspic33ak_i3c_status_t dspic33ak_i3c_deinit(dspic33ak_i3c_instance_t inst)
{
    const dspic33ak_i3c_device_t *dev = dspic33ak_i3c_get_device(inst);

    if ((unsigned)inst >= (unsigned)DSPIC33AK_I3C_INST_COUNT) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if (dev == 0) {
        return DSPIC33AK_I3C_ERR_NOT_PRESENT;
    }

    dspic33ak_i3c_reg_write(dev->regs.INTCON, 0u);
    dspic33ak_i3c_reg_write(dev->regs.INTSTACON, 0u);
    dspic33ak_i3c_reg_clear(dev->regs.CNTRL, DSPIC33AK_I3C_CNTRL_ENABLE);
    dspic33ak_i3c_reg_clear(dev->regs.CON, DSPIC33AK_I3C_CON_ON);

    return DSPIC33AK_I3C_OK;
}
