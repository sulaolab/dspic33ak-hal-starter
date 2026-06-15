/*
 * i2c_loopback.c
 * --------------
 * See i2c_loopback.h. Master (I2C2) writes a known pattern to an I2C3 slave at
 * 0x55 over the shared MikroBUS A/B bus; the slave's callbacks capture what it
 * received and we compare.
 */

#include <xc.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "i2c_loopback.h"
#include "dspic33ak_i2c_master.h"
#include "dspic33ak_i2c_slave.h"
#include "systick.h"

#define LB_SLAVE_INST   DSPIC33AK_I2C_INST_3   /* MikroBUS B */
#define LB_SLAVE_ADDR   0x55u
#define LB_BUF_LEN      16u

/* Captured by the slave callbacks (touched in ISR context -> volatile). */
static volatile uint8_t  g_rx_buf[LB_BUF_LEN];
static volatile uint8_t  g_rx_count;
static volatile bool     g_got_stop;
static volatile bool     g_addressed;

static void slave_on_addr(bool is_read)
{
    (void)is_read;
    g_addressed = true;
}

static void slave_on_rx(uint8_t b)
{
    if (g_rx_count < LB_BUF_LEN) {
        g_rx_buf[g_rx_count] = b;
    }
    g_rx_count++;
}

static void slave_on_stop(void)
{
    g_got_stop = true;
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

void i2c_loopback_run(dspic33ak_i2c_instance_t master_inst)
{
    static const uint8_t pattern[4] = { 0xDEu, 0xADu, 0xBEu, 0xEFu };
    dspic33ak_i2c_slave_config_t scfg = {
        .addr7         = (uint8_t)LB_SLAVE_ADDR,
        .addr_mask     = 0u,
        .clock_stretch = false,
        .on_addr_match = slave_on_addr,
        .on_rx_byte    = slave_on_rx,
        .on_tx_byte    = 0,
        .on_stop       = slave_on_stop,
    };
    dspic33ak_i2c_status_t st;
    uint8_t i;

    printf(" I2C loopback: master (I2C%u) -> slave (I2C3 @ 0x%02X)\n",
           (unsigned)master_inst + 1u, (unsigned)LB_SLAVE_ADDR);

    /* The slave HAL enables the interrupt sources; the application owns the
     * vectors and their priority (0 would leave them masked). */
    _I2C3IP   = 4;
    _I2C3RXIP = 4;
    _I2C3TXIP = 4;

    g_rx_count  = 0u;
    g_got_stop  = false;
    g_addressed = false;

    st = dspic33ak_i2c_slave_init(LB_SLAVE_INST, &scfg);
    if (st != DSPIC33AK_I2C_OK) {
        printf("   slave init failed (%d)\n", (int)st);
        return;
    }

    /* Master writes the known pattern to the slave address. */
    st = dspic33ak_i2c_write(master_inst, (uint8_t)LB_SLAVE_ADDR, pattern,
                             sizeof pattern);

    /* Let the slave's STOP interrupt settle before reading the results. */
    {
        uint32_t t0 = systick_ms();
        while ((uint32_t)(systick_ms() - t0) < 3u) { }
    }

    printf("   master write: %s; slave received %u byte(s):",
           (st == DSPIC33AK_I2C_OK) ? "ACK" : "no-ACK/err",
           (unsigned)g_rx_count);
    for (i = 0u; i < g_rx_count && i < LB_BUF_LEN; i++) {
        printf(" %02X", g_rx_buf[i]);
    }
    printf("\n");

    {
        bool match = g_addressed && g_got_stop &&
                     (g_rx_count == sizeof pattern);
        for (i = 0u; match && i < sizeof pattern; i++) {
            if (g_rx_buf[i] != pattern[i]) {
                match = false;
            }
        }
        printf("   loopback %s\n", match ? "MATCH" : "MISMATCH");
    }

    dspic33ak_i2c_slave_deinit(LB_SLAVE_INST);
}
