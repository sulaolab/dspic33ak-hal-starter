/*
 * can_loopback.c
 * --------------
 * CAN FD internal-loopback self-test + repeatable per-beat round-trip
 * (see can_loopback.h).
 */

#include "can_loopback.h"

#include <stdint.h>
#include <stdio.h>

#include "dspic33ak_canfd_node.h"
#include "systick.h"

/* Caller-owned CAN message RAM (TX queue + RX FIFO 1), 4-byte aligned, sized by
 * the HAL's compile-time geometry constant (= dspic33ak_canfd_msg_ram_size()).
 * File scope so the boot self-test and the per-beat tick share one region. */
static uint32_t can1_msg_ram[DSPIC33AK_CANFD_MSG_RAM_WORDS] __attribute__((aligned(4)));
static bool     can_ready = false;

#define CAN_LOOPBACK_ID   0x123u

/* Bring CAN1 up in INTERNAL loopback (no transceiver / wiring / termination).
 * Idempotent enough for our use: re-running just re-applies the config. */
static bool can_loopback_bringup(void)
{
    dspic33ak_canfd_config_t cfg = {0};

    cfg.can_clk_hz   = 20000000u;   /* FCAN from dspic33ak_clock_can_init()   */
    cfg.nominal_bps  = 500000u;
    cfg.data_bps     = 2000000u;
    cfg.sample_pct   = 80u;
    cfg.brs          = true;
    cfg.mode         = DSPIC33AK_CANFD_MODE_INTERNAL_LOOPBACK;  /* swap to *_EXTERNAL_LOOPBACK for bus */
    cfg.timeout_ms   = 10u;
    cfg.get_ms       = systick_ms;
    cfg.msg_ram      = can1_msg_ram;
    cfg.msg_ram_size = (uint16_t)sizeof(can1_msg_ram);

    can_ready = (dspic33ak_canfd_init(DSPIC33AK_CANFD_INST_1, &cfg) == DSPIC33AK_CANFD_OK);
    return can_ready;
}

/* Transmit one 8-byte CAN FD frame (data seeded by `seed`) and receive it back
 * through the internal loopback path. On success `rx` holds the received frame. */
static bool can_loopback_roundtrip(uint8_t seed, dspic33ak_canfd_frame_t *rx)
{
    dspic33ak_canfd_frame_t tx = {0};
    uint8_t i;

    tx.id  = CAN_LOOPBACK_ID;
    tx.fd  = true;
    tx.brs = true;
    tx.len = 8u;
    for (i = 0u; i < 8u; i++) {
        tx.data[i] = (uint8_t)(seed + i);
    }

    if (dspic33ak_canfd_transmit(DSPIC33AK_CANFD_INST_1, &tx) != DSPIC33AK_CANFD_OK) {
        return false;
    }
    if (dspic33ak_canfd_receive(DSPIC33AK_CANFD_INST_1, rx) != DSPIC33AK_CANFD_OK) {
        return false;
    }
    if ((rx->id != tx.id) || (rx->len != tx.len)) {
        return false;
    }
    for (i = 0u; i < tx.len; i++) {
        if (rx->data[i] != tx.data[i]) {
            return false;
        }
    }
    return true;
}

bool can_loopback_selftest(void)
{
    dspic33ak_canfd_frame_t rx = {0};

    if (!can_loopback_bringup()) {
        return false;
    }
    /* One verified round-trip; leaves CAN1 initialized for can_loopback_tick(). */
    return can_loopback_roundtrip(0xA0u, &rx);
}

void can_loopback_tick(uint32_t beat)
{
    dspic33ak_canfd_frame_t rx = {0};
    bool ok;

    if (!can_ready) {
        return;
    }
    /* Vary the payload each beat so the log shows live data, like the I2C demo. */
    ok = can_loopback_roundtrip((uint8_t)beat, &rx);
    if (ok) {
        printf(" CAN1 loopback: id=0x%03lX len=%u data=%02X%02X%02X%02X%02X%02X%02X%02X\n",
               (unsigned long)rx.id, (unsigned)rx.len,
               rx.data[0], rx.data[1], rx.data[2], rx.data[3],
               rx.data[4], rx.data[5], rx.data[6], rx.data[7]);
    } else {
        printf(" CAN1 loopback: round-trip FAILED\n");
    }
}
