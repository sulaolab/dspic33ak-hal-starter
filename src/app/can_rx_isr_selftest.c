/*
 * can_rx_isr_selftest.c
 * ---------------------
 * Single-board CAN FD RX-interrupt self-test (see can_rx_isr_selftest.h).
 *
 * Uses INTERNAL loopback, so every frame transmitted is self-received with no
 * bus, transceiver or partner. The supported RX-ISR API is exercised exactly as
 * an application would:
 *
 *     dspic33ak_canfd_init(...);                                 // node layer
 *     dspic33ak_canfd_isr_enable(inst, callback, &ctx, prio);    // one-call RX setup
 *     dspic33ak_canfd_transmit(inst, &frame);                    // blocking TX
 *     // frames are delivered to `callback`, drained there with receive()
 */

#include "can_rx_isr_selftest.h"
#include "can_bus_test.h"   /* CAN_BUS_TEST: decides who owns the CAN1 vectors */

#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dspic33ak_canfd_node.h"
#include "dspic33ak_canfd_isr.h"
#include "dspic33ak_tick_timer.h"
#include "starter_clock.h"

#define RXISR_TEST_ID        0x222u
#define RXISR_OVERFLOW_TX    24u    /* >> RX FIFO depth, so the FIFO must overflow */
#define RXISR_WAIT_MS        20u    /* generous bound for self-loopback delivery   */

/* Caller-owned CAN message RAM (TX queue + RX FIFO 1), 4-byte aligned. */
static uint32_t rxisr_msg_ram[DSPIC33AK_CANFD_MSG_RAM_WORDS] __attribute__((aligned(4)));

/* Shared between the ISR callback and the test body. */
typedef struct {
    volatile uint32_t rx_frames;       /* frames drained in the callback        */
    volatile uint32_t overflow_events; /* RX_OVERFLOW events seen               */
    volatile uint32_t last_events;     /* OR of the most recent event word      */
    dspic33ak_canfd_frame_t last_rx;   /* last frame drained (for content check)*/
} rxisr_ctx_t;

static rxisr_ctx_t g_ctx;

/* RX event callback (CAN ISR context). Drains the RX FIFO via receive() - data
 * is already present, so receive() returns without blocking - and records what
 * happened. No printf / blocking calls here. */
static void rxisr_callback(dspic33ak_canfd_instance_t inst, uint32_t events, void *user_data)
{
    rxisr_ctx_t *ctx = (rxisr_ctx_t *)user_data;

    ctx->last_events = events;

    if ((events & DSPIC33AK_CANFD_EVENT_RX_OVERFLOW) != 0u) {
        ctx->overflow_events++;
    }
    if ((events & DSPIC33AK_CANFD_EVENT_RX_AVAILABLE) != 0u) {
        dspic33ak_canfd_frame_t f;
        while (dspic33ak_canfd_rx_available(inst)) {
            if (dspic33ak_canfd_receive(inst, &f) != DSPIC33AK_CANFD_OK) {
                break;
            }
            ctx->last_rx = f;
            ctx->rx_frames++;
        }
    }
}

/* CAN1 RX + general vector forwarders for the single-board build. The TX vector
 * is intentionally NOT defined: dspic33ak_canfd_isr_enable() never enables the
 * _C1TXIE line, so it cannot fire. (When CAN_BUS_TEST=1 the two-board test owns
 * the CAN1 vectors instead, so guard these out to avoid duplicate symbols.) */
#if !CAN_BUS_TEST
void __attribute__((interrupt, no_auto_psv)) _C1RXInterrupt(void)
{
    dspic33ak_canfd_irq_handler(DSPIC33AK_CANFD_INST_1);
}
void __attribute__((interrupt, no_auto_psv)) _C1Interrupt(void)
{
    dspic33ak_canfd_irq_handler(DSPIC33AK_CANFD_INST_1);
}
#endif

static bool rxisr_bringup(void)
{
    dspic33ak_canfd_config_t cfg = {0};

    cfg.can_clk_hz   = STARTER_CLOCK_CAN_FCAN_HZ;
    cfg.nominal_bps  = 500000u;
    cfg.data_bps     = 2000000u;
    cfg.sample_pct   = 80u;
    cfg.brs          = true;
    cfg.mode         = DSPIC33AK_CANFD_MODE_INTERNAL_LOOPBACK;
    cfg.timeout_ms   = 10u;
    cfg.get_ms       = dspic33ak_tick_timer_get_ms;
    cfg.msg_ram      = rxisr_msg_ram;
    cfg.msg_ram_size = (uint16_t)sizeof(rxisr_msg_ram);

    return dspic33ak_canfd_init(DSPIC33AK_CANFD_INST_1, &cfg) == DSPIC33AK_CANFD_OK;
}

static void rxisr_build(uint8_t seed, uint8_t len, dspic33ak_canfd_frame_t *tx)
{
    uint8_t i;
    memset(tx, 0, sizeof(*tx));
    tx->id  = RXISR_TEST_ID;
    tx->fd  = true;
    tx->brs = true;
    tx->len = len;
    for (i = 0u; i < len; i++) {
        tx->data[i] = (uint8_t)(seed + i);
    }
}

/* Spin (bounded) until `*counter` reaches `target` or the timeout elapses. */
static bool rxisr_wait_count(volatile uint32_t *counter, uint32_t target)
{
    uint32_t start = dspic33ak_tick_timer_get_ms();
    while (*counter < target) {
        if ((uint32_t)(dspic33ak_tick_timer_get_ms() - start) >= RXISR_WAIT_MS) {
            return false;
        }
    }
    return true;
}

