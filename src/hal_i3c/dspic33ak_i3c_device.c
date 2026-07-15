#include <xc.h>

#include "dspic33ak_i3c_device.h"

/*
 * Device/instance mapping layer.
 *
 * This is the only file that should directly name I3C1CON / I3C1CNTRL / queue
 * register symbols.  Devices without I3C1 compile with the instance marked not
 * present.
 */

static const dspic33ak_i3c_device_t g_i3c_devices[DSPIC33AK_I3C_INST_COUNT] = {
#if defined(I3C1CON) && defined(I3C1CNTRL) && defined(I3C1CMDQUE)
    [DSPIC33AK_I3C_INST_1] = {
        .present = true,
        .regs = {
            .CON          = &I3C1CON,
            .PG           = &I3C1PG,
            .CNTRL        = &I3C1CNTRL,
            .ADD          = &I3C1ADD,
            .HWCAP        = &I3C1HWCAP,
            .CMDQUE       = &I3C1CMDQUE,
            .RESPQUE      = &I3C1RESPQUE,
            .TXRXDATA     = &I3C1TXRXDATA,
            .RSTCON       = &I3C1RSTCON,
            .INTSTA       = &I3C1INTSTA,
            .INTSTACON    = &I3C1INTSTACON,
            .INTCON       = &I3C1INTCON,
            .QLEVEL       = &I3C1QLEVEL,
            .BUFLEVEL     = &I3C1BUFLEVEL,
            .STATE        = &I3C1STATE,
            .I2CFMTIM     = &I3C1I2CFMTIM,
            .I2CFMPTIM    = &I3C1I2CFMPTIM,
            .BUSTIM       = &I3C1BUSTIM,
            .BUSIDLETIM   = &I3C1BUSIDLETIM,
            .CNTRLTIMOUT  = &I3C1CNTRLTIMOUT,
        },
    },
#else
    [DSPIC33AK_I3C_INST_1] = { .present = false },
#endif
};

const dspic33ak_i3c_device_t *dspic33ak_i3c_get_device(
    dspic33ak_i3c_instance_t inst)
{
    if ((unsigned)inst >= (unsigned)DSPIC33AK_I3C_INST_COUNT) {
        return 0;
    }

    if (!g_i3c_devices[inst].present) {
        return 0;
    }

    return &g_i3c_devices[inst];
}

bool dspic33ak_i3c_instance_is_present(dspic33ak_i3c_instance_t inst)
{
    return (dspic33ak_i3c_get_device(inst) != 0);
}

dspic33ak_i3c_status_t dspic33ak_i3c_set_interrupt_priority(
    dspic33ak_i3c_instance_t inst,
    uint8_t priority)
{
    if ((unsigned)inst >= (unsigned)DSPIC33AK_I3C_INST_COUNT) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if (priority > 7u) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if (dspic33ak_i3c_get_device(inst) == 0) {
        return DSPIC33AK_I3C_ERR_NOT_PRESENT;
    }

    switch (inst) {
    case DSPIC33AK_I3C_INST_1:
    {
        bool ok = false;
#if defined(_I3C1GIP)
        _I3C1GIP = priority;
        ok = true;
#endif
#if defined(_I3C1SEIP)
        _I3C1SEIP = priority;
        ok = true;
#endif
#if defined(_I3C1DEIP)
        _I3C1DEIP = priority;
        ok = true;
#endif
        return ok ? DSPIC33AK_I3C_OK : DSPIC33AK_I3C_ERR_UNSUPPORTED;
    }
    default:
        break;
    }

    return DSPIC33AK_I3C_ERR_UNSUPPORTED;
}
