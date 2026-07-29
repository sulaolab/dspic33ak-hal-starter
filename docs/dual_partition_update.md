# Dual-Partition Firmware Update Guide

This guide describes the complete beginner-facing workflow for first-time
provisioning and later serial firmware updates on the dsPIC33AK512MPS512.

The updater is part of the application. It always writes the partition that is
not currently running, verifies the received image, and changes the boot
sequence only after an explicit commit. There is no separate P1 image or P2
image: the same `reflash_image.bin` is used in both directions.

## What you need

Install the following on Windows:

- PowerShell 7 (`pwsh`)
- MPLAB X IDE 6.30
- XC-DSC 3.31.01
- Python 3
- Tera Term

Connect the dsPIC33AK512MPS512 board by USB. Windows normally exposes two serial
ports:

| Windows description | Firmware connection | Purpose |
| --- | --- | --- |
| **USB Serial Port** | UART1 / MCP2221A | Commands and XMODEM transfers |
| **USB Serial Device** | UART2 / PKOB4 | Output mirror only |

Use **USB Serial Port** for every command and XMODEM operation in this guide.

## 1. Build the firmware and update artifacts

This guide builds the shipped default demo firmware (`APP_BUILD_STARTER_DEFAULT`).
To build a different demo variant instead (see
[buildtools/README.md](../buildtools/README.md) for the catalog), run
`.\buildtools\switch_config.ps1` first; it persists the choice so the commands
below still apply unchanged.

Open PowerShell 7 in the repository root and run:

```powershell
pwsh -File .\buildtools\build.ps1 `
  -Full `
  -Configuration dsPIC33AK512
```

A successful build ends with:

```text
provision: PASS
Dual-partition artifacts: PASS
```

The script creates these files under
`firmware.X/dist/dsPIC33AK512/production/`:

| Artifact | Purpose |
| --- | --- |
| `firmware.X.production.hex` | Raw compiler output; not the normal first-flash file |
| `firmware.X.production.bundle.hex` | Verified initial PKOB4 image with matching P1/P2 UCA |
| `firmware.X.production.bundle.verify_report.txt` | PASS result and bundle SHA-256 |
| `reflash_image.bin` | Partition-independent serial update package |

`build.ps1` first builds `firmware.X.production.hex`, then calls
`provision.ps1`. Provisioning copies the required P1 configuration words to the
P2 UCA, generates the bundle, verifies both partitions, and checks the selected
MPLAB X configuration's device, DFP, and compiler against the manifest pins. Finally,
`build.ps1` generates the DBFW-manifested `reflash_image.bin`.

Do not rename individual files in this output set. The scripts deliberately use
matching deterministic names so a verification report cannot silently attest a
different bundle.

## 2. Perform the first PKOB4 flash

The first flash must provision the dual-partition boot mode and both partitions'
UCA configuration. Run:

```powershell
pwsh -File .\buildtools\flashauto.ps1 `
  -Configuration dsPIC33AK512
```

If more than one PKOB4 is connected, identify the target first:

```powershell
pwsh -File .\buildtools\flashauto.ps1 -List
```

Then specify its serial number:

```powershell
pwsh -File .\buildtools\flashauto.ps1 `
  -Configuration dsPIC33AK512 `
  -Serial YOUR_PKOB4_SERIAL
