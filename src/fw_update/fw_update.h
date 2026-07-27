#ifndef FW_UPDATE_H
#define FW_UPDATE_H

//===========================================================
// fw_update.{c,h}
//
// Serial dual-partition update orchestrator. Receives a firmware image over
// XMODEM-CRC (UART1) and programs it into the INACTIVE flash partition, then
// reads it back and CRC-checks it. It NEVER touches the active partition (the
// code currently executing) nor the partition's last 128-bit word (BTSEQ) --
// committing the swap (BTSEQ stamp + reset) is handled separately by fw_commit().
//
// Data flow:  xmodem_receive() -> fw_sink() -> 512-byte row buffer
//             -> lazy PageErase + RowProgram + Verify (inactive alias)
//             -> running CRC-16 over programmed rows
//             -> after EOT, ReadWord the programmed span -> read-back CRC.
// PASS iff readback_crc == img_crc and no NVM/overflow error occurred.
//
// Blocking: runs to completion inside the calling console verb. The foreground
// application loop is paused; this routine owns UART1 + NVM while active.
//===========================================================

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    FW_UPDATE_OK = 0,
    FW_UPDATE_ERR_XMODEM,     // transfer failed (timeout / cancel / sync / nothing received)
    FW_UPDATE_ERR_NVM,        // flash erase / program / verify error (see last_wrec)
    FW_UPDATE_ERR_OVERFLOW,   // image would exceed the usable partition size
    FW_UPDATE_ERR_IMAGE_FORMAT, // missing/invalid DBFW package trailer
    FW_UPDATE_ERR_IMAGE_CRC,  // package payload CRC/complement mismatch
    FW_UPDATE_ERR_READBACK    // post-program read-back CRC mismatch
} fw_update_status_t;

typedef struct
{
    fw_update_status_t status;
    uint32_t bytes_rx;         // payload bytes received (block-granular, incl. padding)
    uint32_t pages_erased;     // 4 KB pages erased in the inactive partition
    uint32_t rows_programmed;  // 512-byte rows programmed
    uint16_t img_crc;          // CRC-16 over the programmed span (rows, 0xFF-padded tail)
    uint16_t readback_crc;     // CRC-16 over the same span read back from flash
    uint16_t package_crc;      // DBFW trailer CRC-16 over bytes preceding the trailer
    uint8_t  last_wrec;        // NVM WREC after the last flash op (0 = clean)
} fw_update_result_t;

// Receive an image over XMODEM-CRC (UART1) into the inactive partition, program
// + verify it row by row, then read it back and CRC-check. Blocking. Result in
// *out (may be NULL). Returns out->status.
fw_update_status_t fw_update_receive( fw_update_result_t* out );

// Read-only UI check for whether *fca5 is currently armed.
bool fw_update_receive_verified( void );

// One-shot commit gate. Returns true exactly once after a successful receive and
// clears the gate before the caller performs any BTSEQ operation. Consequently a
// failed commit cannot be retried against a target that rollback may have changed;
// the operator must run *fua5 again. A failed/absent receive returns false.
bool fw_update_claim_verified_receive( void );

#ifdef __cplusplus
}
#endif

#endif // FW_UPDATE_H
