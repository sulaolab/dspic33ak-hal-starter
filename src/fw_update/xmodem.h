#ifndef XMODEM_H
#define XMODEM_H

//===========================================================
// xmodem.{c,h}
//
// Blocking XMODEM-CRC receiver for the serial Dual Bank update feature.
//
// Pure transport: it drives the sender with 'C' (CRC mode), receives 128-byte
// (SOH) or 1024-byte (STX) blocks, validates the sequence number + CRC-16-CCITT,
// ACK/NAKs, and hands each validated data block to a caller-supplied sink. It
// knows nothing about flash -- the sink decides what to do with the bytes.
//
// It talks to UART1 (the console) directly via the dspic33ak_uart HAL and takes
// over the RX path for the duration of the transfer, so the cooperative console
// pump (app_uart_process) must NOT be running concurrently. The caller invokes
// this from inside a console verb and returns to the normal pump afterwards.
//
// Timeouts use the starter's 1 ms tick timer. CRC mode only -- no checksum-mode
// fallback.
//===========================================================

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Outcome of a receive session.
typedef enum
{
    XMODEM_OK = 0,          // EOT after >= 1 data block; every block delivered
    XMODEM_ERR_TIMEOUT,     // handshake never answered, or too many timeouts
    XMODEM_ERR_CANCELLED,   // sender sent CAN CAN, or the error budget ran out
    XMODEM_ERR_SINK,        // sink callback requested abort (returned non-zero)
    XMODEM_ERR_SYNC         // unrecoverable framing / out-of-sequence block
} xmodem_status_t;

// Data sink. Called once per validated, in-order data block.
//   off  = byte offset of this block within the image (0, then += block len)
//   data = block payload (do not retain the pointer past the call)
//   len  = payload length (128 or 1024)
//   ctx  = opaque caller context
// Return 0 to accept the block, non-zero to abort the whole transfer (the
// receiver then sends CAN and xmodem_receive() returns XMODEM_ERR_SINK).
typedef int (*xmodem_sink_fn)( uint32_t off, const uint8_t* data, uint16_t len, void* ctx );

// Receive an image over XMODEM-CRC on UART1. Blocking until EOT / cancel /
// timeout. Each in-order block is delivered to `sink`. `bytes_received`
// (optional, may be NULL) receives the total payload bytes delivered, which is
// block-granular and therefore includes any CPMEOF padding in the final block.
xmodem_status_t xmodem_receive( xmodem_sink_fn sink, void* ctx, uint32_t* bytes_received );

// CRC-16-CCITT (XMODEM variant): poly 0x1021, init 0x0000, MSB-first, no final
// xor. Exposed so the flash orchestrator can compute the image / read-back CRC
// with the identical algorithm. `xmodem_crc16_cont` chains chunks: passing the
// prior return value as `crc` yields the same result as CRC-ing the whole span.
uint16_t xmodem_crc16( const uint8_t* data, uint16_t len );
uint16_t xmodem_crc16_cont( uint16_t crc, const uint8_t* data, uint16_t len );

#ifdef __cplusplus
}
#endif

#endif // XMODEM_H
