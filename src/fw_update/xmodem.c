//===========================================================
// xmodem.c -- blocking XMODEM-CRC receiver (see xmodem.h)
//===========================================================

#include "xmodem.h"
#include "dspic33ak_uart.h"
#include "nora_tick_timer.h"

//-----------------------------------------------------------
// Wire constants
//-----------------------------------------------------------
#define XMODEM_UART         DSPIC33AK_UART_INST_1   // console UART

#define XM_SOH              0x01u   // 128-byte data block follows
#define XM_STX              0x02u   // 1024-byte data block follows
#define XM_EOT              0x04u   // end of transmission
#define XM_ACK              0x06u   // block accepted
#define XM_NAK              0x15u   // block rejected / resend
#define XM_CAN              0x18u   // cancel (two in a row = abort)
#define XM_CRCCHR           0x43u   // 'C' -- receiver requests CRC mode

#define XM_BLK_128          128u
#define XM_BLK_1024         1024u
#define XM_BLK_MAX          XM_BLK_1024

//-----------------------------------------------------------
// Timing / retry budget (ms via GetTicks)
//-----------------------------------------------------------
#define XM_CHAR_TIMEOUT_MS  1000u   // gap allowed between bytes inside a block
#define XM_HANDSHAKE_MS     3000u   // wait after each 'C' for the first header
#define XM_HANDSHAKE_TRIES  10u     // number of 'C' kicks before giving up
#define XM_MAX_ERRORS       10u     // consecutive block errors before aborting

//-----------------------------------------------------------
// One 1024-byte scratch block (module-static: keeps it off the stack)
//-----------------------------------------------------------
static uint8_t s_blk[XM_BLK_MAX];

//-----------------------------------------------------------
// CRC-16-CCITT (XMODEM): poly 0x1021, init 0x0000, MSB-first, no final xor.
//-----------------------------------------------------------
uint16_t xmodem_crc16_cont( uint16_t crc, const uint8_t* data, uint16_t len )
{
    uint16_t i;
    for ( i = 0u; i < len; i++ )
    {
        uint8_t j;
        crc ^= (uint16_t)( (uint16_t)data[i] << 8 );
        for ( j = 0u; j < 8u; j++ )
        {
            if ( ( crc & 0x8000u ) != 0u )
            {
                crc = (uint16_t)( (uint16_t)( crc << 1 ) ^ 0x1021u );
            }
            else
            {
                crc = (uint16_t)( crc << 1 );
            }
        }
    }
    return crc;
}

uint16_t xmodem_crc16( const uint8_t* data, uint16_t len )
{
    return xmodem_crc16_cont( 0u, data, len );
}

//-----------------------------------------------------------
// UART helpers
//-----------------------------------------------------------
static void xm_send( uint8_t b )
{
    (void)dspic33ak_uart_write_byte( XMODEM_UART, b );
}

// Read one byte, waiting up to timeout_ms. true = got a byte, false = timeout.
static bool xm_read( uint8_t* b, uint32_t timeout_ms )
{
    uint32_t start = nora_tick_timer_get_ms();
    for ( ;; )
    {
        if ( dspic33ak_uart_read_byte( XMODEM_UART, b ) == DSPIC33AK_UART_OK )
        {
            return true;
        }
        if ( (uint32_t)( nora_tick_timer_get_ms() - start ) >= timeout_ms )
        {
            return false;
        }
    }
}

// XMODEM purge: drain the line until it has been idle for one char-timeout, so a
// following NAK re-synchronises the sender on a clean block boundary.
static void xm_purge( void )
{
    uint8_t b;
    while ( xm_read( &b, XM_CHAR_TIMEOUT_MS ) )
    {
        /* discard */
    }
}

static void xm_cancel( void )
{
    xm_send( XM_CAN );
    xm_send( XM_CAN );
    xm_send( XM_CAN );
}

