//===========================================================
// fw_btseq.c -- BTSEQ / partition manager + commit + swap (see fw_btseq.h)
//===========================================================

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "fw_btseq.h"
#include "fw_update.h"
#include "fw_uca.h"
#include "nora_nvm.h"
#include "dspic33ak_uart.h"

//-----------------------------------------------------------
// Encode / decode.
//-----------------------------------------------------------
uint32_t fw_btseq_encode(uint16_t seq)
{
    uint32_t s  = (uint32_t)seq & FW_BTSEQ_MASK;
    uint32_t ic = (~s) & FW_BTSEQ_MASK;         // one's-complement of the 12-bit value
    return (ic << 12) | s;                      // reserved [31:24] stay 0 (masked form)
}

bool fw_btseq_decode_valid(uint32_t low, uint16_t *seq_out)
{
    uint32_t value = low & FW_BTSEQ_MASK;
    uint32_t ic    = (low >> 12) & FW_BTSEQ_MASK;
    bool     valid = ( value == ((~ic) & FW_BTSEQ_MASK) );
    if ( seq_out != NULL )
    {
        *seq_out = (uint16_t)value;
    }
    // A DED ECC error would trap on the ReadWord itself, not reach here; software
    // cannot separately test it. Cross-partition reads of programmed flash are
    // already proven not to trap by the receive/read-back path.
    return valid;
}

uint16_t fw_btseq_read_seq(uint32_t word_adr)
{
    uint32_t w[NORA_NVM_U32_PER_WORD] = { 0u, 0u, 0u, 0u };
    uint16_t seq = FW_BTSEQ_BLANK;

    if ( NORA_NVM_ReadWord(word_adr, w) != NORA_NVM_OK )
    {
        return FW_BTSEQ_BLANK;
    }
    if ( !fw_btseq_decode_valid(w[0], &seq) )
    {
        return FW_BTSEQ_BLANK;                  // blank/invalid -> evaluates highest
    }
    return seq;
}

//-----------------------------------------------------------
// Drain UART1 TX so the final "resetting" line is fully on the wire before the
// core resets. Bounded spin (never hangs even if the UART is wedged).
//-----------------------------------------------------------
static void fw_drain_uart1(void)
{
    uint32_t guard = 0u;
    while ( !dspic33ak_uart_tx_done(DSPIC33AK_UART_INST_1) && (guard < 2000000u) )
    {
        guard++;
    }
}

//-----------------------------------------------------------
// Software device reset -- re-evaluates BTSEQ boot selection and boots the lowest
// valid partition (DS70005591C section 7.4.3.6 step 4a). XC-DSC v3.21/v3.31.01
// expose NO reset builtin (only __builtin_software_breakpoint), so the core `reset`
// instruction is the mechanism; RCON.SWR is its power-on readback proof.
//-----------------------------------------------------------
static void fw_sys_reset(void)
{
    fw_drain_uart1();
    __asm__ volatile ("reset");
    for ( ;; ) { }                              // unreachable
}

// Fail-closed recovery after a BTSEQ program/verify error. A failed WordProgram
// can still have changed enough bits to form a valid, lower sequence number. In
// that case merely returning without reset is not sufficient: a later external
// reset could select the target bank. Erasing its final page invalidates BTSEQ
// (and, if present, sacrifices only the uncommitted target image) while leaving
// the currently executing partition untouched.
static bool fw_invalidate_inactive_btseq(void)
{
    uint32_t final_page = FW_BTSEQ_INACTIVE_ADR &
                          ~(uint32_t)(NORA_NVM_PAGE_BYTES - 1u);
    uint32_t rb[NORA_NVM_U32_PER_WORD] = { 0u, 0u, 0u, 0u };

    if (NORA_NVM_PageErase(final_page) != NORA_NVM_OK)
    {
        return false;
    }
    if (NORA_NVM_ReadWord(FW_BTSEQ_INACTIVE_ADR, rb) != NORA_NVM_OK)
    {
        return false;
    }
    return (rb[0] == 0xFFFFFFFFu) && (rb[1] == 0xFFFFFFFFu) &&
           (rb[2] == 0xFFFFFFFFu) && (rb[3] == 0xFFFFFFFFu);
}