/* True if the transmit interrupt line for CAN1 is currently disabled. */
static bool rxisr_tx_line_disabled(void)
{
#if defined(_C1TXIE)
    return _C1TXIE == 0u;
#else
    return true;   /* no such line on this part */
#endif
}

bool can_rx_isr_selftest_run(void)
{
    dspic33ak_canfd_frame_t tx;
    dspic33ak_canfd_bus_status_t bs;
    bool callback_ok;
    bool content_ok;
    bool overflow_status;
    bool overflow_cb;
    bool overflow_ok;
    bool tx_line_off;
    uint32_t phase2_drained;
    uint8_t i;

    memset(&g_ctx, 0, sizeof(g_ctx));

    if (!rxisr_bringup()) {
        printf(" CAN1 RX-ISR self-test: init FAILED\n");
        return false;
    }

    /* One-call RX setup: register callback + arm RX interrupts (RX + general
     * CPU lines only). priority 0 => DSPIC33AK_CANFD_ISR_DEFAULT_PRIORITY. */
    if (dspic33ak_canfd_isr_enable(DSPIC33AK_CANFD_INST_1, rxisr_callback, &g_ctx, 0u)
        != DSPIC33AK_CANFD_OK) {
        printf(" CAN1 RX-ISR self-test: isr_enable FAILED\n");
        (void)dspic33ak_canfd_deinit(DSPIC33AK_CANFD_INST_1);
        return false;
    }

    /* Check immediately after arming that TX line was NOT enabled. */
    tx_line_off = rxisr_tx_line_disabled();

    /* ---- Phase 1: RX callback + receive() ----
     * Transmit one frame; internal loopback delivers it, the RX interrupt fires,
     * and the callback drains it. */
    rxisr_build(0x40u, 8u, &tx);
    (void)dspic33ak_canfd_transmit(DSPIC33AK_CANFD_INST_1, &tx);
    callback_ok = rxisr_wait_count(&g_ctx.rx_frames, 1u);

    content_ok = callback_ok && (g_ctx.last_rx.id == tx.id) && (g_ctx.last_rx.len == tx.len);
    for (i = 0u; content_ok && i < tx.len; i++) {
        if (g_ctx.last_rx.data[i] != tx.data[i]) {
            content_ok = false;
        }
    }

    /* ---- Phase 2: RX overflow detection ----
     * Quiesce the ISR so nothing drains the RX FIFO, transmit far more frames
     * than the FIFO holds (the extras have nowhere to go and overflow), then
     * check both detection paths:
     *   - STATUS: the sticky CxFIFOSTA1.RXOVIF read via get_status() (no IRQ),
     *   - CALLBACK: re-arm, and the pending overflow + not-empty flags fire one
     *     callback that reports RX_OVERFLOW (and drains the held frames). */
    (void)dspic33ak_canfd_isr_disable(DSPIC33AK_CANFD_INST_1);
    (void)dspic33ak_canfd_clear_rx_overflow(DSPIC33AK_CANFD_INST_1);
    g_ctx.rx_frames = 0u;          /* count only phase-2 frames from here */
    for (i = 0u; i < RXISR_OVERFLOW_TX; i++) {
        rxisr_build((uint8_t)(0x80u + i), 8u, &tx);
        (void)dspic33ak_canfd_transmit(DSPIC33AK_CANFD_INST_1, &tx);
    }

    /* Status path: read the sticky overflow flag while the ISR is still off. */
    overflow_status = (dspic33ak_canfd_get_status(DSPIC33AK_CANFD_INST_1, &bs) == DSPIC33AK_CANFD_OK)
                      && bs.rx_overflow;

    /* Callback path: re-arm and let the pending flags fire one event. */
    g_ctx.overflow_events = 0u;
    (void)dspic33ak_canfd_isr_enable(DSPIC33AK_CANFD_INST_1, rxisr_callback, &g_ctx, 0u);
    overflow_cb = rxisr_wait_count(&g_ctx.overflow_events, 1u);
    phase2_drained = g_ctx.rx_frames;   /* frames the FIFO actually held = depth */

    overflow_ok = overflow_status || overflow_cb;

    /* Leave CAN1 quiesced + de-initialized so the live can_loopback demo owns it. */
    (void)dspic33ak_canfd_isr_disable(DSPIC33AK_CANFD_INST_1);
    (void)dspic33ak_canfd_deinit(DSPIC33AK_CANFD_INST_1);

    printf(" CAN1 RX-ISR self-test:\n");
    printf("   callback fired     : %s\n", callback_ok ? "yes" : "NO");
    printf("   frame content      : %s\n", content_ok ? "match" : "MISMATCH");
    printf("   RX overflow        : %s (status=%s, callback=%s; %lu/%u frames held)\n",
           overflow_ok ? "detected" : "NOT detected",
           overflow_status ? "yes" : "no", overflow_cb ? "yes" : "no",
           (unsigned long)phase2_drained, (unsigned)RXISR_OVERFLOW_TX);
    printf("   TX interrupt line  : %s\n", tx_line_off ? "disabled (as required)" : "ENABLED (unexpected)");

    return callback_ok && content_ok && overflow_ok && tx_line_off;
}
