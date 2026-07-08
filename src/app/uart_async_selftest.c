#include "uart_async_selftest.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dspic33ak_tick_timer.h"
#include "dspic33ak_uart.h"

#define UART_ASYNC_INST              DSPIC33AK_UART_INST_1
#define UART_ASYNC_TX_EVENT_TIMEOUT_MS   1000u
#define UART_ASYNC_TX_DONE_TIMEOUT_MS    1000u
#define UART_ASYNC_RX_TIMEOUT_MS        10000u

typedef struct {
    bool tx_start_ok;
    bool tx_send_complete_ok;
    bool tx_count_ok;
    bool tx_done_ok;
    bool tx_not_busy_ok;
    bool tx_timeout;
    dspic33ak_uart_status_t tx_start_status;
    size_t tx_count;
    size_t tx_expected;

    bool rx_abort_start_ok;
    bool rx_abort_ok;
    bool rx_abort_not_busy_ok;
    bool rx_abort_no_complete_ok;
    bool rx_abort_count_ok;
    dspic33ak_uart_status_t rx_abort_start_status;
    dspic33ak_uart_status_t rx_abort_status;
    size_t rx_abort_count;
    size_t rx_abort_len;

    bool rx_start_ok;
    bool rx_complete_ok;
    bool rx_count_ok;
    bool rx_not_busy_ok;
    bool rx_data_ok;
    bool rx_timeout;
    dspic33ak_uart_status_t rx_start_status;
    size_t rx_count;
} uart_async_selftest_result_t;

static volatile uint32_t s_uart_async_events;
static volatile uint32_t s_uart_async_callback_count;
static volatile uint32_t s_uart_async_unexpected_inst_count;
static volatile uint32_t s_uart_async_total_callback_count;
static volatile uint32_t s_uart_async_total_unexpected_inst_count;

static const uint8_t s_tx_message[] =
    "[UART-ASYNC-TX] interrupt-driven TX path active\r\n";
static const uint8_t s_rx_expected[] = { 'R', 'X', 'O', 'K' };

static void uart_async_event_callback(
    dspic33ak_uart_instance_t inst,
    uint32_t events,
    void *user_data)
{
    (void)user_data;

    if (inst != UART_ASYNC_INST) {
        s_uart_async_unexpected_inst_count++;
        s_uart_async_total_unexpected_inst_count++;
    }
    s_uart_async_events |= events;
    s_uart_async_callback_count++;
    s_uart_async_total_callback_count++;
}

static void event_state_clear(void)
{
    s_uart_async_events = 0u;
    s_uart_async_callback_count = 0u;
    s_uart_async_unexpected_inst_count = 0u;
}

static void event_totals_clear(void)
{
    s_uart_async_total_callback_count = 0u;
    s_uart_async_total_unexpected_inst_count = 0u;
}

static bool elapsed_ms(uint32_t start_ms, uint32_t timeout_ms)
{
    return ((uint32_t)(dspic33ak_tick_timer_get_ms() - start_ms) >= timeout_ms);
}

static bool wait_for_event(uint32_t event_mask, uint32_t timeout_ms)
{
    const uint32_t start_ms = dspic33ak_tick_timer_get_ms();

    while ((s_uart_async_events & event_mask) == 0u) {
        if (elapsed_ms(start_ms, timeout_ms)) {
            return false;
        }
    }
    return true;
}

static bool wait_for_tx_done(uint32_t timeout_ms)
{
    const uint32_t start_ms = dspic33ak_tick_timer_get_ms();

    while (!dspic33ak_uart_tx_done(UART_ASYNC_INST)) {
        if (elapsed_ms(start_ms, timeout_ms)) {
            return false;
        }
    }
    return true;
}

static void tx_cleanup(void)
{
    (void)dspic33ak_uart_tx_abort(UART_ASYNC_INST);
    (void)wait_for_tx_done(UART_ASYNC_TX_DONE_TIMEOUT_MS);
}

static void selftest_cleanup(void)
{
    tx_cleanup();
    (void)dspic33ak_uart_rx_abort(UART_ASYNC_INST);
    (void)dspic33ak_uart_set_callback(UART_ASYNC_INST, NULL, NULL);
    dspic33ak_uart_rx_flush(UART_ASYNC_INST);
}

