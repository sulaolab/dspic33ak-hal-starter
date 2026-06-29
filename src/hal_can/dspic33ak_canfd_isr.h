/**
 * @file    dspic33ak_canfd_isr.h
 * @brief   dsPIC33AK CAN FD HAL - OPTIONAL interrupt / event layer.
 *
 * This file is entirely additive and OPTIONAL. The basic CAN HAL
 * (dspic33ak_canfd_node.h: blocking init / transmit / receive) works on its own
 * and is unchanged by this layer - a user who only needs simple blocking CAN
 * never has to include this header or compile dspic33ak_canfd_isr.c.
 *
 * This layer is RX-focused. It adds what an event-driven receiver needs (and
 * what a CMSIS-Driver CAN wrapper maps onto): a user event callback, RX
 * interrupt enable/disable, an ISR entry point the application forwards its CAN
 * vectors to, and a synchronous bus-health query. Transmit stays blocking
 * (dspic33ak_canfd_transmit, node layer); this layer does NOT enable any TX
 * interrupt. No ARM_CAN_* types appear here; those stay in the (future) CMSIS
 * wrapper.
 *
 * Opt-in sequence (RX events):
 *   1. dspic33ak_canfd_init(...)                       // basic init (node layer)
 *   2. dspic33ak_canfd_isr_enable(inst, cb, ud, prio)  // register callback + arm RX
 *   3. forward the RX + general CAN vectors to dspic33ak_canfd_irq_handler(inst)
 *   4. transmit with the blocking dspic33ak_canfd_transmit(); receive in the callback
 *
 * NOTE (dsPIC33AK): the CAN FD module raises SEPARATE CPU interrupts for
 * receive (_CxRXInterrupt) and general/error (_CxInterrupt); RX-FIFO-not-empty
 * arrives on _CxRXInterrupt and RX-overflow on _CxInterrupt. Forward BOTH to the
 * single dspic33ak_canfd_irq_handler() - it reads CxINT and dispatches. The
 * transmit interrupt (_CxTXInterrupt) is intentionally never enabled by this
 * layer, so its vector does not need to be forwarded for RX operation.
 */
#ifndef DSPIC33AK_CANFD_ISR_H
#define DSPIC33AK_CANFD_ISR_H

#include "dspic33ak_canfd.h"
#include "dspic33ak_canfd_node.h"   /* dspic33ak_canfd_frame_t for experimental tx_start */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Experimental TX-complete interrupt path (async transmit) - DISABLED by default.
 *
 * The TX-complete interrupt is NOT validated on dsPIC33AK512MPS512: arming it
 * currently triggers an unhandled-interrupt trap. To make sure it can never be
 * enabled unintentionally, the async-transmit helpers (dspic33ak_canfd_tx_start /
 * _tx_is_busy / _tx_abort) and the handler's TX-complete handling are compiled
 * out unless a project explicitly opts in by building with
 * -DDSPIC33AK_CANFD_ENABLE_EXPERIMENTAL_TX_IRQ=1. Until it is validated, use the
 * blocking dspic33ak_canfd_transmit() for TX and the RX events for receive.
 */
#ifndef DSPIC33AK_CANFD_ENABLE_EXPERIMENTAL_TX_IRQ
#define DSPIC33AK_CANFD_ENABLE_EXPERIMENTAL_TX_IRQ  0
#endif

/** Default CPU interrupt priority used when a caller passes 0 to isr_enable. */
#define DSPIC33AK_CANFD_ISR_DEFAULT_PRIORITY  4u

/* ---------------------------------------------------------------------- */
/* Event bit-flags. The callback receives an OR of these per ISR.         */
/* (Style mirrors hal_uart EVENT_*; maps cleanly to ARM_CAN_EVENT_* later.)*/
/* ---------------------------------------------------------------------- */
#define DSPIC33AK_CANFD_EVENT_TX_COMPLETE   (1u << 0)  /* reserved: experimental TX IRQ only */
#define DSPIC33AK_CANFD_EVENT_RX_AVAILABLE  (1u << 1)  /* >=1 frame waiting in RX FIFO */
#define DSPIC33AK_CANFD_EVENT_BUS_ERROR     (1u << 2)  /* error state changed (CERRIF) */
#define DSPIC33AK_CANFD_EVENT_BUS_OFF       (1u << 3)  /* controller went bus-off */
#define DSPIC33AK_CANFD_EVENT_RX_OVERFLOW   (1u << 4)  /* RX FIFO overflow */
#define DSPIC33AK_CANFD_EVENT_INVALID_MSG   (1u << 5)  /* invalid message (IVMIF) */

