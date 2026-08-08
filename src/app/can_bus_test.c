/*
 * can_bus_test.c
 * --------------
 * Two-board CAN FD bus test (see can_bus_test.h).
 */

#include "can_bus_test.h"

#if CAN_BUS_TEST   /* whole file compiles only when the two-board test is enabled */

#include <stdint.h>
#include <stdio.h>

#include "nora_canfd_node.h"
#include "nora_canfd_isr.h"
#include "nora_tick_timer.h"
#include "starter_clock.h"

#define CAN_BUS_TEST_ORIG_ID  0x0A0u
#define CAN_BUS_TEST_ECHO_ID  0x0B0u
#define CAN_BUS_TEST_RXQ_LEN  16u

/* RX ring: producer = ISR (drains the hardware RX FIFO), consumer = main loop. */
static nora_canfd_frame_t g_rxq[CAN_BUS_TEST_RXQ_LEN];
static volatile uint8_t        g_rxq_head = 0u;
static volatile uint8_t        g_rxq_tail = 0u;

/* HAL event callback (CAN ISR context): drain the RX FIFO into the ring. Never
 * printf or call blocking APIs here; the main loop prints and re-transmits. */
static void can_bus_test_event(nora_canfd_instance_t inst, uint32_t events, void *user_data)
{
    (void)user_data;
    if ((events & NORA_CANFD_EVENT_RX_AVAILABLE) != 0u) {
        nora_canfd_frame_t f;
        while (nora_canfd_rx_available(inst)) {
            uint8_t nh;
            if (nora_canfd_receive(inst, &f) != NORA_CANFD_OK) {
                break;
            }
            nh = (uint8_t)((g_rxq_head + 1u) % CAN_BUS_TEST_RXQ_LEN);
            if (nh != g_rxq_tail) {
                g_rxq[g_rxq_head] = f;
                g_rxq_head = nh;
            }
        }
    }
}

/* dsPIC33AK CAN raises separate RX / TX / general CPU vectors. This RX-focused
 * layer uses only RX + general; the TX vector is intentionally not enabled. */
void __attribute__((interrupt, no_auto_psv)) _C1RXInterrupt(void) { nora_canfd_irq_handler(NORA_CANFD_INST_1); }
void __attribute__((interrupt, no_auto_psv)) _C1Interrupt(void)   { nora_canfd_irq_handler(NORA_CANFD_INST_1); }

/* Log one frame in the same format as the can_loopback demo on the other board:
 * " <[CAN1 Tx] id=0x0B0 len=64 data=...". No timestamp prefix (the terminal adds
 * its own); all `len` bytes are shown. */
static void bus_test_log(char dir, const char *tag, const nora_canfd_frame_t *f)
{
    uint8_t i;
    printf(" %c[CAN1 %s] id=0x%03lX len=%u data=", dir, tag,
           (unsigned long)f->id, (unsigned)f->len);
    for (i = 0u; i < f->len; i++) {
        printf("%02X", f->data[i]);
    }
    printf("\n");
}

void can_bus_test_run(bool is_echo)
{
    static uint32_t can1_msg_ram[NORA_CANFD_MSG_RAM_WORDS] __attribute__((aligned(4)));
    nora_canfd_config_t cfg = {0};
    nora_canfd_frame_t  tx  = {0};
    nora_canfd_frame_t  rx  = {0};
    const char *name = is_echo ? "CAN1:echo" : "CAN1:orig";
    uint32_t tx_count = 0u;
    uint32_t last_ms;
    uint8_t  k;

    cfg.can_clk_hz   = STARTER_CLOCK_CAN_FCAN_HZ;
    cfg.nominal_bps  = 500000u;
    cfg.data_bps     = 2000000u;
    cfg.sample_pct   = 80u;
    cfg.brs          = true;
    cfg.mode         = NORA_CANFD_MODE_NORMAL_FD;
    cfg.timeout_ms   = 10u;
    cfg.get_ms       = nora_tick_timer_get_ms;
    cfg.msg_ram      = can1_msg_ram;
    cfg.msg_ram_size = (uint16_t)sizeof(can1_msg_ram);

    printf(" CAN bus test: role=%s, NORMAL_FD 500k/2M, TX id=0x%03X\n",
           name, is_echo ? CAN_BUS_TEST_ECHO_ID : CAN_BUS_TEST_ORIG_ID);

    if (nora_canfd_init(NORA_CANFD_INST_1, &cfg) != NORA_CANFD_OK) {
        printf(" CAN bus test: init FAILED\n");
        for (;;) { }
    }
    (void)nora_canfd_isr_set_callback(NORA_CANFD_INST_1, can_bus_test_event, NULL);
    (void)nora_canfd_isr_enable(NORA_CANFD_INST_1, 4u);

    last_ms = nora_tick_timer_get_ms();
    for (;;) {
        uint32_t now = nora_tick_timer_get_ms();

        /* ORIGINATOR: send a frame with changing data once per second. If a lone
         * node has gone bus-off (no ACK partner yet), re-init so it recovers. */
        if (!is_echo && ((uint32_t)(now - last_ms) >= 1000u)) {
            nora_canfd_bus_status_t st;
            last_ms = now;
            if ((nora_canfd_get_status(NORA_CANFD_INST_1, &st) == NORA_CANFD_OK)
                && st.bus_off) {
                /* Quiesce the event layer before re-init drops the module into
                 * config mode, then re-arm it afterwards. */
                nora_canfd_isr_disable(NORA_CANFD_INST_1);
                (void)nora_canfd_init(NORA_CANFD_INST_1, &cfg);
                (void)nora_canfd_isr_set_callback(NORA_CANFD_INST_1, can_bus_test_event, NULL);
                (void)nora_canfd_isr_enable(NORA_CANFD_INST_1, 4u);
            }
            tx.id = CAN_BUS_TEST_ORIG_ID; tx.extended = false; tx.fd = true; tx.brs = true; tx.len = 8u;
            for (k = 0u; k < 8u; k++) {
                tx.data[k] = (uint8_t)(tx_count + k);
            }
            if (nora_canfd_transmit(NORA_CANFD_INST_1, &tx) == NORA_CANFD_OK) {
                bus_test_log('<', "Tx", &tx);
                tx_count++;
            }
        }

        /* Drain frames the ISR queued; print, and (echo node) re-send under our id. */
        while (g_rxq_tail != g_rxq_head) {
            rx = g_rxq[g_rxq_tail];
            g_rxq_tail = (uint8_t)((g_rxq_tail + 1u) % CAN_BUS_TEST_RXQ_LEN);
            bus_test_log('>', "Rx", &rx);
            if (is_echo) {
                rx.id = CAN_BUS_TEST_ECHO_ID;
                if (nora_canfd_transmit(NORA_CANFD_INST_1, &rx) == NORA_CANFD_OK) {
                    bus_test_log('<', "Tx", &rx);
                }
            }
        }
    }
}

#endif /* CAN_BUS_TEST */
