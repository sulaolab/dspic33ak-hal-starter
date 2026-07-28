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
| `switch_config.ps1` | Choose what the next build targets (interactive menu or scripted). |
| `build.ps1` | Build (follows the selection above), provision/verify both partitions' UCA, and generate `reflash_image.bin`. |
| `provision.ps1` | Regenerate and independently verify the P1+P2 UCA bundle plus MPLAB X device/DFP/compiler pins. |
| `flashauto.ps1` | Flash the verified bundle through PKOB4, then reset. |
| `hal_starter_build_state.ps1` | Selection-state helper shared by the three scripts above (not run directly). |

## What Gets Selected

A build target has two parts, and both persist so a bare `build.ps1` /
`flashauto.ps1` follow them (no environment variable is involved).

### 1. MPLAB configuration = device

| Configuration | Device |
| --- | --- |
| `dsPIC33AK512` | `dsPIC33AK512MPS512` |

The catalog's authority is `firmware.X/nbproject/configurations.xml`. There is
only one configuration today; a future board variant is a new `<conf>` entry
there, not a script rewrite. The active configuration is stored the same way
MPLAB X IDE stores it: `nbproject/Makefile-impl.mk` (`DEFAULTCONF=`, tracked,
byte-exact) and `nbproject/private/configurations.xml` (`<defaultConf>`,
untracked, IDE-only).

### 2. `APP_BUILD` variation = which demo variant

| Variation | What it does |
| --- | --- |
| `APP_BUILD_STARTER_DEFAULT` | TDM8 smoke demo (FS_50PCT); all self-tests off (shipped default) |
| `APP_BUILD_TDM_SMOKE_OFF` | TDM smoke demo off; frees MikroBUS-A SPI pins for a Click board |
| `APP_BUILD_TDM_SMOKE_FS_PULSE` | TDM smoke demo with FS_PULSE waveform instead of FS_50PCT |
| `APP_BUILD_TDM_FS_RUNTIME_SWITCH_TEST` | Opt-in FS-pin PPS restore self-test (requires FS_50PCT) |
| `APP_BUILD_UART_ASYNC_SELFTEST` | Opt-in UART1 async TX/RX self-test before the boot banner |
| `APP_BUILD_TDM_NEG_TEST_1LEG` | HAL negative-validation self-test, single-leg matrix; smoke resumes after |
| `APP_BUILD_TDM_NEG_TEST_2LEG` | HAL negative-validation self-test, 2-leg matrix; smoke off |
| `APP_BUILD_CAN_BUS_TEST_ORIGINATOR` | Two-board CAN FD bus test; this board is the ORIGINATOR (id 0x0A0) |
| `APP_BUILD_CAN_BUS_TEST_ECHO` | Two-board CAN FD bus test; this board is the ECHO (id 0x0B0) |

The table's authority is [`src/app/app_build_config.h`](../src/app/app_build_config.h).
`switch_config.ps1` / `build.ps1` read this header at startup to build their
menu/default, so adding a variation there is enough (this Markdown table is
manual documentation only). The selection is stored in
`buildtools/active_build.json` (untracked). If nothing is selected, the build
does not pass `-DAPP_BUILD=` at all and the header's own compile-time default
applies (`APP_BUILD_STARTER_DEFAULT`) -- the same result as a plain MPLAB X IDE
build.

## Selecting: `switch_config.ps1`

No arguments = interactive menu (run from the repository root):

```powershell
.\buildtools\switch_config.ps1
```

```text
MPLAB configurations:
  1) dsPIC33AK512         dsPIC33AK512MPS512  <- active

Select configuration [1-1, Enter = keep dsPIC33AK512, q = quit]:

APP_BUILD variations:
   1) APP_BUILD_STARTER_DEFAULT           TDM8 smoke demo (FS_50PCT); all self-tests off
      <- selected, compile-time default
   2) APP_BUILD_TDM_SMOKE_OFF             TDM smoke demo off; MikroBUS-A SPI pins free for a Click board
   ...
Select APP_BUILD [1-9, Enter = keep APP_BUILD_STARTER_DEFAULT, q = quit]: 2

Active configuration: dsPIC33AK512
Active APP_BUILD:     APP_BUILD_TDM_SMOKE_OFF
```