/**
 * User event callback. Invoked from dspic33ak_canfd_irq_handler() context with
 * an OR of the DSPIC33AK_CANFD_EVENT_* bits that occurred.
 *
 * On DSPIC33AK_CANFD_EVENT_RX_AVAILABLE the handler does NOT drain the FIFO.
 * The callback must drain every pending frame before it returns:
 *
 *   while (dspic33ak_canfd_rx_available(inst)) {
 *       (void)dspic33ak_canfd_receive(inst, &frame);
 *   }
 *
 * The rx_available() guard matters because dspic33ak_canfd_receive() is the
 * blocking node-layer API. With data already pending it returns immediately,
 * and draining the FIFO prevents the RX interrupt from re-asserting at once.
 */
typedef void (*dspic33ak_canfd_event_callback_t)(dspic33ak_canfd_instance_t inst,
                                                 uint32_t events,
                                                 void *user_data);

/** Decoded CxTREC error state + RX-FIFO overflow (synchronous snapshot). */
typedef struct {
    uint8_t tx_err_count;   /* TERRCNT */
    uint8_t rx_err_count;   /* RERRCNT */
    bool    error_warning;  /* EWARN  - TEC or REC >= 96 */
    bool    error_passive;  /* TXBP or RXBP */
    bool    bus_off;        /* TXBO   */
    bool    rx_overflow;    /* RX FIFO 1 overflowed (CxFIFOSTA1.RXOVIF, sticky) -
                             * the "status" path to detect overflow without the
                             * interrupt/callback. See dspic33ak_canfd_get_status(). */
} dspic33ak_canfd_bus_status_t;

/* ---------------------------------------------------------------------- */
/* Setup                                                                  */
/* ---------------------------------------------------------------------- */

/**
 * Register the RX event callback and arm receive interrupts in one call.
 *
 * This is the whole RX-ISR setup: it stores @p callback / @p user_data, arms the
 * RX-not-empty and RX-overflow module sources, and enables ONLY the RX-related
 * CPU interrupt lines (the receive line _CxRX and the general line _Cx, which
 * carries RX-overflow) at @p priority. Call after dspic33ak_canfd_init().
 *
 * @p priority is 1..7; pass 0 for DSPIC33AK_CANFD_ISR_DEFAULT_PRIORITY.
 * @p callback is required because the RX FIFO must be drained from the RX event
 * path. Passing NULL returns DSPIC33AK_CANFD_ERR_INVALID_ARG.
 *
 * TX is intentionally untouched: NO transmit interrupt line (_CxTX) and no
 * TX-complete source are enabled here - transmit stays blocking. Bus-error
 * (CERR) / invalid-message (IVM) sources are likewise NOT armed: on dsPIC33AK
 * they are delivered on separate CPU vectors this layer does not forward, so
 * enabling them would trap. Bus health is queried synchronously via
 * dspic33ak_canfd_get_status(), and BUS_OFF is derived from CxTREC in the handler.
 *
 * The HAL does NOT define the interrupt vectors; the application owns the
 * _CxRXInterrupt and _CxInterrupt vectors and forwards both to
 * dspic33ak_canfd_irq_handler(inst).
 */
dspic33ak_canfd_status_t dspic33ak_canfd_isr_enable(
    dspic33ak_canfd_instance_t inst,
    dspic33ak_canfd_event_callback_t callback,
    void *user_data,
    uint8_t priority);

/**
 * Re-point the event callback without re-arming.
 * Optional - dspic33ak_canfd_isr_enable() already registers a callback; use this
 * only to swap the callback on an already-armed instance. Passing NULL returns
 * DSPIC33AK_CANFD_ERR_INVALID_ARG.
 */
dspic33ak_canfd_status_t dspic33ak_canfd_isr_set_callback(
    dspic33ak_canfd_instance_t inst,
    dspic33ak_canfd_event_callback_t callback,
    void *user_data);