static const char *pass_fail(bool pass)
{
    return pass ? "PASS" : "FAIL";
}

static const char *pass_timeout_fail(bool pass, bool timeout)
{
    if (pass) {
        return "PASS";
    }
    return timeout ? "TIMEOUT" : "FAIL";
}

static void run_tx_test(uart_async_selftest_result_t *result)
{
    result->tx_expected = sizeof(s_tx_message) - 1u;

    printf(" [UART-ASYNC] TX test starts; async message follows.\n");
    (void)wait_for_tx_done(UART_ASYNC_TX_DONE_TIMEOUT_MS);

    event_state_clear();
    result->tx_start_status =
        dspic33ak_uart_tx_start(UART_ASYNC_INST, s_tx_message, result->tx_expected);
    result->tx_start_ok = (result->tx_start_status == DSPIC33AK_UART_OK);
    if (!result->tx_start_ok) {
        tx_cleanup();
        return;
    }

    result->tx_send_complete_ok =
        wait_for_event(DSPIC33AK_UART_EVENT_SEND_COMPLETE,
                       UART_ASYNC_TX_EVENT_TIMEOUT_MS);
    result->tx_timeout = !result->tx_send_complete_ok;
    if (!result->tx_send_complete_ok) {
        tx_cleanup();
        return;
    }

    result->tx_count = dspic33ak_uart_tx_count_get(UART_ASYNC_INST);
    result->tx_count_ok = (result->tx_count == result->tx_expected);
    result->tx_not_busy_ok = !dspic33ak_uart_tx_is_busy(UART_ASYNC_INST);
    result->tx_done_ok = wait_for_tx_done(UART_ASYNC_TX_DONE_TIMEOUT_MS);
}

static void run_rx_abort_test(uart_async_selftest_result_t *result)
{
    uint8_t rx_abort_buffer[8u];

    memset(rx_abort_buffer, 0, sizeof(rx_abort_buffer));
    result->rx_abort_len = sizeof(rx_abort_buffer);

    event_state_clear();
    result->rx_abort_start_status =
        dspic33ak_uart_rx_start_clean(UART_ASYNC_INST,
                                      rx_abort_buffer,
                                      sizeof(rx_abort_buffer));
    result->rx_abort_start_ok =
        (result->rx_abort_start_status == DSPIC33AK_UART_OK);

    result->rx_abort_status = dspic33ak_uart_rx_abort(UART_ASYNC_INST);
    result->rx_abort_ok = (result->rx_abort_status == DSPIC33AK_UART_OK);
    result->rx_abort_not_busy_ok = !dspic33ak_uart_rx_is_busy(UART_ASYNC_INST);
    result->rx_abort_no_complete_ok =
        ((s_uart_async_events & DSPIC33AK_UART_EVENT_RX_COMPLETE) == 0u);
    result->rx_abort_count = dspic33ak_uart_rx_count_get(UART_ASYNC_INST);
    result->rx_abort_count_ok = (result->rx_abort_count <= result->rx_abort_len);
}

static void run_rx_interactive_test(uart_async_selftest_result_t *result)
{
    uint8_t rx_buffer[sizeof(s_rx_expected)];

    memset(rx_buffer, 0, sizeof(rx_buffer));

    printf(" [UART-ASYNC-RX] Type exactly: RXOK\n");
    (void)wait_for_tx_done(UART_ASYNC_TX_DONE_TIMEOUT_MS);

    event_state_clear();
    result->rx_start_status =
        dspic33ak_uart_rx_start_clean(UART_ASYNC_INST, rx_buffer, sizeof(rx_buffer));
    result->rx_start_ok = (result->rx_start_status == DSPIC33AK_UART_OK);
    if (!result->rx_start_ok) {
        return;
    }

    result->rx_complete_ok =
        wait_for_event(DSPIC33AK_UART_EVENT_RX_COMPLETE, UART_ASYNC_RX_TIMEOUT_MS);
    result->rx_timeout = !result->rx_complete_ok;
    if (!result->rx_complete_ok) {
        (void)dspic33ak_uart_rx_abort(UART_ASYNC_INST);
        return;
    }

    result->rx_count = dspic33ak_uart_rx_count_get(UART_ASYNC_INST);
    result->rx_count_ok = (result->rx_count == sizeof(s_rx_expected));
    result->rx_not_busy_ok = !dspic33ak_uart_rx_is_busy(UART_ASYNC_INST);
    result->rx_data_ok =
        (memcmp(rx_buffer, s_rx_expected, sizeof(s_rx_expected)) == 0);
}

