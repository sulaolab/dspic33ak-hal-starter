//===========================================================
// fw_uca.c -- per-partition UCA validation (see fw_uca.h)
//===========================================================

#include <stdint.h>
#include <stdbool.h>

#include "fw_uca.h"
#include "dspic33ak_nvm.h"

//-----------------------------------------------------------
// Read one 32-bit config word through a volatile absolute pointer.
//
// A config fuse is a small NV segment; reading it as a 128-bit NVM word would run
// past the segment into an unimplemented hole. This mirrors fw_console.c's ?fb
// FBOOT read and hal_udid's uniqueID read, both HW-verified. The returned value
// lands in a CPU register / RAM scalar, so any later comparison is RAM-vs-RAM
// (no flash-resident comparand -> no address-error trap on this core).
//-----------------------------------------------------------
static uint32_t fw_uca_rd(uint32_t addr)
{
    const volatile uint32_t *p = (const volatile uint32_t *)(uintptr_t)addr;
    return *p;
}

bool fw_uca_read(bool p2, fw_uca_snapshot_t *snap)
{
    uint32_t main_base   = p2 ? FW_UCA_P2_MAIN   : FW_UCA_P1_MAIN;
    uint32_t backup_base = p2 ? FW_UCA_P2_BACKUP : FW_UCA_P1_BACKUP;

    if ( snap == 0 ) { return false; }

    snap->fcp_main       = fw_uca_rd(main_base   + FW_UCA_OFF_FCP);
    snap->fcp_backup     = fw_uca_rd(backup_base + FW_UCA_OFF_FCP);
    snap->ficd_main      = fw_uca_rd(main_base   + FW_UCA_OFF_FICD);
    snap->ficd_backup    = fw_uca_rd(backup_base + FW_UCA_OFF_FICD);
    snap->fdevopt_main   = fw_uca_rd(main_base   + FW_UCA_OFF_FDEVOPT);
    snap->fdevopt_backup = fw_uca_rd(backup_base + FW_UCA_OFF_FDEVOPT);
    snap->fwdt_main      = fw_uca_rd(main_base   + FW_UCA_OFF_FWDT);
    snap->fwdt_backup    = fw_uca_rd(backup_base + FW_UCA_OFF_FWDT);
    return true;
}

fw_uca_status_t fw_uca_validate(bool p2, fw_uca_report_t *report)
{
    fw_uca_snapshot_t snap;
    fw_uca_status_t   status;
    bool alti2c2_on;
    bool bootswp_enabled;
    bool fdevopt_backup_ok;
    bool ficd_backup_ok;
    bool fcp_erased_ok;
    bool fwdt_erased_ok;

    (void)fw_uca_read(p2, &snap);

    // All comparands are RAM scalars (the mask literals and the read-back words).
    alti2c2_on        = ((snap.fdevopt_main & FW_UCA_FDEVOPT_ALTI2C2) == 0U);
    bootswp_enabled   = ((snap.ficd_main    & FW_UCA_FICD_NOBTSWP)    == 0U);
    fdevopt_backup_ok = (snap.fdevopt_main == snap.fdevopt_backup);
    ficd_backup_ok    = (snap.ficd_main    == snap.ficd_backup);
    fcp_erased_ok     = (snap.fcp_main == FW_UCA_WORD_ERASED) &&
                        (snap.fcp_backup == FW_UCA_WORD_ERASED);
    fwdt_erased_ok    = (snap.fwdt_main == FW_UCA_WORD_ERASED) &&
                        (snap.fwdt_backup == FW_UCA_WORD_ERASED);

    // Both functional policies are required for this dual-partition build. The
    // DFP names FICD bit15 NOBTSWP, but its cleared state enables BOOTSWP.
    if ( !alti2c2_on || !bootswp_enabled || !fcp_erased_ok || !fwdt_erased_ok )
    {
        status = FW_UCA_ERR_MISMATCH;
    }
    else if ( !fdevopt_backup_ok || !ficd_backup_ok )
    {
        status = FW_UCA_ERR_BACKUP;
    }
    else
    {
        status = FW_UCA_OK;
    }

    if ( report != 0 )
    {
        report->snap              = snap;
        report->p2                = p2;
        report->alti2c2_on        = alti2c2_on;
        report->bootswp_enabled   = bootswp_enabled;
        report->fdevopt_backup_ok = fdevopt_backup_ok;
        report->ficd_backup_ok    = ficd_backup_ok;
        report->fcp_erased_ok     = fcp_erased_ok;
        report->fwdt_erased_ok    = fwdt_erased_ok;
        report->status            = status;
    }
    return status;
}

fw_uca_status_t fw_uca_validate_active(fw_uca_report_t *report)
{
    // Active partition = P2 iff P2ACTIV. NOT the program-flash |0x400000 alias:
    // the UCA is at a fixed physical address, chosen by which partition booted.
    return fw_uca_validate(DSPIC33AK_NVM_IsPartition2Active(), report);
}

fw_uca_status_t fw_uca_validate_inactive(fw_uca_report_t *report)
{
    return fw_uca_validate(!DSPIC33AK_NVM_IsPartition2Active(), report);
}

const char *fw_uca_status_name(fw_uca_status_t s)
{
    switch ( s )
    {
    case FW_UCA_OK:           return "OK";
    case FW_UCA_ERR_MISMATCH: return "MISMATCH";
    case FW_UCA_ERR_BACKUP:   return "BACKUP";
    case FW_UCA_ERR_READ:     return "READ";
    default:                  return "?";
    }
}
