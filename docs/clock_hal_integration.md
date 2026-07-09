# Clock HAL Integration

## §1 Clock HAL Integration — Generic HAL + Starter Policy

### Status

Starter Clock HAL integration:
  PENDING I2C LOOPBACK RERUN

Starter mainline integration:
  NOT PERFORMED BY DESIGN

Ready for later three-way mainline operation:
  PENDING I2C LOOPBACK RERUN

This report is the branch-preparation and first hardware-smoke record for
`exp/starter-clock-hal-integration`. The clock-critical smoke items passed, but
the external I2C2/I2C3 loopback runtime check did not return matching data on
this board setup. That item must be rerun before this section may be promoted to
`COMPLETE`.

### Starting Starter State

| Item | Value |
|---|---|
| Repository | `sulaolab/dspic33ak-hal-starter` |
| Working folder | `C:\00_storage\_Git_Work\vscode-home\dspic33ak-hal-starter` |
| Branch | `exp/starter-clock-hal-integration` |
| Starting HEAD | `dd2e5939f79f3c2d291839dccf29d512b6504c0f` |
| Starting base | `main` / `origin/main` at the starting HEAD |
| Mainline action | None; this task is branch-only |

The Starter `main` branch was not modified. The integration branch was created
from the verified starting HEAD.

### Perseus Source Provenance

The generic Clock HAL was imported from the pinned Perseus readiness source:

| Item | Value |
|---|---|
| Source repository | `sulaolab/perseus_512_96K` |
| Source branch | `exp/perseus-clock-hal-starter-readiness` |
| Source commit | `89c2b88f44f0830ece99dcc9649b5e16382f3421` |
| Source report | `docs/pll2_frc_cold_start_findings.md`, `§13 Clock HAL Starter-Migration Readiness Review` |

The imported files are:

| Starter path | SHA-1 blob |
|---|---|
| `src/hal_clock/dspic33ak_clock.h` | `7ae5e89a7adf8f2e3a1bc5512f6db7af41d3bce4` |
| `src/hal_clock/dspic33ak_clock.c` | `4f0640f5f819c75cc76002b26f88d706c59a7eaf` |
| `src/hal_clock/dspic33ak_clock_device.h` | `05b7f375c630d3f6799cd90c475138572fabe1b4` |
| `src/hal_clock/dspic33ak_clock_device.c` | `a230a806eceb51a0bf956d20352b74f27f1c660a` |
| `src/hal_clock/dspic33ak_clock_reg.h` | `e9636729324a84c9e842d522a83508a9a282ba88` |
| `src/hal_clock/dspic33ak_clock_reg.c` | `ab5a3d42476d2e22cee318ac611ffacf856de2d8` |

Final preparation verification confirmed all six local blobs are byte-identical
to the pinned Perseus commit. No Starter-local change was made inside
`src/hal_clock/`.

### Architecture

Before this branch, `src/clock/dspic33ak_clock.*` mixed generic PLL/CLKGEN
mechanics with Starter policy. After the migration the layering is:

```text
Application / peripheral integration
        ->
src/clock/starter_clock.*
        ->
src/hal_clock/dspic33ak_clock.*
        ->
device/register adaptation
        ->
dsPIC33AK SFRs
```

Ownership boundaries:

| Layer | Owns |
|---|---|
| Application / demos | When to call the Starter clock policy and which peripheral clock facts to pass into HAL configs. |
| `src/clock/` | Starter board/demo policy: selected source, target frequencies, CLKGEN routing, and CAN FCAN policy. |
| `src/hal_clock/` | Reusable Clock HAL mechanics: PLL solving, logical source encoding, CLKGEN programming, SFR polling, and register adaptation. |

The generic Clock HAL remains mechanism-only. The Starter policy remains outside
the HAL folder.

### Legacy Module Removal

Removed legacy files:

```text
src/clock/dspic33ak_clock.c
src/clock/dspic33ak_clock.h
```

Added Starter policy files:

