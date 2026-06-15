#include "dspic33ak_i2c_slave.h"
#include "dspic33ak_i2c_device.h"
#include "dspic33ak_i2c_reg.h"
#include "dspic33ak_i2c_common.h"

/* --------------------------------------------------------------------------
 * dsPIC33AK I2C slave engine (interrupt-driven, callback-based).
 *
 * The peripheral is the newer FIFO/packet type, but we drive it one byte at a
 * time (CON2.PSZ = 1) so the slave looks like the classic byte-at-a-time slave:
 *   - a received byte (address or data) raises the RX interrupt (STAT1.RBF),
 *   - STOP raises the event interrupt (STAT1.P),
 *   - during a master-read the TX interrupt asks for the next byte.
 * STAT1.D_A distinguishes an address byte (0) from a data byte (1) and
 * STAT1.R_W gives the addressed direction (1 = master reads from us).
 * -------------------------------------------------------------------------- */

static dspic33ak_i2c_slave_config_t g_cfg[DSPIC33AK_I2C_INST_COUNT];
static bool                         g_active[DSPIC33AK_I2C_INST_COUNT];

/* --------------------------------------------------------------------------
 * Init
 * -------------------------------------------------------------------------- */
dspic33ak_i2c_status_t dspic33ak_i2c_slave_init(
    dspic33ak_i2c_instance_t inst,
    const dspic33ak_i2c_slave_config_t *config)
{
    const dspic33ak_i2c_regs_t *r;
    dspic33ak_i2c_status_t st;

    if (config == 0 || config->addr7 > 0x7Fu) {
        return DSPIC33AK_I2C_ERR_INVALID_ARG;
    }

    st = dspic33ak_i2c_get_regs(inst, &r);
    if (st != DSPIC33AK_I2C_OK) {
        return st;
    }

    /* Configure with the module disabled. */
    dspic33ak_i2c_reg_clear(r->CON1, DSPIC33AK_I2C_CON1_ON);

    *r->CON1 = 0u;                 /* 7-bit (A10M=0), no master request bits */
    *r->CON2 = 0x00000001u;        /* PSZ = 1: one byte per packet           */

    if (config->clock_stretch) {
        dspic33ak_i2c_reg_set(r->CON1, DSPIC33AK_I2C_CON1_STREN);
    }

    /* 7-bit own address (right-justified) and address mask. */
    *r->ADD = (uint32_t)config->addr7;
    *r->MSK = (uint32_t)config->addr_mask;

    /* Drop any stale receive-overflow latch. */
    dspic33ak_i2c_reg_clear(r->STAT1, DSPIC33AK_I2C_STAT1_I2COV);

    g_cfg[inst]    = *config;
    g_active[inst] = true;

    /* Event + RX on now; TX is enabled on demand during a master-read so an
     * empty TRN does not storm us with TX interrupts while idle. */
    dspic33ak_i2c_reg_irq_clear(&r->irq_event);
    dspic33ak_i2c_reg_irq_clear(&r->irq_rx);
    dspic33ak_i2c_reg_irq_clear(&r->irq_tx);
    dspic33ak_i2c_reg_irq_enable(&r->irq_event);
    dspic33ak_i2c_reg_irq_enable(&r->irq_rx);

    /* Release SCL and turn the slave on. */
    dspic33ak_i2c_reg_set(r->CON1, DSPIC33AK_I2C_CON1_SCLREL);
    dspic33ak_i2c_reg_set(r->CON1, DSPIC33AK_I2C_CON1_ON);

    return DSPIC33AK_I2C_OK;
}

/* --------------------------------------------------------------------------
 * Deinit
 * -------------------------------------------------------------------------- */
dspic33ak_i2c_status_t dspic33ak_i2c_slave_deinit(dspic33ak_i2c_instance_t inst)
{
    const dspic33ak_i2c_regs_t *r;
    dspic33ak_i2c_status_t st;

    st = dspic33ak_i2c_get_regs(inst, &r);
    if (st != DSPIC33AK_I2C_OK) {
        return st;
    }

    dspic33ak_i2c_reg_irq_disable(&r->irq_event);
    dspic33ak_i2c_reg_irq_disable(&r->irq_rx);
    dspic33ak_i2c_reg_irq_disable(&r->irq_tx);

    dspic33ak_i2c_reg_clear(r->CON1, DSPIC33AK_I2C_CON1_ON);

    g_active[inst] = false;

    return DSPIC33AK_I2C_OK;
}