//-----------------------------------------------------------
// Receiver
//-----------------------------------------------------------
xmodem_status_t xmodem_receive( xmodem_sink_fn sink, void* ctx, uint32_t* bytes_received )
{
    uint8_t  expected = 1u;     // next in-order block number (wraps mod 256)
    uint32_t off      = 0u;     // image offset of the next block
    uint32_t got      = 0u;     // total payload bytes delivered
    uint32_t errors   = 0u;     // consecutive error counter
    bool     started  = false;  // at least one data block accepted
    bool     have_hs  = false;  // `hs` already holds an unconsumed header
    uint8_t  hs       = 0u;     // current header byte

    if ( sink == NULL )
    {
        return XMODEM_ERR_SINK;
    }

    dspic33ak_uart_rx_flush( XMODEM_UART );

    // Handshake: kick the sender with 'C' (CRC mode) until the first header
    // arrives. CRC only -- no checksum-mode fallback.
    {
        uint32_t tries;
        for ( tries = 0u; ( tries < XM_HANDSHAKE_TRIES ) && !have_hs; tries++ )
        {
            xm_send( XM_CRCCHR );
            if ( xm_read( &hs, XM_HANDSHAKE_MS ) )
            {
                have_hs = true;
            }
        }
        if ( !have_hs )
        {
            return XMODEM_ERR_TIMEOUT;
        }
    }

    for ( ;; )
    {
        uint16_t blklen;
        uint8_t  bnum, bnumc;
        uint8_t  crchi, crclo;
        uint16_t crc_rx, crc_calc;
        uint16_t i;
        bool     read_ok;

        // Obtain the next header (unless the handshake / a re-check already has one).
        if ( !have_hs )
        {
            if ( !xm_read( &hs, XM_CHAR_TIMEOUT_MS ) )
            {
                if ( ++errors > XM_MAX_ERRORS ) { xm_cancel(); return XMODEM_ERR_TIMEOUT; }
                xm_send( XM_NAK );
                continue;
            }
        }
        have_hs = false;

        if ( hs == XM_EOT )
        {
            xm_send( XM_ACK );
            if ( bytes_received != NULL ) { *bytes_received = got; }
            return started ? XMODEM_OK : XMODEM_ERR_TIMEOUT;
        }
        if ( hs == XM_CAN )
        {
            uint8_t b2;
            if ( xm_read( &b2, XM_CHAR_TIMEOUT_MS ) && ( b2 == XM_CAN ) )
            {
                return XMODEM_ERR_CANCELLED;   // sender aborted
            }
            if ( ++errors > XM_MAX_ERRORS ) { xm_cancel(); return XMODEM_ERR_CANCELLED; }
            xm_purge();
            xm_send( XM_NAK );
            continue;
        }
        if      ( hs == XM_SOH ) { blklen = XM_BLK_128; }
        else if ( hs == XM_STX ) { blklen = XM_BLK_1024; }
        else
        {
            // Garbage where a header was expected.
            if ( ++errors > XM_MAX_ERRORS ) { xm_cancel(); return XMODEM_ERR_SYNC; }
            xm_purge();
            xm_send( XM_NAK );
            continue;
        }

        // Block number + complement, payload, CRC-16 (hi, lo).
        read_ok = xm_read( &bnum,  XM_CHAR_TIMEOUT_MS )
               && xm_read( &bnumc, XM_CHAR_TIMEOUT_MS );
        for ( i = 0u; read_ok && ( i < blklen ); i++ )
        {
            read_ok = xm_read( &s_blk[i], XM_CHAR_TIMEOUT_MS );
        }
        read_ok = read_ok
               && xm_read( &crchi, XM_CHAR_TIMEOUT_MS )
               && xm_read( &crclo, XM_CHAR_TIMEOUT_MS );

        if ( !read_ok )
        {
            if ( ++errors > XM_MAX_ERRORS ) { xm_cancel(); return XMODEM_ERR_TIMEOUT; }
            xm_purge();
            xm_send( XM_NAK );
            continue;
        }

        crc_rx   = (uint16_t)( ( (uint16_t)crchi << 8 ) | crclo );
        crc_calc = xmodem_crc16( s_blk, blklen );

        if ( ( (uint8_t)( bnum ^ bnumc ) != 0xFFu ) || ( crc_rx != crc_calc ) )
        {
            if ( ++errors > XM_MAX_ERRORS ) { xm_cancel(); return XMODEM_ERR_SYNC; }
            xm_purge();
            xm_send( XM_NAK );
            continue;
        }

        if ( bnum == (uint8_t)( expected - 1u ) )
        {
            // Duplicate of the previous block (our ACK was lost) -- re-ACK, no deliver.
            xm_send( XM_ACK );
            errors = 0u;
            continue;
        }
        if ( bnum != expected )
        {
            xm_cancel();                       // out of sequence -- unrecoverable
            return XMODEM_ERR_SYNC;
        }

        // Good, in-order block.
        if ( sink( off, s_blk, blklen, ctx ) != 0 )
        {
            xm_cancel();
            return XMODEM_ERR_SINK;
        }
        off      += blklen;
        got      += blklen;
        expected  = (uint8_t)( expected + 1u );
        started   = true;
        errors    = 0u;
        xm_send( XM_ACK );
    }
}
