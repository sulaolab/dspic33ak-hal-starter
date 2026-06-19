/*
 * can_loopback.c
 * --------------
 * CAN FD internal-loopback self-test (see can_loopback.h).
 */

#include "can_loopback.h"

#include <stdint.h>

#include "dspic33ak_canfd_node.h"
#include "systick.h"

bool can_loopback_selftest(void)
{
    /* Caller-owned CAN message RAM (TX queue + RX FIFO 1), 4-byte aligned. The
     * HAL reports the required size via dspic33ak_canfd_msg_ram_size(). */
    static uint32_t can1_msg_ram[ (576u + 3u) / 4u ] __attribute__((aligned(4)));

    dspic33ak_canfd_config_t cfg = {0};
    dspic33ak_canfd_frame_t  tx  = {0};
    dspic33ak_canfd_frame_t  rx  = {0};
    dspic33ak_canfd_status_t st;
    uint8_t i;

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

    st = dspic33ak_canfd_init(DSPIC33AK_CANFD_INST_1, &cfg);
    if (st != DSPIC33AK_CANFD_OK) {
        return false;
    }

    /* A known CAN FD frame (FDF + BRS, DLC=8). */
    tx.id       = 0x123u;
    tx.extended = false;
    tx.fd       = true;
    tx.brs      = true;
    tx.len      = 8u;
    for (i = 0u; i < 8u; i++) {
        tx.data[i] = (uint8_t)(0xA0u + i);
    }

    st = dspic33ak_canfd_transmit(DSPIC33AK_CANFD_INST_1, &tx);
    if (st != DSPIC33AK_CANFD_OK) {
        return false;
    }

    st = dspic33ak_canfd_receive(DSPIC33AK_CANFD_INST_1, &rx);
    if (st != DSPIC33AK_CANFD_OK) {
        return false;
    }

    if ((rx.id != tx.id) || (rx.len != tx.len)) {
        return false;
    }
    for (i = 0u; i < tx.len; i++) {
        if (rx.data[i] != tx.data[i]) {
            return false;
        }
    }
    return true;
}