```

Without an explicit `-Hex`, `flashauto.ps1` does **not** scan for an arbitrary
HEX file. It constructs these exact paths from the MPLAB X project name and the
selected configuration:

```text
firmware.X/dist/dsPIC33AK512/production/firmware.X.production.bundle.hex
firmware.X/dist/dsPIC33AK512/production/firmware.X.production.bundle.verify_report.txt
```

Before programming, it requires the report to say `PASS`, extracts the recorded
bundle SHA-256, recalculates the current bundle SHA-256, and refuses to flash if
they differ. It then passes the verified bundle path to `flash_pkob4.exe` and
resets the board after programming.

A successful first flash includes:

```text
Programming/Verify complete
Program succeeded.
reset succeeded
flashauto: flash + reset completed
```

> [!CAUTION]
> `-Hex` is an advanced diagnostic override. It selects the requested file
> directly and bypasses the default verified-bundle selection. Do not use
> `-Hex` for the normal beginner workflow.

## 3. Connect Tera Term to UART1

Open Tera Term and select a **Serial** connection to **USB Serial Port**. Do not
select **USB Serial Device**.

Configure the serial port as follows:

```text
Speed        230400
Data         8 bit
Parity       none
Stop bits    1 bit
Flow control none
```

In the terminal settings, use:

```text
Local echo       off
Transmit newline CR
```

This public starter workflow uses a direct COM connection. It does not require
TCP, Telnet, a local monitor, or `127.0.0.1:23`.

After reset, confirm that the boot banner contains lines similar to:

```text
bank   : P1 active, BTSEQ=0xFFF
config : active UCA OK (ALTI2C2=ON, BOOTSWP=ENABLED)
update : type *fua5 on UART1, then send reflash_image.bin via XMODEM
```

Do not continue with a serial update if the active UCA is reported as invalid.
Repeat the verified first-flash procedure instead.

To see the same information again later without resetting the board, type
`?fp` (read-only; does not arm or change anything):

```text
"?fp" bank     : P1 active, BTSEQ=0xFFF (inactive P2, BTSEQ=0xFFF)
"?fp" active   : UCA OK (ALTI2C2=ON, BOOTSWP=ENABLED)
"?fp" inactive : UCA OK (ALTI2C2=ON, BOOTSWP=ENABLED)
```

`BOOTSWP=ENABLED` is the functional meaning of FICD bit 15 being clear. XC-DSC
and the DFP spell the configuration pragma `NOBTSWP=ON`; for this device that
spelling means raw `NOBTSWP=0`, which enables the BOOTSWP instruction. The
updater commits through BTSEQ followed by reset, but preserves and verifies this
project configuration consistently in both partition UCAs.

## 4. Arm the inactive-partition receiver

Type the following command in Tera Term and press Enter:

```text
*fua5
```

Commands are case-insensitive. The application responds with instructions such
as:

```text
Firmware update armed.
Tera Term: File > Transfer > XMODEM > Send
Select reflash_image.bin (1K may be checked or unchecked).
LED7..LED0 show transfer progress as a bar.
Waiting for xmodem-crc data on UART1...
```

> [!IMPORTANT]
> Start the XMODEM send within about **30 seconds**. The receiver kicks the sender
> ten times at three-second intervals and then gives up, so if you take too long
> choosing the file the transfer dialog opens onto a receiver that has already
> stopped listening. Cancel the dialog, type `*fua5` again, and retry.

Periodic application output stops automatically as soon as `*fua5` is accepted,
and the background TDM/DMA smoke stream is stopped before Flash erase/program
begins. This is intentional and needs no separate command: XMODEM control bytes
must be the only board-to-host traffic during the transfer, and autonomous DMA
must not run across NVM CPU stalls.

Periodic output does **not** come back on its own -- not after a successful
receive, not after a failed receive, and not after a failed commit. It only
resumes on an explicit `*tq0000`, or a reset (a successful `*fca5` commit). The
TDM/DMA smoke stream is the one thing that *does* restart automatically after a
failed receive or a failed commit, since otherwise the demo would stay silently
dead with no way back short of a reset; that restart is independent of the
output toggle and happens either way.

Use `*tq0000` / `*tq0001` any time, independent of the update workflow:

```text
*tq0001   -- periodic starter output OFF
*tq0000   -- periodic starter output ON
```

## 5. Send `reflash_image.bin` with XMODEM

In Tera Term, select:

```text
File > Transfer > XMODEM > Send
```

Choose:

```text
firmware.X\dist\dsPIC33AK512\production\reflash_image.bin
```

Use XMODEM-CRC. **1K may be checked or unchecked** -- both work, and neither
needs any particular file size.

<img src="images/dual-partition-xmodem-1k-transfer.png" alt="Tera Term sending reflash_image.bin over XMODEM-1K: the console shows *fua5 accepted, the armed instructions, and the receiver's repeated C handshake characters, while the XMODEM Send dialog reports protocol XMODEM (1k), packet 62, 63488 bytes transferred at 16.38 KB/s, 75.6 percent complete" width="640">

A transfer in progress with **1K** checked. The dialog's packet count and byte
total are Tera Term's own accounting of what it has sent.

The `ccc` visible on the console is normal and worth recognising: in XMODEM the
*receiver* starts the conversation, and the character `C` means "ready, and use
CRC-16 rather than the original checksum". The board repeats it every three
seconds until the sender responds, so the number of `C`s you see is simply how
long the file dialog took. Once blocks start flowing the `C`s stop, and the
board's per-block replies (`ACK`/`NAK`) are control bytes that a terminal does
not display.

XMODEM has no file-length field, so every sender pads its final block out to the
full block size (128 or 1024 bytes), conventionally with `0x1A` (CP/M EOF). Those
pad bytes arrive after the real image, which is why `bytes received` in the
firmware's report is normally a little larger than `reflash_image.bin` itself --
that is expected, not an error. The DBFW manifest is therefore placed at the
**front** of the file: the receiver learns the exact payload length before any
image byte arrives, programs exactly that many bytes, and discards the sender's
padding without ever writing it to flash.

While the transfer runs, the board fills the user LEDs as a progress bar,
one step per eighth of the payload, growing from **LED7 toward LED0**. The bar is left standing when
the transfer ends, so it doubles as a result indicator at a glance: a full bar
means the whole payload was received and programmed, and a partial bar shows how
far a failed transfer got. LED0 goes back to being the heartbeat indicator after
`*tq0000`.

Progress is shown on the LEDs rather than as characters on the console because
the XMODEM transport owns UART1 for the whole transfer: the sender is waiting for
`ACK`/`NAK`, so any extra byte the board printed there would corrupt the protocol.
Tera Term's own transfer dialog remains the byte-accurate progress display.

> [!WARNING]
> Do not use **File > Send file**. That command sends an unframed byte stream;
> it is not XMODEM. The firmware will discard detected plain-file input and tell
> you to retry with **File > Transfer > XMODEM > Send**.

## 6. Check the receive and validation result

Wait for both success lines:

```text
Firmware receive: PASS
Validation: PASS (DBFW manifest + xmodem-crc + flash read-back)
```

The complete result resembles:

```text
Firmware receive: PASS
  bytes        : ...
  pages erased : ...
  rows written : ...
  image crc    : ....
  read-back crc: ....
  payload crc  : ....
