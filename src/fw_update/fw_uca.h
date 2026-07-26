#ifndef FW_UCA_H
#define FW_UCA_H

//===========================================================
// fw_uca.{c,h} -- per-partition User Configuration Area (UCA) validation.
//
// WHY THIS EXISTS
//   In Flash Dual Partition each PHYSICAL partition carries its own copy of the
//   configuration fuses (the "UCA" / Configuration A region) at FIXED physical
//   addresses. Unlike program flash, the UCA is NOT remapped by NVMCON.P2ACTIV:
//   the active partition's fuses are simply the ones the HW loaded into the
//   shadow config registers at reset.
//
//     P1 UCA1: main 0x7F3000, backup 0x7F3800
//     P2 UCA2: main 0x7FB000, backup 0x7FB800
//     word offsets: FCP +0x000, FICD +0x010, FDEVOPT +0x020, FWDT +0x030
//     (shared UCB FBOOT lives at 0x7F40D0 -- NOT part of any partition's UCA)
//
//   A plain PKOB4 flash of the stock production HEX programs ONLY P1's UCA; the
//   P2 UCA stays erased (0xFFFFFFFF). Erased FDEVOPT has ALTI2C2 (bit4) = 1,
//   selecting the wrong I2C2 pins for this board after a swap. The
//   tools/gen_dual_partition_hex.py provisioning step clones the P1 UCA into P2
//   so both partitions boot with identical board configuration. This module is
//   the firmware-side guard used by the boot banner and pre-commit gate.
//
// COUPLING (single source of truth in three places -- keep in sync):
//   src/main.c  `#pragma config`  (FDEVOPT_ALTI2C2=ON, NOBTSWP=ON, BTMODE=DUAL)
//   tools/uca_manifest.py         (host generator/verifier)
//   this header                   (firmware runtime check)
//
// READ RULE (mirrors ?fb / hal_udid): each config word is a small fuse segment,
// so it is read as a SINGLE 32-bit word through a volatile absolute uint32_t*,
// one word at a time -- NOT a 128-bit DSPIC33AK_NVM_ReadWord (that would read
// past the segment into an unimplemented hole). All comparands are RAM scalars
// (this core traps on a runtime data-pointer read of a flash-resident constant).
//===========================================================

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------
// UCA region addresses (per physical partition; fixed, NOT P2ACTIV-remapped).
//-----------------------------------------------------------
#define FW_UCA_P1_MAIN     (0x7F3000UL)
#define FW_UCA_P1_BACKUP   (0x7F3800UL)
#define FW_UCA_P2_MAIN     (0x7FB000UL)
#define FW_UCA_P2_BACKUP   (0x7FB800UL)

#define FW_UCA_OFF_FCP     (0x000UL)
#define FW_UCA_OFF_FICD    (0x010UL)
#define FW_UCA_OFF_FDEVOPT (0x020UL)
#define FW_UCA_OFF_FWDT    (0x030UL)

//-----------------------------------------------------------
// Config-bit masks (VERIFIED vs DFP atdf 1.3.185). A cleared bit = feature ON.
//-----------------------------------------------------------
#define FW_UCA_FDEVOPT_ALTI2C2 (0x10U)      // bit4: 0 => board-required alternate I2C2 pins.
#define FW_UCA_FICD_NOBTSWP    (0x8000U)    // bit15: 0 => NOBTSWP ON (expected for this build).
#define FW_UCA_WORD_ERASED     (0xFFFFFFFFU)

typedef enum
{
    FW_UCA_OK = 0,             // required bits correct (ALTI2C2/NOBTSWP ON), main==backup
    FW_UCA_ERR_MISMATCH,       // a required config bit is wrong (or UCA is erased)
    FW_UCA_ERR_BACKUP,         // main != backup (UCA integrity)
    FW_UCA_ERR_READ            // reserved (volatile-pointer reads do not fail)
} fw_uca_status_t;

// Raw 4 config words, main + backup, for one partition.
typedef struct
{
    uint32_t fcp_main,     fcp_backup;
    uint32_t ficd_main,    ficd_backup;
    uint32_t fdevopt_main, fdevopt_backup;
    uint32_t fwdt_main,    fwdt_backup;
} fw_uca_snapshot_t;

// Decoded validation report for one partition.
typedef struct
{
    fw_uca_snapshot_t snap;
    bool p2;                   // true: this report is for physical Partition 2
    bool alti2c2_on;           // FDEVOPT(main) bit4 == 0
    bool nobtswp_on;           // FICD(main)    bit15 == 0
    bool fdevopt_backup_ok;    // FDEVOPT main == backup
    bool ficd_backup_ok;       // FICD    main == backup
    bool fcp_erased_ok;        // FCP main+backup are both erased (build policy)
    bool fwdt_erased_ok;       // FWDT main+backup are both erased (build policy)
    fw_uca_status_t status;    // overall
} fw_uca_report_t;

// Read a partition's UCA (main+backup) into *snap. `p2`: true -> P2 (0x7FB0xx),
// false -> P1 (0x7F30xx). Always succeeds (volatile-pointer reads). Returns true.
bool fw_uca_read(bool p2, fw_uca_snapshot_t *snap);

// Validate a partition's UCA into *report (may be NULL for status-only). Overall
// status: MISMATCH if ALTI2C2/NOBTSWP is wrong or FCP/FWDT is unexpectedly
// programmed, else BACKUP if FICD/FDEVOPT main!=backup, else OK. No auto-repair.
fw_uca_status_t fw_uca_validate(bool p2, fw_uca_report_t *report);

// Validate the currently-ACTIVE partition's UCA (chosen by P2ACTIV, NOT the
// program-flash |0x400000 alias). Used by the boot banner.
fw_uca_status_t fw_uca_validate_active(fw_uca_report_t *report);

// Validate the currently-INACTIVE partition's UCA. Used by the pre-commit gate so
// a swap is refused if the partition we are about to boot has an unprovisioned UCA.
fw_uca_status_t fw_uca_validate_inactive(fw_uca_report_t *report);

// Short human-readable name for a status (for console/banner printing).
const char *fw_uca_status_name(fw_uca_status_t s);

#ifdef __cplusplus
}
#endif

#endif // FW_UCA_H