/** Disable the RX CPU interrupt lines and RX module interrupt sources. */
dspic33ak_canfd_status_t dspic33ak_canfd_isr_disable(dspic33ak_canfd_instance_t inst);

/* ---------------------------------------------------------------------- */
/* Async transmit - EXPERIMENTAL, opt-in, RESERVED                        */
/*                                                                        */
/* Compiled in only with -DDSPIC33AK_CANFD_ENABLE_EXPERIMENTAL_TX_IRQ=1.  */
/* The TX-complete interrupt is NOT validated on this part (it traps); the */
/* default build does not declare or enable any of this. Treat it as       */
/* reserved until validated. TX in the supported flow stays blocking.      */
/* ---------------------------------------------------------------------- */
#if DSPIC33AK_CANFD_ENABLE_EXPERIMENTAL_TX_IRQ

/**
 * Queue @p frame into the TX queue (same path as dspic33ak_canfd_transmit) and
 * arm the TXQ-empty interrupt so DSPIC33AK_CANFD_EVENT_TX_COMPLETE fires once
 * all queued frames have actually been transmitted on the bus.
 *
 * This is NOT fully asynchronous: it goes through dspic33ak_canfd_transmit(), so
 * if the TX queue is full it first blocks for queue space (honoring the
 * configured timeout). Once the frame is queued it returns without waiting for
 * the frame to leave the bus.
 *
 * NOTE: the RX event path (isr_enable + RX_AVAILABLE) is HW-validated. The
 * TX-complete interrupt arming here is NOT yet validated on dsPIC33AK512MPS512 -
 * enabling it currently triggers an unhandled-interrupt trap, under
 * investigation. This is why the whole async-TX path is opt-in.
 */
dspic33ak_canfd_status_t dspic33ak_canfd_tx_start(dspic33ak_canfd_instance_t inst,
                                                  const dspic33ak_canfd_frame_t *frame);

/** True between tx_start() and the TX_COMPLETE event. */
bool dspic33ak_canfd_tx_is_busy(dspic33ak_canfd_instance_t inst);

/** Disarm the TX-complete interrupt and clear the busy flag (does not abort an
 *  in-flight frame on the wire). */
dspic33ak_canfd_status_t dspic33ak_canfd_tx_abort(dspic33ak_canfd_instance_t inst);

#endif /* DSPIC33AK_CANFD_ENABLE_EXPERIMENTAL_TX_IRQ */

/* ---------------------------------------------------------------------- */
/* ISR entry + status                                                     */
/* ---------------------------------------------------------------------- */

/**
 * Service the module interrupt: read/clear the module flags, build the event
 * word and invoke the callback. Ordinary function, NOT an ISR vector - forward
 * the instance's RX and general CAN vectors to it (the TX vector is not enabled
 * by this layer and does not need forwarding):
 *
 *   void __attribute__((interrupt, no_auto_psv)) _C1RXInterrupt(void)
 *   { dspic33ak_canfd_irq_handler(DSPIC33AK_CANFD_INST_1); }
 *   void __attribute__((interrupt, no_auto_psv)) _C1Interrupt(void)
 *   { dspic33ak_canfd_irq_handler(DSPIC33AK_CANFD_INST_1); }
 */
void dspic33ak_canfd_irq_handler(dspic33ak_canfd_instance_t inst);

/**
 * Read and decode CxTREC (error counters / state) and the RX-FIFO overflow flag.
 * Works with or without interrupts enabled - this is the synchronous "status"
 * path for overflow detection that complements the RX_OVERFLOW callback event.
 * Use dspic33ak_canfd_clear_rx_overflow() to clear the sticky overflow flag.
 */
dspic33ak_canfd_status_t dspic33ak_canfd_get_status(dspic33ak_canfd_instance_t inst,
                                                    dspic33ak_canfd_bus_status_t *status);

/** Clear the sticky RX-FIFO-overflow flag (CxFIFOSTA1.RXOVIF). */
dspic33ak_canfd_status_t dspic33ak_canfd_clear_rx_overflow(dspic33ak_canfd_instance_t inst);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_CANFD_ISR_H */