Validation: PASS (DBFW manifest + xmodem-crc + flash read-back)
Type *fca5 to activate the validated partition.
Periodic starter output stays off. Type *tq0000 to resume it.
```

At this point, the new firmware is present only in the inactive partition. The
currently running partition remains selected until the commit command. Periodic
output stays off either way (it does not resume on its own after a PASS or a
FAIL); type `*tq0000` if you want it back before deciding whether to commit.

> [!NOTE]
> The commit authorization lives only in RAM. If the board resets or loses power
> after `Validation: PASS` but before `*fca5`, the current partition stays active
> and safe -- but the authorization is gone, so `*fca5` alone is refused and the
> image must be transferred again. The received image is still in Flash; only the
> permission to commit it is not.

## 7. Commit and boot the updated partition

Only after both PASS lines are visible, type:

```text
*fca5
```

The firmware validates the inactive partition's UCA, writes and reads back its
BTSEQ word, and resets the device. A successful commit resembles:

```text
Committing validated partition; the board will reset...
"*fca5" committed next=0xFFE ... -- resetting to swap
```

After reset, verify that the other partition is active and the UCA is valid:

```text
bank   : P2 active, BTSEQ=0xFFE
config : active UCA OK (ALTI2C2=ON, BOOTSWP=ENABLED)
```

The next update uses exactly the same steps and the same generated filename.
When P2 is active, the application writes P1; when P1 is active, it writes P2.

## What each path writes

Serial updates are **code-only**. Knowing which path owns which memory explains
most surprises:

| | Initial bundle flash (PKOB4) | Serial update (`*fua5` / `*fca5`) |
| --- | --- | --- |
| Application, active partition | written | never touched |
| Application, inactive partition | left erased until the first serial update | written |
| Per-partition config words (P1 + P2 UCA) | written, P2 cloned from P1 and verified | never touched |
| Shared boot config (FBOOT / `BTMODE=DUAL`) | written | never touched |
| BTSEQ (boot selection) | left erased | written by `*fca5` only |

`reflash_image.bin` is a slice of `[0x800000, 0x840000)` and contains no UCA,
FBOOT, or configuration words at all, so a serial update cannot change a
partition's fuses even in principle.

> [!IMPORTANT]
> After editing any `#pragma config`, a serial update will **not** apply it.
> Rebuild and re-provision over PKOB4 with the verified bundle
> (`build.ps1`, then `flashauto.ps1`), which rewrites both partitions' config
> words and re-verifies them.

