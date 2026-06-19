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

/* ---- CAN FD vs classic CAN switch ---------------------------------------
 * CAN_LOOPBACK_FD = 1 : CAN FD frame  - FDF set, BRS (fast data phase),
 *                                       payload up to 64 bytes.
 *                 = 0 : classic CAN   - no FDF/BRS, single bit rate, max 8 bytes.
 *
 * The CAN_LB_* settings below are EVERYTHING that differs between the two: the
 * lines marked "FD:" are the CAN FD-specific configuration. Flip CAN_LOOPBACK_FD
 * (or override -DCAN_LOOPBACK_FD=0) to switch the demo frame between the modes. */
#ifndef CAN_LOOPBACK_FD
#define CAN_LOOPBACK_FD   1
#endif

#if CAN_LOOPBACK_FD
#define CAN_LB_CFG_BRS    true    /* FD: allow bit-rate switching in the controller   */
#define CAN_LB_FRAME_FD   true    /* FD: mark the frame as CAN FD (FDF bit)           */
#define CAN_LB_FRAME_BRS  true    /* FD: switch to the data bit rate for the payload  */
#define CAN_LB_LEN        64u     /* FD: payload up to 64 bytes (DLC 15)              */
#else
#define CAN_LB_CFG_BRS    false   /* classic: single bit rate, no BRS                 */
#define CAN_LB_FRAME_FD   false   /* classic: not a CAN FD frame                      */
#define CAN_LB_FRAME_BRS  false
#define CAN_LB_LEN        8u      /* classic: max 8 data bytes                        */
#endif

/* Bring CAN1 up in INTERNAL loopback (no transceiver / wiring / termination).
 * Idempotent enough for our use: re-running just re-applies the config. */
static bool can_loopback_bringup(void)
{
    dspic33ak_canfd_config_t cfg = {0};

    cfg.can_clk_hz   = 20000000u;   /* FCAN from dspic33ak_clock_can_init()   */
    cfg.nominal_bps  = 500000u;
    cfg.data_bps     = 2000000u;
    cfg.sample_pct   = 80u;
    cfg.brs          = CAN_LB_CFG_BRS;   /* FD: BRS allowed; classic: single rate */
    cfg.mode         = DSPIC33AK_CANFD_MODE_INTERNAL_LOOPBACK;  /* swap to *_EXTERNAL_LOOPBACK for bus */
    cfg.timeout_ms   = 10u;
    cfg.get_ms       = systick_ms;
    cfg.msg_ram      = can1_msg_ram;
    cfg.msg_ram_size = (uint16_t)sizeof(can1_msg_ram);

    can_ready = (dspic33ak_canfd_init(DSPIC33AK_CANFD_INST_1, &cfg) == DSPIC33AK_CANFD_OK);
    return can_ready;
}

/* Transmit one 8-byte CAN FD frame (data seeded by `seed`) and receive it back
 * through the internal loopback path. On success `tx`/`rx` hold the sent and
 * received frames. */
static bool can_loopback_roundtrip(uint8_t seed,
                                   dspic33ak_canfd_frame_t *tx,
                                   dspic33ak_canfd_frame_t *rx)
{
    uint8_t i;

    tx->id  = CAN_LOOPBACK_ID;
    tx->fd  = CAN_LB_FRAME_FD;    /* FD: FDF bit; classic: standard frame */
    tx->brs = CAN_LB_FRAME_BRS;  /* FD: fast data phase                  */
    tx->len = CAN_LB_LEN;        /* FD: up to 64; classic: 8             */
    for (i = 0u; i < CAN_LB_LEN; i++) {
        tx->data[i] = (uint8_t)(seed + i);
    }

    if (dspic33ak_canfd_transmit(DSPIC33AK_CANFD_INST_1, tx) != DSPIC33AK_CANFD_OK) {
        return false;
    }
    if (dspic33ak_canfd_receive(DSPIC33AK_CANFD_INST_1, rx) != DSPIC33AK_CANFD_OK) {
        return false;
    }
    if ((rx->id != tx->id) || (rx->len != tx->len)) {
        return false;
    }
    for (i = 0u; i < tx->len; i++) {
        if (rx->data[i] != tx->data[i]) {
            return false;
        }
    }
    return true;
}

/* Print one frame as "<[CAN1 Tx] ..." / ">[CAN1 Rx] ...". */
static void can_log_frame(char dir, const char *tag, const dspic33ak_canfd_frame_t *f)
{
    uint8_t i;
    printf("%c[CAN1 %s] id=0x%03lX len=%u data=", dir, tag,
           (unsigned long)f->id, (unsigned)f->len);
    for (i = 0u; i < f->len; i++) {
        printf("%02X", f->data[i]);
    }
    printf("\n");
}

bool can_loopback_selftest(void)
{
    dspic33ak_canfd_frame_t tx = {0};
    dspic33ak_canfd_frame_t rx = {0};

    if (!can_loopback_bringup()) {
        return false;
    }
    /* One verified round-trip; leaves CAN1 initialized for can_loopback_tick(). */
    return can_loopback_roundtrip(0xA0u, &tx, &rx);
}

void can_loopback_tick(uint32_t beat)
{
    dspic33ak_canfd_frame_t tx = {0};
    dspic33ak_canfd_frame_t rx = {0};

    if (!can_ready) {
        return;
    }
    /* Vary the payload each beat so the log shows live data, like the I2C demo. */
    if (can_loopback_roundtrip((uint8_t)beat, &tx, &rx)) {
        can_log_frame('<', "Tx", &tx);
        can_log_frame('>', "Rx", &rx);
    } else {
        printf(" CAN1 loopback: round-trip FAILED\n");
    }
    printf("\n");   /* blank line so the CAN and I2C blocks stay separated */
}
