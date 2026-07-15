#include "dspic33ak_i3c_master.h"
#include "dspic33ak_i3c_device.h"

static bool dspic33ak_i3c_addr7_valid(uint8_t addr7)
{
    return (addr7 > 0u) && (addr7 < 0x78u);
}

dspic33ak_i3c_status_t dspic33ak_i3c_i2c_compat_init(
    dspic33ak_i3c_instance_t inst,
    const dspic33ak_i3c_i2c_compat_config_t *config)
{
    const dspic33ak_i3c_device_t *dev;

    if ((unsigned)inst >= (unsigned)DSPIC33AK_I3C_INST_COUNT) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if (config == 0) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if ((config->peripheral_clk_hz == 0u) || (config->bus_hz == 0u)) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if ((config->pin_route != DSPIC33AK_I3C_PIN_ROUTE_PRIMARY) &&
        (config->pin_route != DSPIC33AK_I3C_PIN_ROUTE_ALTERNATE)) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }

    dev = dspic33ak_i3c_get_device(inst);
    if (dev == 0) {
        return DSPIC33AK_I3C_ERR_NOT_PRESENT;
    }

    /*
     * Foundation only:
     *
     * The DFP register map is wired up, but a correct init still needs family
     * data sheet work for I2C-compatible timing registers and command/response
     * queue sequencing.  Do not partially enable the bus until that logic is
     * implemented and measured.
     */
    (void)dev;

    return DSPIC33AK_I3C_ERR_UNSUPPORTED;
}

dspic33ak_i3c_status_t dspic33ak_i3c_i2c_write(
    dspic33ak_i3c_instance_t inst,
    uint8_t addr7,
    const uint8_t *tx,
    size_t tx_len)
{
    if ((unsigned)inst >= (unsigned)DSPIC33AK_I3C_INST_COUNT) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if (!dspic33ak_i3c_addr7_valid(addr7)) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if ((tx == 0) && (tx_len != 0u)) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if (dspic33ak_i3c_get_device(inst) == 0) {
        return DSPIC33AK_I3C_ERR_NOT_PRESENT;
    }
    if (!dspic33ak_i3c_is_initialized(inst)) {
        return DSPIC33AK_I3C_ERR_NOT_INITIALIZED;
    }

    return DSPIC33AK_I3C_ERR_UNSUPPORTED;
}

dspic33ak_i3c_status_t dspic33ak_i3c_i2c_read(
    dspic33ak_i3c_instance_t inst,
    uint8_t addr7,
    uint8_t *rx,
    size_t rx_len)
{
    if ((unsigned)inst >= (unsigned)DSPIC33AK_I3C_INST_COUNT) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if (!dspic33ak_i3c_addr7_valid(addr7)) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if ((rx == 0) || (rx_len == 0u)) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if (dspic33ak_i3c_get_device(inst) == 0) {
        return DSPIC33AK_I3C_ERR_NOT_PRESENT;
    }
    if (!dspic33ak_i3c_is_initialized(inst)) {
        return DSPIC33AK_I3C_ERR_NOT_INITIALIZED;
    }

    return DSPIC33AK_I3C_ERR_UNSUPPORTED;
}

dspic33ak_i3c_status_t dspic33ak_i3c_i2c_write_read(
    dspic33ak_i3c_instance_t inst,
    uint8_t addr7,
    const uint8_t *tx,
    size_t tx_len,
    uint8_t *rx,
    size_t rx_len)
{
    if ((tx == 0) && (tx_len != 0u)) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if ((rx == 0) || (rx_len == 0u)) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if ((unsigned)inst >= (unsigned)DSPIC33AK_I3C_INST_COUNT) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if (!dspic33ak_i3c_addr7_valid(addr7)) {
        return DSPIC33AK_I3C_ERR_INVALID_ARG;
    }
    if (dspic33ak_i3c_get_device(inst) == 0) {
        return DSPIC33AK_I3C_ERR_NOT_PRESENT;
    }
    if (!dspic33ak_i3c_is_initialized(inst)) {
        return DSPIC33AK_I3C_ERR_NOT_INITIALIZED;
    }

    return DSPIC33AK_I3C_ERR_UNSUPPORTED;
}
