# Build Tools

Command-line helpers for building, flashing, and resetting this MPLAB X firmware
project without opening the IDE.

> [!TIP]
> Fresh clones can flash/reset immediately. The PKOB4 helper executables are
> vendored under [`_flash_reset_tools`](./_flash_reset_tools/), and
> `flashauto.ps1` uses them automatically.

## Scripts

| Script | Purpose |
| --- | --- |
| `build.ps1` | Generate MPLAB X makefiles when needed, then build the production HEX. |
| `flashauto.ps1` | Auto-detect the production HEX and PKOB4 serial, flash the target, then reset it. |

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

# Flash the built HEX, then reset
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
