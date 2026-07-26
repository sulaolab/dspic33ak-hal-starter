# Build Tools

Command-line helpers for building, flashing, and resetting this MPLAB X firmware
project without opening the IDE.

For the complete first-time provisioning and Tera Term update procedure, see
the [Dual-Partition Firmware Update Guide](../docs/dual_partition_update.md).

> [!TIP]
> Fresh clones can flash/reset immediately. The PKOB4 helper executables are
> vendored under [`_flash_reset_tools`](./_flash_reset_tools/), and
> `flashauto.ps1` uses them automatically.

## Scripts

| Script | Purpose |
| --- | --- |
| `build.ps1` | Build, provision/verify both partitions' UCA, and generate `reflash_image.bin`. |
| `provision.ps1` | Regenerate and independently verify the P1+P2 UCA bundle. |
| `flashauto.ps1` | Flash the verified bundle through PKOB4, then reset. |

## Common Commands

Run these from the repository root:

```powershell
# Incremental build
.\buildtools\build.ps1

# Full clean-build
.\buildtools\build.ps1 -Full

# Clean outputs only
.\buildtools\build.ps1 -Clean

# Regenerate MPLAB X makefiles only
.\buildtools\build.ps1 -Generate

# Flash the verified dual-partition bundle, then reset
.\buildtools\flashauto.ps1

# Reset only
.\buildtools\flashauto.ps1 -Reset

# Clean Boost Java state before the reset step
.\buildtools\flashauto.ps1 -CleanJava

# List connected PKOB4 serial numbers
.\buildtools\flashauto.ps1 -List

# Select a board explicitly
.\buildtools\flashauto.ps1 -Serial 020085204RYN000318
```

## Flash/Reset Tool Lookup

The default flash path is
`firmware.X/dist/dsPIC33AK512/production/firmware.X.production.bundle.hex`.
`flashauto.ps1` also requires its `.verify_report.txt` to contain `PASS` and a
matching bundle SHA-256; it refuses an unverified or stale-paired initial image.
`build.ps1` creates both plus
`reflash_image.bin` automatically. The binary carries a small DBFW project-ID and
CRC trailer checked by the firmware before `*fca5` is enabled. Python 3 is
required for these dependency-free Intel HEX tools.

`flashauto.ps1` looks for `flash_pkob4.exe` and `reset_pkob4.exe` in this order:

1. `-ToolsDir`
2. `FLASH_RESET_TOOLS`
3. `buildtools\_flash_reset_tools`
4. legacy `.\_flash_reset_tools`
5. legacy `..\_flash_reset_tools`

The vendored tools are self-contained Windows x64 executables. They do not require
a separate .NET runtime. MPLAB X must be installed; the tools auto-detect the
newest MPLAB X installation and use its bundled `mdb.bat`, `ipecmdboost.jar`, and
Java runtime.

For lower-level tool details, see [`_flash_reset_tools/README.md`](./_flash_reset_tools/README.md).

## Notes

- If more than one PKOB4 board is connected, pass `-Serial` explicitly.
- Reset re-enumerates PKOB4 USB, so a CDC console may briefly disconnect.
- Keep MPLAB X IDE / IPE closed while flashing or resetting from these scripts.
