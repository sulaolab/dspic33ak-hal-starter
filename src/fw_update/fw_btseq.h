#ifndef FW_BTSEQ_H
#define FW_BTSEQ_H

//===========================================================
// fw_btseq.{c,h} -- Boot Sequence Number (BTSEQ) / partition manager.
//
// The last link of the dual-partition update chain: after fw_update has received and
// read-back-verified a firmware image into the INACTIVE partition, this module
// COMMITS the swap -- it stamps the inactive partition's BTSEQ word so that
// partition wins boot selection, then triggers a device reset so the board boots
// the newly-received image.
//
// BTSEQ layout (DS70005591C section 7.4.2.2.1): each 256 KB partition stores a
// 12-bit boot sequence number in the LOW 32-bit sub-word of its LAST 128-bit
// flash word (active 0x83FFF0, inactive alias 0xC3FFF0):
//   [11:0]  = BTSEQ  (the value)
//   [23:12] = IBTSEQ (one's-complement of the 12-bit value; NOT auto-generated)
//   [127:24]= reserved, must read 0
// A partition's BTSEQ is VALID iff [11:0] == ~[23:12] (and no ECC DED). An
// invalid/blank word is treated as 0xFFF (highest). The LOWEST valid sequence
// number wins; a tie defaults to Partition 1. To commit the inactive partition
// we write it (active_seq - 1), which is strictly lower, so it boots after reset.
//
// Encode uses the masked form ((~seq & 0xFFF) << 12) | (seq & 0xFFF). The
// datasheet Example 7-1 form (~(uint16_t)seq << 12) | seq leaves bits [27:24]=0xF;
// the masked form is validity-equivalent (hardware only compares [23:12] vs
// [11:0]) and additionally honours reserved [127:24]=0, so it is preferred.
//
// Swap mechanism = plain software device reset (datasheet section 7.4.3.6 step 4a;
// "default to reset for v1"). BOOTSWP (no-reset) is the documented alternative but
// is not built here (pure A/B needs no separate updater mode).
//===========================================================

#include <stdint.h>
#include <stdbool.h>

#include "dspic33ak_nvm.h"

#ifdef __cplusplus
extern "C" {
#endif

// Partition geometry / BTSEQ word addresses (Flash Dual Partition: 256 KB each).
#define FW_BTSEQ_PARTITION_BYTES  UINT32_C(0x40000)
#define FW_BTSEQ_ACTIVE_ADR       (DSPIC33AK_NVM_ACTIVE_BASE + FW_BTSEQ_PARTITION_BYTES - DSPIC33AK_NVM_WORD_BYTES)   // 0x83FFF0
#define FW_BTSEQ_INACTIVE_ADR     (DSPIC33AK_NVM_TO_INACTIVE(FW_BTSEQ_ACTIVE_ADR))                                    // 0xC3FFF0
#define FW_BTSEQ_MASK             (0x0FFFu)
#define FW_BTSEQ_BLANK            (0x0FFFu)   // value a blank/invalid word evaluates to

typedef enum
{
    FW_COMMIT_OK = 0,             // (never returned on success -- commit resets the device)
    FW_COMMIT_ERR_NOT_VERIFIED,   // no verified receive precedes this commit
    FW_COMMIT_ERR_WRAP,           // active seq == 0 (4096 generations exhausted)
    FW_COMMIT_ERR_NVM,            // erase / program error
    FW_COMMIT_ERR_VERIFY,         // stamped BTSEQ word failed read-back / decode
    FW_COMMIT_ERR_ROLLBACK,       // failed to invalidate a possibly-written BTSEQ
    FW_COMMIT_ERR_UCA             // inactive partition's UCA is not provisioned (would boot broken)
} fw_commit_status_t;

// Encode a 12-bit sequence number into the low BTSEQ sub-word (masked form).
uint32_t fw_btseq_encode(uint16_t seq);

// Decode a low BTSEQ sub-word: returns true iff [11:0]==~[23:12]; *seq_out (may be
// NULL) receives the raw [11:0] value regardless of validity.
bool fw_btseq_decode_valid(uint32_t low, uint16_t *seq_out);

// Read + decode a partition's BTSEQ word. A blank/invalid word evaluates to 0xFFF.
uint16_t fw_btseq_read_seq(uint32_t word_adr);
static inline uint16_t fw_btseq_read_active_seq(void)   { return fw_btseq_read_seq(FW_BTSEQ_ACTIVE_ADR); }
static inline uint16_t fw_btseq_read_inactive_seq(void) { return fw_btseq_read_seq(FW_BTSEQ_INACTIVE_ADR); }

// Stamp the inactive partition's BTSEQ = (active-1) and reset to swap. Requires a
// verified fw_update_receive() first (else FW_COMMIT_ERR_NOT_VERIFIED). The gate
// is consumed by one commit attempt, so any failure requires a fresh receive. On success
// this DOES NOT RETURN (it resets the device); it only returns a fw_commit_status_t
// on failure, leaving the currently-active partition untouched and still bootable.
fw_commit_status_t fw_commit(void);

#ifdef __cplusplus
}
#endif

#endif // FW_BTSEQ_H