bool dspic33ak_i2c_slave_is_active(dspic33ak_i2c_instance_t inst)
{
    if (!dspic33ak_i2c_inst_is_valid(inst)) {
        return false;
    }
    return g_active[inst];
}

/* --------------------------------------------------------------------------
 * Helper: release SCL after handling a byte when clock stretching is on.
 * -------------------------------------------------------------------------- */
static void release_clock(dspic33ak_i2c_instance_t inst,
                          const dspic33ak_i2c_regs_t *r)
{
    if (g_cfg[inst].clock_stretch) {
        dspic33ak_i2c_reg_set(r->CON1, DSPIC33AK_I2C_CON1_SCLREL);
    }
}

/* --------------------------------------------------------------------------
 * RX interrupt: a received byte (address or data) is in RCV.
 * -------------------------------------------------------------------------- */
void dspic33ak_i2c_slave_rx_irq(dspic33ak_i2c_instance_t inst)
{
    const dspic33ak_i2c_regs_t *r;

    if (dspic33ak_i2c_get_regs(inst, &r) != DSPIC33AK_I2C_OK) {
        return;
    }

    while (dspic33ak_i2c_reg_is_set(r->STAT1, DSPIC33AK_I2C_STAT1_RBF)) {
        uint32_t stat = *r->STAT1;
        uint8_t  b    = (uint8_t)(*r->RCV & 0xFFu);   /* reading clears RBF */

        if (!g_active[inst]) {
            continue;   /* drain the byte; nothing to do */
        }

        if ((stat & DSPIC33AK_I2C_STAT1_D_A) == 0u) {
            /* Address byte: note the direction for the caller. */
            bool is_read = ((stat & DSPIC33AK_I2C_STAT1_R_W) != 0u);
            if (g_cfg[inst].on_addr_match != 0) {
                g_cfg[inst].on_addr_match(is_read);
            }
            if (is_read) {
                /* Master will clock bytes out of us: start sourcing them. */
                dspic33ak_i2c_reg_irq_clear(&r->irq_tx);
                dspic33ak_i2c_reg_irq_enable(&r->irq_tx);
            }
        } else {
            /* Data byte from a master-write. */
            if (g_cfg[inst].on_rx_byte != 0) {
                g_cfg[inst].on_rx_byte(b);
            }
        }

        release_clock(inst, r);
    }

    dspic33ak_i2c_reg_irq_clear(&r->irq_rx);
}

/* --------------------------------------------------------------------------
 * TX interrupt: the master is reading; provide the next byte.
 * -------------------------------------------------------------------------- */
void dspic33ak_i2c_slave_tx_irq(dspic33ak_i2c_instance_t inst)
{
    const dspic33ak_i2c_regs_t *r;
    uint8_t b = 0xFFu;

    if (dspic33ak_i2c_get_regs(inst, &r) != DSPIC33AK_I2C_OK) {
        return;
    }

    if (!g_active[inst]) {
        dspic33ak_i2c_reg_irq_disable(&r->irq_tx);
        dspic33ak_i2c_reg_irq_clear(&r->irq_tx);
        return;
    }

    if (g_cfg[inst].on_tx_byte != 0) {
        b = g_cfg[inst].on_tx_byte();
    }

    *r->TRN = (uint32_t)b;
    release_clock(inst, r);

    dspic33ak_i2c_reg_irq_clear(&r->irq_tx);
}

/* --------------------------------------------------------------------------
 * Event interrupt: START / STOP / bus events.
 * -------------------------------------------------------------------------- */
void dspic33ak_i2c_slave_event_irq(dspic33ak_i2c_instance_t inst)
{
    const dspic33ak_i2c_regs_t *r;

    if (dspic33ak_i2c_get_regs(inst, &r) != DSPIC33AK_I2C_OK) {
        return;
    }

    if (g_active[inst]) {
        uint32_t stat = *r->STAT1;

        if ((stat & DSPIC33AK_I2C_STAT1_P) != 0u) {
            /* STOP: the transaction is over. Stop sourcing TX bytes. */
            dspic33ak_i2c_reg_irq_disable(&r->irq_tx);
            if (g_cfg[inst].on_stop != 0) {
                g_cfg[inst].on_stop();
            }
        }

        /* Clear a receive-overflow latch so the slave does not wedge. */
        if (dspic33ak_i2c_reg_is_set(r->STAT1, DSPIC33AK_I2C_STAT1_I2COV)) {
            dspic33ak_i2c_reg_clear(r->STAT1, DSPIC33AK_I2C_STAT1_I2COV);
        }
    }

    dspic33ak_i2c_reg_irq_clear(&r->irq_event);
}