```text
src/clock/starter_clock.c
src/clock/starter_clock.h
```

At this branch state, the only `dspic33ak_clock.*` file family is the generic
HAL copy under `src/hal_clock/`.

### Starter Policy Implementation

Public Starter policy API:

```c
#define STARTER_CLOCK_SYS_HZ       (200000000UL)
#define STARTER_CLOCK_FCY_HZ       (STARTER_CLOCK_SYS_HZ / 2UL)
#define STARTER_CLOCK_CAN_FCAN_HZ  (20000000UL)

bool starter_clock_init(void);
bool starter_clock_can_init(void);
```

`starter_clock_init()` implements:

| Policy | Value |
|---|---|
| PLL input | FRC, 8 MHz |
| Application SYSCLK | PLL1, 200 MHz |
| FCY | 100 MHz |
| Application CLKGENs | CLKGEN1/5/6/8/9 |
| Application CLKGEN divider | divide-by-1 |

`starter_clock_can_init()` implements:

| Policy | Value |
|---|---|
| CAN clock source | PLL1 |
| CAN CLKGEN | CLKGEN10 |
| CAN divider | divide-by-10 |
| CAN FCAN | 20 MHz |

The Starter policy layer calls `dspic33ak_clock_pll_configure()` and
`dspic33ak_clock_clkgen_configure()`. It does not duplicate PLL solving, raw
NOSC encodings, SFR writes, or polling loops.

### API Migration

Old public Starter Clock API uses were migrated:

| Old symbol | New symbol |
|---|---|
| `dspic33ak_clock_init()` | `starter_clock_init()` |
| `dspic33ak_clock_can_init()` | `starter_clock_can_init()` |
| `DSPIC33AK_CLOCK_SYS_HZ` | `STARTER_CLOCK_SYS_HZ` |
| `DSPIC33AK_CLOCK_SYS_HZ / 2` | `STARTER_CLOCK_FCY_HZ` |
| CAN `20000000u` literals in Starter app tests | `STARTER_CLOCK_CAN_FCAN_HZ` |

Migrated users include `main.c`, CAN loopback, CAN bus test, CAN RX-ISR
self-test, and the TDM smoke demo. Comments in board and CAN headers were
updated where they named the old API.

### Clock Facts

The intended effective Starter behavior is preserved:

| Consumer | Clock fact |
|---|---|
| SYSCLK banner | `200000000 Hz (FRC -> PLL1)` |
| UART1 console | CLKGEN8 from PLL1 divide-by-1, 200 MHz input, 230400 8N1 |
| Timer1 tick | FCY, 100 MHz |
| Timer2 HRT | FCY, 100 MHz |
| I2C | FCY, 100 MHz |
| CAN FD | CLKGEN10 from PLL1 divide-by-10, 20 MHz FCAN |
| TDM smoke | SPI1 baud source derived from `STARTER_CLOCK_SYS_HZ` |

The standalone `src/hal_can/` comments still use examples such as 20 MHz FCAN
where appropriate; board policy is not moved into the generic CAN HAL.

### Project and Documentation Changes

`firmware.X/nbproject/configurations.xml` was updated to:

```text
replace ../src/clock/dspic33ak_clock.c with starter_clock.c/.h
add the six src/hal_clock files
add ..\src\hal_clock to the include path
```

The generated `Makefile-*.mk` files remain untracked local artifacts.

`README.md` and `docs/source_layout.md` were updated so the repository layout
now describes:

```text
src/hal_clock/  reusable generic Clock HAL
src/clock/      Starter-specific clock policy
```

The README active HAL surface now includes Clock HAL explicitly.

### Static Verification

Focused inventories were run after migration.

| Check | Result |
|---|---|
| `dspic33ak_clock_init` | No references |
| `dspic33ak_clock_can_init` | No references |
| `DSPIC33AK_CLOCK_SYS_HZ` | No references |
| `DSPIC33AK_CLKSRC_` | No references |
| Old `src/clock/dspic33ak_clock.*` implementation | Removed |
| `#include "dspic33ak_clock.h"` | Only `src/clock/starter_clock.c` and `src/hal_clock/*`, as intended |
| Raw PLL/CLKGEN SFR access | Limited to `src/hal_clock/dspic33ak_clock_reg.c` |
| `git diff --check` | PASS |