`Enter` = keep the current value, `q` = cancel without changing anything.

Scripted / one-shot form (does not prompt):

```powershell
# Free the MikroBUS-A SPI pins for a Click board
.\buildtools\switch_config.ps1 -Preset APP_BUILD_TDM_SMOKE_OFF

# Back to the shipped default
.\buildtools\switch_config.ps1 -Preset APP_BUILD_STARTER_DEFAULT

# Show the catalog and the current selection, change nothing
.\buildtools\switch_config.ps1 -List
```

Running with no arguments when stdin has no console (e.g. from CI) fails with
an explicit error instead of hanging; pass `-Preset`/`-Configuration` there.

## Building: `build.ps1`

```powershell
.\buildtools\build.ps1
```

With no arguments this follows the `switch_config.ps1` selection and prints
what it is building:

```text
Configuration: dsPIC33AK512  (dsPIC33AK512MPS512)
APP_BUILD: APP_BUILD_TDM_SMOKE_OFF  [active selection]
```

`[...]` shows where that came from: `[active selection]` (from
`switch_config.ps1`), `[-Preset]` (a one-shot override for this build only), or
`[compile-time default; none selected]`.

| Mode | Behavior |
| --- | --- |
| (none) | Generate makefiles if missing, then build. |
| `-Full` | Generate makefiles, clean, then build. |
| `-Clean` | Remove build outputs only. |
| `-Generate` | Generate MPLAB X makefiles only. |

`-Configuration` / `-Preset` are one-shot overrides for this build only; they do
not change the persisted selection:

```powershell
.\buildtools\build.ps1 -Preset APP_BUILD_CAN_BUS_TEST_ORIGINATOR
.\buildtools\build.ps1 -Full
```

`-Jobs N` sets the parallel job count (default: logical core count, capped at 8).

### Clean-build only when `APP_BUILD` actually changed

All `APP_BUILD` variations for this configuration share one object directory, so
mixing objects from two variations must never happen. `build.ps1` stamps
`firmware.X/build/dsPIC33AK512/.hal_starter_app_build` with what the existing
objects were built with, and only promotes to `-Full` when that changes:

```text
APP_BUILD changed (APP_BUILD_STARTER_DEFAULT -> APP_BUILD_TDM_SMOKE_OFF): promoting to -Full.
```

Rebuilding the same variation stays incremental. A missing stamp (e.g. after an
MPLAB X IDE build) is treated as unknown and also promotes to `-Full`, on the
safe side.

Building, provisioning/verifying, and generating `reflash_image.bin` all still
happen automatically in one `build.ps1` run, unchanged from before -- see
"Flash/Reset Tool Lookup" below.

## Common Commands

Run these from the repository root:

```powershell
# Incremental build (follows the switch_config.ps1 selection)
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

Since `APP_BUILD` is not part of the HEX path, `flashauto.ps1` reports which
variation the last `build.ps1` run of this configuration produced (from the
same stamp `build.ps1` writes):

```text
Configuration: dsPIC33AK512
Last build of this configuration: APP_BUILD_TDM_SMOKE_OFF
```

## Notes

- `buildtools/active_build.json` and the `.hal_starter_app_build` stamp file
  under `firmware.X/build/<conf>/` are both untracked local state; delete
  either to fall back to the compile-time default.
- If more than one PKOB4 board is connected, pass `-Serial` explicitly.
- Reset re-enumerates PKOB4 USB, so a CDC console may briefly disconnect.
- Keep MPLAB X IDE / IPE closed while flashing or resetting from these scripts.
