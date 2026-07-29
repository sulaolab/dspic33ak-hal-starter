#include "dspic33ak_nvm.h"

#include <xc.h>

//===========================================================
// NVMCON.NVMOP operation codes (DS70005591C, section 7.2.1 / 7.3.2.2).
//   0b0001 Word program   (data source: NVMDATA0..3)
//   0b0010 Row  program   (data source: RAM buffer at NVMSRCADR)
//   0b0011 Page erase      (8 rows)
//===========================================================
#define NVMOP_WORD_PROGRAM (0x1U)
#define NVMOP_ROW_PROGRAM  (0x2U)
#define NVMOP_PAGE_ERASE   (0x3U)

// WREC captured from the last operation (0 == success). Diagnostics only.
static uint8_t s_last_wrec = 0U;

//-----------------------------------------------------------
// Run one program/erase operation to completion.
//
// Precondition: NVMADR (+ NVMDATA0..3 or NVMSRCADR) already loaded for `op`.
// Sequence per DS70005591C 7.3.2.2: clear stale WRERR, select NVMOP, WREN=1,
// set WR, wait for hardware to clear WR (the CPU stalls anyway), WREN=0, then
// test WRERR. Returns OK when WRERR == 0.
//-----------------------------------------------------------
static dspic33ak_nvm_status_t nvm_execute(uint8_t op)
{
    if (NVMCONbits.LOCK != 0U)
    {
        return DSPIC33AK_NVM_ERR_LOCKED;
    }

    NVMCONbits.WRERR = 0U;    // clear stale error status before starting
    NVMCONbits.NVMOP = op;
    NVMCONbits.WREN  = 1U;    // enable program/erase

    NVMCONbits.WR    = 1U;    // start; CPU stalls here until the op completes
    while (NVMCONbits.WR != 0U)
    {
        // Hardware clears WR on completion. The CPU is stalled during the
        // operation, so this loop is a robustness belt-and-braces guard.
    }

    NVMCONbits.WREN = 0U;     // disable further writes

    // A program/erase operation can leave the program-memory read buffer
    // holding data fetched before the modification. DS70005591C 7.3.1
    // explicitly recommends clearing DRBV after every program/erase or a
    // following direct read may return that stale line (most visibly, the
    // pre-programmed all-ones BTSEQ word). Writing zero invalidates the buffer.
    NVMCONbits.DRBV = 0U;

    s_last_wrec = (uint8_t)NVMCONbits.WREC;

    return (NVMCONbits.WRERR != 0U) ? DSPIC33AK_NVM_ERR_WRERR : DSPIC33AK_NVM_OK;
}

bool DSPIC33AK_NVM_IsPartition2Active(void)
{
    return (NVMCONbits.P2ACTIV != 0U);
}

dspic33ak_nvm_status_t DSPIC33AK_NVM_PageErase(uint32_t page_addr)
{
    if (!DSPIC33AK_NVM_IsPageAligned(page_addr))
    {
        return DSPIC33AK_NVM_ERR_ARG;
    }

    // Any address within the target page selects that page for erase.
    NVMADR = page_addr;

    return nvm_execute(NVMOP_PAGE_ERASE);
}

dspic33ak_nvm_status_t DSPIC33AK_NVM_WordProgram(uint32_t word_addr, const uint32_t data[DSPIC33AK_NVM_U32_PER_WORD])
{
    if ((data == NULL) || !DSPIC33AK_NVM_IsWordAligned(word_addr))
    {
        return DSPIC33AK_NVM_ERR_ARG;
    }

    // Load the 128-bit word to program. NVMDATA0 = bits 31:0 ... NVMDATA3 = 127:96.
    NVMDATA0 = data[0];
    NVMDATA1 = data[1];
    NVMDATA2 = data[2];
    NVMDATA3 = data[3];

    NVMADR = word_addr;       // NVMADR[3:0] == 0 forces Flash-word alignment

    return nvm_execute(NVMOP_WORD_PROGRAM);
}

dspic33ak_nvm_status_t DSPIC33AK_NVM_RowProgram(uint32_t row_addr, const uint32_t *ram_src)
{
    if ((ram_src == NULL) || !DSPIC33AK_NVM_IsRowAligned(row_addr))
    {
        return DSPIC33AK_NVM_ERR_ARG;
    }

    // NVMSRCADR takes the data-RAM byte address of the first word of the row.
    // SRCADR[23:2] is used, so the buffer must be 4-byte aligned.
    if (((uint32_t)(uintptr_t)ram_src & 0x3U) != 0U)
    {
        return DSPIC33AK_NVM_ERR_ARG;
    }

    NVMSRCADR = (uint32_t)(uintptr_t)ram_src;
    NVMADR    = row_addr;

    return nvm_execute(NVMOP_ROW_PROGRAM);
}

dspic33ak_nvm_status_t DSPIC33AK_NVM_ReadWord(uint32_t word_addr, uint32_t out[DSPIC33AK_NVM_U32_PER_WORD])
{
    const volatile uint32_t *source;
    uint32_t index;

    if ((out == NULL) || !DSPIC33AK_NVM_IsWordAligned(word_addr))
    {
        return DSPIC33AK_NVM_ERR_ARG;
    }

    // dsPIC33A has a unified/linear address space, so program memory is read
    // with a plain volatile pointer -- no PSV / table-read setup.
    source = (const volatile uint32_t *)(uintptr_t)word_addr;
    for (index = 0U; index < DSPIC33AK_NVM_U32_PER_WORD; index++)
    {
        out[index] = source[index];
    }

    return DSPIC33AK_NVM_OK;
}

// NOTE: `expect` MUST point into data RAM, not program flash. This core takes an
// address-error trap on a runtime data-pointer read of a flash-resident object,
// so comparing flash against another flash location is not supported. In the
// updater `expect` is always the received/source block in RAM, which matches.
dspic33ak_nvm_status_t DSPIC33AK_NVM_Verify(uint32_t flash_addr, const void *expect, uint32_t len_bytes)
{
    const volatile uint32_t *flash;
    const uint32_t *want;
    uint32_t count;
    uint32_t index;

    if ((expect == NULL) || !DSPIC33AK_NVM_IsWordAligned(flash_addr) ||
        ((len_bytes & (DSPIC33AK_NVM_WORD_BYTES - 1U)) != 0U))
    {
        return DSPIC33AK_NVM_ERR_ARG;
    }

    flash = (const volatile uint32_t *)(uintptr_t)flash_addr;
    want  = (const uint32_t *)expect;
    count = len_bytes / 4U;

    for (index = 0U; index < count; index++)
    {
        if (flash[index] != want[index])
        {
            return DSPIC33AK_NVM_ERR_VERIFY;
        }
    }

    return DSPIC33AK_NVM_OK;
}

uint8_t DSPIC33AK_NVM_LastWrec(void)
{
    return s_last_wrec;
}