//-----------------------------------------------------------
// Commit the inactive partition and swap to it (see fw_btseq.h). Returns only on
// failure; on success it resets the device and never returns.
//-----------------------------------------------------------
fw_commit_status_t fw_commit(void)
{
    uint16_t active;
    uint16_t next;
    uint32_t enc;
    uint32_t data[NORA_NVM_U32_PER_WORD];
    uint32_t rb[NORA_NVM_U32_PER_WORD] = { 0u, 0u, 0u, 0u };
    uint16_t rb_seq = 0u;
    bool     inactive_blank;
    nora_nvm_status_t s;

    // 1) Gate: only commit a partition a *verified receive* just filled. A selftest
    //    pattern (non-bootable) or a failed/absent receive must never be committed.
    if ( !fw_update_claim_verified_receive() )
    {
        return FW_COMMIT_ERR_NOT_VERIFIED;
    }

    // 1b) Gate: the partition we are about to boot must have a provisioned UCA.
    //     If the INACTIVE partition's UCA has ALTI2C2 OFF (or an erased/blank UCA),
    //     committing the swap would boot with different board pin configuration.
    //     Refuse -- the currently-active partition stays bootable. This
    //     This gate is mandatory; no debug bypass is exposed in the beginner API.
    if ( fw_uca_validate_inactive(0) != FW_UCA_OK )
    {
        return FW_COMMIT_ERR_UCA;
    }

    // 2) Lowest valid BTSEQ wins; the next generation is (active - 1).
    active = fw_btseq_read_active_seq();         // blank/invalid -> 0xFFF
    if ( active == 0u )
    {
        return FW_COMMIT_ERR_WRAP;               // 4096 generations exhausted
    }
    next = (uint16_t)((active - 1u) & FW_BTSEQ_MASK);
    enc  = fw_btseq_encode(next);

    // 3) The BTSEQ word must be VIRGIN (erased) before WordProgram (one-shot ECC).
    //    Read the inactive word:
    //      * all-ones  -> virgin; program directly. This holds whether the final
    //        page is otherwise blank or already carries image rows from a large
    //        receive -- such a receive erased the whole final page, leaving this
    //        word blank while its own image bytes sit below it (untouched here).
    //      * non-blank -> a stale BTSEQ from a previous commit. That can only occur
    //        when the current receive did NOT reach the final page (a receive that
    //        does erases the page and blanks this word). So no current image byte
    //        lives in the final page and re-erasing it to re-virginize the word is
    //        safe -- it cannot destroy any verified image data.
    (void)NORA_NVM_ReadWord(FW_BTSEQ_INACTIVE_ADR, rb);
    inactive_blank = (rb[0] == 0xFFFFFFFFu) && (rb[1] == 0xFFFFFFFFu) &&
                     (rb[2] == 0xFFFFFFFFu) && (rb[3] == 0xFFFFFFFFu);
    if ( !inactive_blank )
    {
        uint32_t final_page = FW_BTSEQ_INACTIVE_ADR & ~(uint32_t)(NORA_NVM_PAGE_BYTES - 1u);
        s = NORA_NVM_PageErase(final_page);
        if ( s != NORA_NVM_OK )
        {
            return FW_COMMIT_ERR_NVM;
        }
    }

    // 4) Program the inactive BTSEQ word: {enc, 0, 0, 0} (reserved [127:24] = 0).
    data[0] = enc;
    data[1] = 0u;
    data[2] = 0u;
    data[3] = 0u;
    s = NORA_NVM_WordProgram(FW_BTSEQ_INACTIVE_ADR, data);
    if ( s != NORA_NVM_OK )
    {
        printf(" BTSEQ program failed: status=%u wrec=%02X\r\n",
               (unsigned)s, (unsigned)NORA_NVM_LastWrec());
        return fw_invalidate_inactive_btseq() ? FW_COMMIT_ERR_NVM :
                                                FW_COMMIT_ERR_ROLLBACK;
    }

    // 5) Read-back verify the stamped word before handing the board to it. On any
    //    mismatch, DO NOT reset -- the currently-active partition still boots.
    s = NORA_NVM_ReadWord(FW_BTSEQ_INACTIVE_ADR, rb);
    if ( s != NORA_NVM_OK )
    {
        printf(" BTSEQ read-back failed: status=%u wrec=%02X\r\n",
               (unsigned)s, (unsigned)NORA_NVM_LastWrec());
        return fw_invalidate_inactive_btseq() ? FW_COMMIT_ERR_VERIFY :
                                                FW_COMMIT_ERR_ROLLBACK;
    }
    if ( (rb[0] != enc) || (rb[1] != 0u) || (rb[2] != 0u) || (rb[3] != 0u) )
    {
        printf(" BTSEQ read-back mismatch: want=%08lX/00000000/00000000/00000000"
               " got=%08lX/%08lX/%08lX/%08lX wrec=%02X\r\n",
               (unsigned long)enc,
               (unsigned long)rb[0], (unsigned long)rb[1],
               (unsigned long)rb[2], (unsigned long)rb[3],
               (unsigned)NORA_NVM_LastWrec());
        return fw_invalidate_inactive_btseq() ? FW_COMMIT_ERR_VERIFY :
                                                FW_COMMIT_ERR_ROLLBACK;
    }
    if ( !fw_btseq_decode_valid(rb[0], &rb_seq) || (rb_seq != next) )
    {
        printf(" BTSEQ decode mismatch: word=%08lX decoded=%03X expected=%03X\r\n",
               (unsigned long)rb[0], (unsigned)rb_seq, (unsigned)next);
        return fw_invalidate_inactive_btseq() ? FW_COMMIT_ERR_VERIFY :
                                                FW_COMMIT_ERR_ROLLBACK;
    }

    // 6) Swap: the inactive partition now holds the lowest valid BTSEQ. A plain
    //    device reset re-evaluates boot selection and boots it. Never returns.
    printf(" \"*fca5\" committed next=0x%03X @0x%06lX -- resetting to swap\n",
           (unsigned)next, (unsigned long)FW_BTSEQ_INACTIVE_ADR);
    fw_sys_reset();

    return FW_COMMIT_OK;                         // unreachable
}