static bool overall_pass(const uart_async_selftest_result_t *result)
{
    return result->tx_start_ok &&
           result->tx_send_complete_ok &&
           result->tx_count_ok &&
           result->tx_not_busy_ok &&
           result->tx_done_ok &&
           result->rx_abort_start_ok &&
           result->rx_abort_ok &&
           result->rx_abort_not_busy_ok &&
           result->rx_abort_no_complete_ok &&
           result->rx_abort_count_ok &&
           result->rx_start_ok &&
           result->rx_complete_ok &&
           result->rx_count_ok &&
           result->rx_not_busy_ok &&
           result->rx_data_ok &&
           (s_uart_async_total_unexpected_inst_count == 0u);
}

static void print_summary(const uart_async_selftest_result_t *result, bool pass)
{
    printf(" TX start             : %s (status=%d)\n",
           pass_fail(result->tx_start_ok),
           (int)result->tx_start_status);
    printf(" TX SEND_COMPLETE     : %s\n",
           pass_timeout_fail(result->tx_send_complete_ok, result->tx_timeout));
    printf(" TX count             : %s (%u/%u)\n",
           pass_fail(result->tx_count_ok),
           (unsigned)result->tx_count,
           (unsigned)result->tx_expected);
    printf(" TX busy clear        : %s\n", pass_fail(result->tx_not_busy_ok));
    printf(" TX line done         : %s\n", pass_fail(result->tx_done_ok));

    printf(" RX abort start       : %s (status=%d)\n",
           pass_fail(result->rx_abort_start_ok),
           (int)result->rx_abort_start_status);
    printf(" RX abort             : %s (status=%d)\n",
           pass_fail(result->rx_abort_ok),
           (int)result->rx_abort_status);
    printf(" RX abort busy clear  : %s\n",
           pass_fail(result->rx_abort_not_busy_ok));
    printf(" RX abort no complete : %s\n",
           pass_fail(result->rx_abort_no_complete_ok));
    printf(" RX abort count       : %s (%u/%u max)\n",
           pass_fail(result->rx_abort_count_ok),
           (unsigned)result->rx_abort_count,
           (unsigned)result->rx_abort_len);

    printf(" RX start             : %s (status=%d)\n",
           pass_fail(result->rx_start_ok),
           (int)result->rx_start_status);
    printf(" RX COMPLETE          : %s\n",
           pass_timeout_fail(result->rx_complete_ok, result->rx_timeout));
    printf(" RX count             : %s (%u/4)\n",
           pass_fail(result->rx_count_ok),
           (unsigned)result->rx_count);
    printf(" RX busy clear        : %s\n", pass_fail(result->rx_not_busy_ok));
    printf(" RX data              : %s\n", pass_fail(result->rx_data_ok));
    printf(" Callback instance    : %s (unexpected=%lu, callbacks=%lu)\n",
           pass_fail(s_uart_async_total_unexpected_inst_count == 0u),
           (unsigned long)s_uart_async_total_unexpected_inst_count,
           (unsigned long)s_uart_async_total_callback_count);
    printf(" UART async self-test : %s\n", pass_fail(pass));
}

bool uart_async_selftest_run(void)
{
    uart_async_selftest_result_t result;
    bool callback_ok;
    bool pass;

    memset(&result, 0, sizeof(result));
    event_totals_clear();

    printf("==============================================\n");
    printf(" UART async self-test\n");

    callback_ok =
        (dspic33ak_uart_set_callback(UART_ASYNC_INST,
                                     uart_async_event_callback,
                                     NULL) == DSPIC33AK_UART_OK);
    if (!callback_ok) {
        printf(" Callback registration: FAIL\n");
        selftest_cleanup();
        printf(" UART async self-test : FAIL\n");
        printf("==============================================\n");
        return false;
    }

    run_tx_test(&result);
    run_rx_abort_test(&result);
    run_rx_interactive_test(&result);

    pass = overall_pass(&result);
    selftest_cleanup();
    print_summary(&result, pass);
    printf("==============================================\n");
    return pass;
}
