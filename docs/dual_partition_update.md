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
Select reflash_image.bin with 1K unchecked.
Waiting for xmodem-crc data on UART1...
```

Periodic application output stops while the update channel is armed, and the
background TDM/DMA smoke stream is stopped before Flash erase/program begins.
This is intentional: XMODEM control bytes must be the only board-to-host traffic
during the transfer, and autonomous DMA must not run across NVM CPU stalls. A
failed receive restarts the TDM demo and periodic output; a successful receive
stays quiet while waiting for `*fca5` and reset. If commit fails and returns,
the demo and periodic output resume after the failure instructions are printed.

## 5. Send `reflash_image.bin` with XMODEM

In Tera Term, select:

```text
File > Transfer > XMODEM > Send
```

Choose:

```text
firmware.X\dist\dsPIC33AK512\production\reflash_image.bin
```

Use XMODEM-CRC with **1K unchecked**. The receiver understands both 128-byte and
1024-byte XMODEM frames, but `reflash_image.bin` deliberately places its DBFW
trailer in the final 128-byte block. Tera Term's 1K mode can append CPMEOF
padding after that trailer, so package validation correctly rejects the file.

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
  package crc  : ....
Validation: PASS (DBFW manifest + xmodem-crc + flash read-back)
Type *fca5 to activate the validated partition.
```

At this point, the new firmware is present only in the inactive partition. The
currently running partition remains selected until the commit command.

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
4. Select `reflash_image.bin` with **1K unchecked**.
5. Wait for `Firmware receive: PASS` and `Validation: PASS`.
6. Type `*fca5` and press Enter.
7. Confirm the new active partition and `active UCA OK` after reset.

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
- **1K** is unchecked.
- The selected file is the latest generated `reflash_image.bin` from this project.

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

## Safety and security scope

The update path includes:

- XMODEM-CRC transport checking
- a DBFW format/version/project identifier
- a package CRC and inverse CRC
- inactive-Flash read-back verification
- inactive UCA validation
- BTSEQ program/read-back verification
- fail-closed commit gating

The DBFW trailer is an integrity and wrong-project guard, not a cryptographic
signature. This example is not a secure-boot implementation and does not
authenticate an untrusted firmware publisher.