## Quick reference

First-time provisioning:

```powershell
pwsh -File .\buildtools\build.ps1 -Full -Configuration dsPIC33AK512
pwsh -File .\buildtools\flashauto.ps1 -Configuration dsPIC33AK512
```

Later serial updates:

1. Connect Tera Term directly to **USB Serial Port**, `230400 8N1`, no flow control.
2. Type `*fua5` and press Enter.
3. Select **File > Transfer > XMODEM > Send**.
4. Select `reflash_image.bin` (**1K** may be checked or unchecked).
5. Wait for `Firmware receive: PASS` and `Validation: PASS`.
6. Type `*fca5` and press Enter.
7. Confirm the new active partition and `active UCA OK` after reset, or check
   without resetting by typing `?fp`.

Other useful commands, usable any time on UART1:

| Command | Effect |
| --- | --- |
| `?fp` | Print which partition is active, both BTSEQ words, and both UCA states. Read-only. |
| `*tq0001` | Turn periodic starter output off, independent of `*fua5`/`*fca5`. |
| `*tq0000` | Turn periodic starter output back on. |

## Troubleshooting

### `*fca5` says that no validated image exists

```text
Commit refused: no validated image. Run *fua5 first.
```

The commit gate is intentionally one-shot. A missing transfer, failed transfer,
failed validation, or previous commit attempt consumes or leaves the gate
closed. Type `*fua5`, transfer `reflash_image.bin` again, and wait for both PASS
lines before retrying `*fca5`.

### Plain file data was detected

Cancel the current send, type `*fua5` again, and use **File > Transfer > XMODEM
> Send**. Do not use **File > Send file**.

### XMODEM does not start

Check all of the following:

- Tera Term is connected to **USB Serial Port**, not **USB Serial Device**.
- The serial settings are `230400 8N1`, no flow control.
- No other terminal has the same COM port open.
- `*fua5` was accepted before opening the XMODEM dialog.
- The selected file is the latest generated `reflash_image.bin` from this project.

The **1K** checkbox does not matter either way (see step 5). If a transfer fails
with `Firmware receive: FAIL (wrong reflash image format...)`, the usual cause is
a stale `reflash_image.bin` -- rebuild with `build.ps1` and send the freshly
generated file.

### The terminal output is unreadable

Recheck `230400 8N1`, parity `none`, and flow control `none`.

### The active UCA is invalid

Do not commit an update. Rebuild and repeat the first PKOB4 flash using the
default verified bundle:

```powershell
pwsh -File .\buildtools\build.ps1 -Full -Configuration dsPIC33AK512
pwsh -File .\buildtools\flashauto.ps1 -Configuration dsPIC33AK512
```

### A transfer is interrupted

The active partition is not intentionally modified by the serial receiver. Type
`*fua5`, send the complete image again, and commit only after both PASS lines.

### XMODEM never starts, or the receiver gave up

The receiver listens for roughly 30 seconds after `*fua5` (ten kicks, three
seconds apart). If the file-selection dialog took longer than that, cancel it,
type `*fua5` again, and pick the file promptly.

### `*fca5` is refused after a reset, even though the transfer passed

Expected: the commit authorization is held in RAM only, so a reset or power cycle
between `Validation: PASS` and `*fca5` clears it. Repeat `*fua5` and the transfer.

## Safety and security scope

The update path includes:

- XMODEM-CRC transport checking
- a DBFW format/version/project identifier
- an exact payload length plus payload CRC and inverse CRC
- inactive-Flash read-back verification
- inactive UCA validation
- BTSEQ program/read-back verification
- fail-closed commit gating

The DBFW manifest is an integrity and wrong-project guard, not a cryptographic
signature. This example is not a secure-boot implementation and does not
authenticate an untrusted firmware publisher.

Firmware version enforcement and anti-rollback are also **not** implemented: any
older image from this same project is a perfectly valid package and will be
accepted and committed. Note that the DBFW `version` field is the *package format*
version, not a firmware release version, and BTSEQ counts down per commit as a
boot-selection mechanism -- neither one expresses "newer than what is installed".
