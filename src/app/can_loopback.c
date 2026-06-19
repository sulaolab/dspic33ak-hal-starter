/*
 * can_loopback.c
 * --------------
 * CAN1 demo (see can_loopback.h). Two parts:
 *   1) boot self-test  - one INTERNAL-loopback round trip (self-ACK, no bus or
 *      partner needed) to prove the CAN FD engine works on a bare board.
 *   2) per-beat tick   - transmits on the REAL bus in NORMAL_FD mode. With an
 *      ACK partner the frame goes out once; with NO partner there is no ACK, so
 *      the controller goes error-passive (TEC=128) and keeps retransmitting - a
 *      continuous burst on CANH/CANL - and the TX queue fills, so further sends
 *      report "queue full / timeout". (Per the CAN spec a lone node parks at
 *      error-passive and does NOT bus-off on ACK-only errors.) So a single board
 *      still emits a real CAN signal, and "no partner" is obvious on a scope and
 *      in the log (state=error-passive, queue timeouts).
 */

#include "can_loopback.h"

#include <stdint.h>
#include <stdio.h>

#include "dspic33ak_canfd_node.h"
#include "dspic33ak_canfd_isr.h"   /* dspic33ak_canfd_get_status() */
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

/* Bring CAN1 up in `mode` (the board layer has already done clock/PPS/power). */
static bool can_bringup(dspic33ak_canfd_mode_t mode)
{
    dspic33ak_canfd_config_t cfg = {0};

    cfg.can_clk_hz   = 20000000u;   /* FCAN from dspic33ak_clock_can_init()   */
    cfg.nominal_bps  = 500000u;
    cfg.data_bps     = 2000000u;
    cfg.sample_pct   = 80u;
    cfg.brs          = CAN_LB_CFG_BRS;   /* FD: BRS allowed; classic: single rate */
    cfg.mode         = mode;
    cfg.timeout_ms   = 10u;
    cfg.get_ms       = systick_ms;
    cfg.msg_ram      = can1_msg_ram;
    cfg.msg_ram_size = (uint16_t)sizeof(can1_msg_ram);

    can_ready = (dspic33ak_canfd_init(DSPIC33AK_CANFD_INST_1, &cfg) == DSPIC33AK_CANFD_OK);
    return can_ready;
}

/* Fill an 8/64-byte demo frame, payload seeded by `seed`. */
static void build_frame(uint8_t seed, dspic33ak_canfd_frame_t *tx)
{
    uint8_t i;
    tx->id  = CAN_LOOPBACK_ID;
    tx->fd  = CAN_LB_FRAME_FD;    /* FD: FDF bit; classic: standard frame */
    tx->brs = CAN_LB_FRAME_BRS;  /* FD: fast data phase                  */
    tx->len = CAN_LB_LEN;        /* FD: up to 64; classic: 8             */
    for (i = 0u; i < CAN_LB_LEN; i++) {
        tx->data[i] = (uint8_t)(seed + i);
    }
}

/* Print one frame as " <[CAN1 Tx] ..." / " >[CAN1 Rx] ..." (leading space to
 * align with the I2C demo lines). */
static void can_log_frame(char dir, const char *tag, const dspic33ak_canfd_frame_t *f)
{
    uint8_t i;
    printf(" %c[CAN1 %s] id=0x%03lX len=%u data=", dir, tag,
           (unsigned long)f->id, (unsigned)f->len);
    for (i = 0u; i < f->len; i++) {
        printf("%02X", f->data[i]);
    }
    printf("\n");
}

static const char *bus_state_str(const dspic33ak_canfd_bus_status_t *s)
{
    if (s->bus_off)       return "bus-off";
    if (s->error_passive) return "error-passive";
    if (s->error_warning) return "warning";
    return "active";
}

bool can_loopback_selftest(void)
{
    dspic33ak_canfd_frame_t tx = {0};
    dspic33ak_canfd_frame_t rx = {0};
    bool ok = false;
    uint8_t i;

    /* Self-contained HAL check: INTERNAL loopback self-ACKs and self-receives, so
     * it passes on a bare board with no bus, transceiver or partner. */
    if (!can_bringup(DSPIC33AK_CANFD_MODE_INTERNAL_LOOPBACK)) {
        return false;
    }
    build_frame(0xA0u, &tx);
    if ((dspic33ak_canfd_transmit(DSPIC33AK_CANFD_INST_1, &tx) == DSPIC33AK_CANFD_OK) &&
        (dspic33ak_canfd_receive(DSPIC33AK_CANFD_INST_1, &rx) == DSPIC33AK_CANFD_OK) &&
        (rx.id == tx.id) && (rx.len == tx.len)) {
        ok = true;
        for (i = 0u; i < tx.len; i++) {
            if (rx.data[i] != tx.data[i]) { ok = false; break; }
        }
    }

    /* Arm the live demo on the REAL bus. NORMAL_FD needs another node to ACK:
     * alone, it goes error-passive and retransmits (a visible burst); see the tick. */
    (void)can_bringup(DSPIC33AK_CANFD_MODE_NORMAL_FD);
    return ok;
}

void can_loopback_tick(uint32_t beat)
{
    dspic33ak_canfd_frame_t tx = {0};
    dspic33ak_canfd_frame_t rx = {0};
    dspic33ak_canfd_bus_status_t st;

    if (!can_ready) {
        return;
    }

    /* A lone node parks at error-passive (the CAN spec keeps it out of bus-off on
     * ACK-only errors) and keeps bursting, so this rarely fires; it is a defensive
     * recovery in case a real bus fault ever does drive the controller bus-off. */
    if ((dspic33ak_canfd_get_status(DSPIC33AK_CANFD_INST_1, &st) == DSPIC33AK_CANFD_OK)
        && st.bus_off) {
        printf(" [CAN1] bus-off -> re-init\n");
        (void)can_bringup(DSPIC33AK_CANFD_MODE_NORMAL_FD);
    }

    /* Transmit one frame on the real bus (drives CANH/CANL via the transceiver). */
    build_frame((uint8_t)beat, &tx);
    if (dspic33ak_canfd_transmit(DSPIC33AK_CANFD_INST_1, &tx) == DSPIC33AK_CANFD_OK) {
        can_log_frame('<', "Tx", &tx);
    } else {
        printf(" <[CAN1 Tx] transmit queue full / timeout\n");
    }

    /* Bus health: TEC stays 0 when a partner ACKs; it climbs (warning ->
     * error-passive -> bus-off) when transmitting alone. */
    if (dspic33ak_canfd_get_status(DSPIC33AK_CANFD_INST_1, &st) == DSPIC33AK_CANFD_OK) {
        printf(" [CAN1] state=%s TEC=%u REC=%u\n",
               bus_state_str(&st), (unsigned)st.tx_err_count, (unsigned)st.rx_err_count);
    }

    /* If a partner echoed a frame back, show it (nothing arrives when alone). */
    if (dspic33ak_canfd_rx_available(DSPIC33AK_CANFD_INST_1)) {
        if (dspic33ak_canfd_receive(DSPIC33AK_CANFD_INST_1, &rx) == DSPIC33AK_CANFD_OK) {
            can_log_frame('>', "Rx", &rx);
        }
    }

    printf("\n");   /* blank line so the CAN and I2C blocks stay separated */
}