### Build Verification

Builds were run with the repository build tools and current installed
MPLAB X / XC-DSC / DFP environment.

| Command | Result |
|---|---|
| `.\buildtools\build.ps1 -Full` | PASS |
| `.\buildtools\build.ps1 -Clean` | PASS |
| `.\buildtools\build.ps1` | PASS |

The final normal build generated:

```text
firmware.X/dist/dsPIC33AK512/production/firmware.X.production.hex
```

No new Clock integration warning was observed.

### Hardware Smoke

Hardware smoke is mandatory for final completion because this migration changes
the live boot clock path and CAN CLKGEN10 policy path.

Current result:

```text
PARTIAL PASS - clock-critical items passed; I2C loopback data path pending.
```

Measured smoke items:

| Item | Result |
|---|---|
| Flash | PASS, `Program succeeded` / `Programming/Verify complete` |
| Reset | PASS, PKOB4 Boost reset succeeded |
| UART console | PASS on `COM10` at 230400 8N1 |
| Boot banner | PASS |
| SYSCLK line | PASS, `200000000 Hz (FRC -> PLL1)` |
| HRT self-check | PASS |
| SST26 JEDEC and sector verify | PASS |
| I2C scan | PASS, device ACK at `0x1A` |
| I2C loopback runtime | PENDING / NOT PASS on this setup: `I2C3 Rd size=0`, `I2C2 Rd=0000000000000000` |
| CAN RX-ISR self-test | PASS |
| CAN live HAL self-check | PASS; later lone-node no-ACK queue timeout behavior observed |
| ADC / RGB / potentiometer | Startup message observed |
| TDM smoke | PASS, started and continued with `miss=0` |

No oscilloscope measurement is required unless UART or runtime behavior exposes a
clock problem.

Hardware identifiers:

| Item | Value |
|---|---|
| PKOB4 serial | `020085204RYN000057` |
| Target device | `dsPIC33AK512MPS512` |
| Device ID | `0xa77c` |
| Device revision | `0x1` |
| Boot UDID line | `FFFFFFFF010B00DBB8D0000D00D76A9D` |
| Console port | `COM10` |
| Firmware left on board | This integration branch firmware |

The observed I2C loopback mismatch is not yet classified as a Clock HAL
regression because the I2C scan, UART baud, timers, CAN, and TDM clock-dependent
paths were operating. It should be rerun with the expected I2C2/I2C3 loopback
wiring before final completion.

### Caveats and Unresolved Items

| Item | Status |
|---|---|
| Hardware smoke | Partial pass; I2C loopback runtime rerun required |
| Starter mainline merge | Not performed by design |
| Three-way mainline transition | Deferred until Starter, Perseus readiness, and standalone HAL publication are coordinated |
| Generic HAL divergence | None; HAL files remain byte-identical to the pinned Perseus source |

### Branch and Push State

This section records the branch state after preparation and first hardware
smoke. A final update is required after the I2C loopback rerun, final status
promotion, and final push.

| Item | Value |
|---|---|
| Branch | `exp/starter-clock-hal-integration` |
| Starting HEAD | `dd2e5939f79f3c2d291839dccf29d512b6504c0f` |
| Hardware state | Integration firmware flashed and left on target |
| Mainline | Unchanged |

### Recommended Next One Task

Restore or confirm the expected I2C2/I2C3 loopback wiring, reset the current
integration branch firmware, and rerun only the default Starter UART smoke long
enough to confirm I2C loopback data matches. If it passes, update this report
with the measured result and promote:

```text
Starter Clock HAL integration:
  COMPLETE
```

only after the loopback item passes or is explicitly waived as a board setup
condition.
