/*
 * i2c_loopback.c
 * --------------
 * See i2c_loopback.h. The I2C3 slave at 0x55 behaves as a small 8-byte
 * register file: a master Write stores the bytes, a master Read returns them.
 * Each tick the master (I2C2) writes a moving pattern and reads it back over
 * the shared MikroBUS A/B bus, logging both sides of both directions.
 */

#include <xc.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "i2c_loopback.h"
#include "dspic33ak_i2c_master.h"
#include "dspic33ak_i2c_slave.h"
#include "dspic33ak_tick_timer.h"

#define LB_SLAVE_INST   DSPIC33AK_I2C_INST_3   /* MikroBUS B */
#define LB_SLAVE_ADDR   0x55u
#define LB_LEN          8u                     /* payload length per transfer */

/* The slave's register file plus capture of what it received/transmitted.
 * These are touched in ISR context, so they are volatile. */
static volatile uint8_t g_mem[LB_LEN];     /* written by master, read by master */
static volatile uint8_t g_rx_idx;          /* slave receive cursor               */
static volatile uint8_t g_tx_idx;          /* slave transmit cursor              */
static volatile uint8_t g_tx_log[LB_LEN];  /* bytes the slave actually sent      */
static volatile uint8_t g_tx_cnt;

/* Slave callbacks --------------------------------------------------------- */
static void slave_on_addr(bool is_read)
{
    /* Reset the relevant cursor at the start of each addressed transaction. */
    if (is_read) {
        g_tx_idx = 0u;
        g_tx_cnt = 0u;
    } else {
        g_rx_idx = 0u;
    }
}

static void slave_on_rx(uint8_t b)
{
    if (g_rx_idx < LB_LEN) {
        g_mem[g_rx_idx] = b;
    }
    g_rx_idx++;
}

static uint8_t slave_on_tx(void)
{
    uint8_t b = (g_tx_idx < LB_LEN) ? g_mem[g_tx_idx] : 0xFFu;
    if (g_tx_cnt < LB_LEN) {
        g_tx_log[g_tx_cnt] = b;
    }
    g_tx_idx++;
    g_tx_cnt++;
    return b;
}

/* --- I2C3 interrupt vectors, delegated to the slave HAL --------------------
 * This is how an application wires the slave into its ISRs: define the device
 * vectors and forward each to the matching slave handler. */
void __attribute__((__interrupt__, __no_auto_psv__)) _I2C3Interrupt(void)
{
    dspic33ak_i2c_slave_event_irq(LB_SLAVE_INST);
}

void __attribute__((__interrupt__, __no_auto_psv__)) _I2C3RXInterrupt(void)
{
    dspic33ak_i2c_slave_rx_irq(LB_SLAVE_INST);
}

void __attribute__((__interrupt__, __no_auto_psv__)) _I2C3TXInterrupt(void)
{
    dspic33ak_i2c_slave_tx_irq(LB_SLAVE_INST);
}

/* ------------------------------------------------------------------------- */

bool i2c_loopback_init(void)
{
    static const dspic33ak_i2c_slave_config_t scfg = {
        .addr7         = (uint8_t)LB_SLAVE_ADDR,
        .addr_mask     = 0u,
        .clock_stretch = false,
        .on_addr_match = slave_on_addr,
        .on_rx_byte    = slave_on_rx,
        .on_tx_byte    = slave_on_tx,
        .on_stop       = 0,
    };

    /* The slave HAL enables the interrupt sources; the application owns the
     * vectors and asks the HAL to set the matching line priorities. */
    if (dspic33ak_i2c_set_interrupt_priority(LB_SLAVE_INST, 4u) != DSPIC33AK_I2C_OK) {
        return false;
    }

    return (dspic33ak_i2c_slave_init(LB_SLAVE_INST, &scfg) == DSPIC33AK_I2C_OK);
}

static void settle_ms(uint32_t ms)
{
    uint32_t t0 = dspic33ak_tick_timer_get_ms();
    while ((uint32_t)(dspic33ak_tick_timer_get_ms() - t0) < ms) { }
}

static void log_bytes(char dir, unsigned inst_num, const char *op,
                      const volatile uint8_t *buf, unsigned n)
{
    unsigned i;
    printf(" %c[I2C%u %s] size=%u ", dir, inst_num, op, n);
    for (i = 0u; i < n && i < LB_LEN; i++) {
        printf("%02X", buf[i]);
    }
    printf("\n");
}

void i2c_loopback_tick(dspic33ak_i2c_instance_t master_inst, uint32_t beat)
{
    uint8_t  tx[LB_LEN];
    uint8_t  rx[LB_LEN];
    unsigned m = (unsigned)master_inst + 1u;   /* master bus number for the log */
    unsigned s = (unsigned)LB_SLAVE_INST + 1u;  /* slave  bus number (I2C3)      */
    uint8_t  i;

    /* A pattern that shifts by 0x11 each beat, e.g. 1122334455667788. */
    for (i = 0u; i < LB_LEN; i++) {
        tx[i] = (uint8_t)(0x11u * (uint8_t)(i + 1u + (uint8_t)beat));
    }

    /* Arrow convention: '<' = Wr (data leaves the device), '>' = Rd (data
     * enters the device). So a master Write and the slave Read of it carry
     * opposite arrows, as do a master Read and the slave Write that feeds it. */

    /* ---- master Write -> slave receives ---- */
    g_rx_idx = 0u;
    log_bytes('<', m, "Wr", tx, LB_LEN);
    (void)dspic33ak_i2c_write(master_inst, (uint8_t)LB_SLAVE_ADDR, tx, LB_LEN);
    settle_ms(1u);
    log_bytes('>', s, "Rd", g_mem, (g_rx_idx < LB_LEN) ? g_rx_idx : LB_LEN);

    /* ---- master Read <- slave transmits (echoes the stored bytes) ---- */
    for (i = 0u; i < LB_LEN; i++) {
        rx[i] = 0u;
    }
    (void)dspic33ak_i2c_read(master_inst, (uint8_t)LB_SLAVE_ADDR, rx, LB_LEN);
    settle_ms(1u);
    log_bytes('>', m, "Rd", rx, LB_LEN);
    log_bytes('<', s, "Wr", g_tx_log, (g_tx_cnt < LB_LEN) ? g_tx_cnt : LB_LEN);

    printf("\n");   /* blank line between per-beat round trips */
}
