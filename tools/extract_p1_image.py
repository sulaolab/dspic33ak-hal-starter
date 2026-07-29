#!/usr/bin/env python3
"""Extract the flat program-flash slice [0x800000, end] from an Intel HEX file
into a self-describing binary, 0xFF-padded, for feeding the *fua5 XMODEM
receiver into the inactive partition. dsPIC33A program flash is flat/1:1 (no
phantom bytes), so a verbatim P1 slice is a bootable image for whichever
partition is inactive (HW remaps the active partition to 0x800000).

The file LEADS with a 16-byte little-endian manifest, followed by the payload:

  magic "DBFW", format version, project ID, payload size, CRC-16, ~CRC-16

The device requires this manifest before it enables *fca5. This prevents an
arbitrary or truncated file from being accepted merely because its XMODEM block
CRCs and flash read-back happened to succeed. It is a format/integrity check,
not a cryptographic signature.

The manifest LEADS (format v2) rather than trails (v1) because XMODEM has no
file-length field: a sender must pad its final block out to the full block size
(128 or 1024), conventionally with 0x1A (CP/M EOF). A trailing manifest is
therefore only "the last 16 bytes received" when the file size happens to be a
multiple of the sender's block size -- true for a 128-byte-aligned file under
128-byte blocks, FALSE under XMODEM-1K, which broke v1 on real hardware. With the
manifest first, the receiver knows the exact payload length before any payload
byte arrives, so no alignment is needed here and the receiver simply discards the
sender's trailing pad bytes. Do NOT "helpfully" pad the output to a block
boundary: it buys nothing and reintroduces an alignment assumption.

The output is NOT partition-specific: the *same* reflash image is sent by XMODEM
to whichever partition is currently inactive (P1->P2 or P2->P1). The conventional
output name is therefore partition-agnostic: `reflash_image.bin` (the default when
no output path is given), NOT `p2_image.bin`.

Usage:
  python tools/extract_p1_image.py <production.hex> [reflash_image.bin]
"""
import sys
import struct

import ihex_lite
import uca_manifest

# Program-flash slice bounds. Taken from the manifest rather than re-typed, so the
# extractor and verify_dual_partition_hex.py cannot disagree about what "the
# application region" means.
PART_BASE = uca_manifest.PROGRAM_REGION_LO   # 0x800000
PART_END  = uca_manifest.PROGRAM_REGION_HI   # 0x840000; one partition = 256 KB

# Max PAYLOAD bytes the firmware *fua5 XMODEM receiver accepts (fw_update.c): one
# partition (0x40000) MINUS the last 512-byte row, which holds BTSEQ and is
# write-protected by the receiver. The 16-byte manifest is metadata and never
# reaches flash, so it does NOT count against this cap. A payload larger than this
# would be refused by the board, so we refuse to emit one here instead.
# Host-side source of truth, shared with verify_dual_partition_hex.py. The firmware
# has its own independent definition; test_dual_partition_hex.py compares the two and
# fails on drift (it cannot prevent it).
FW_MAX_IMAGE_BYTES = uca_manifest.MAX_PAYLOAD_BYTES

PACKAGE_MAGIC = b"DBFW"
PACKAGE_VERSION = 2
# CRC-16/XMODEM("sulaolab/dspic33ak-hal-starter"). This is an accidental
# cross-project-file guard, not a secret or an authentication mechanism.
PACKAGE_PROJECT_ID = 0x3448
PACKAGE_MANIFEST_BYTES = 16
PACKAGE_MANIFEST_STRUCT = struct.Struct("<4sHHIHH")

# Must match FW_PACKAGE_MAX_TRAILING_PAD in src/fw_update/fw_update.c: how much
# sender block padding past the end of the payload the receiver tolerates.
PACKAGE_MAX_TRAILING_PAD = 2048

# Partition-agnostic default: the XMODEM payload is the reflash image for the
# inactive partition, not a "P2 image". See module docstring.
DEFAULT_BIN = "reflash_image.bin"


def crc16_xmodem(data):
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def package_image(raw_image):
    """Prefix the 16-byte manifest to a row-aligned raw image."""
    payload = bytes(raw_image)
    crc = crc16_xmodem(payload)
    manifest = PACKAGE_MANIFEST_STRUCT.pack(
        PACKAGE_MAGIC,
        PACKAGE_VERSION,
        PACKAGE_PROJECT_ID,
        len(payload),
        crc,
        crc ^ 0xFFFF,
    )
    assert len(manifest) == PACKAGE_MANIFEST_BYTES
    return manifest + payload


def validate_package(packaged):
    """Return (ok, reason) using the same checks performed by the firmware.

    Mirrors fw_validate_header() + fw_validate_package() in src/fw_update/fw_update.c,
    including the important part: bytes BEYOND manifest+payload are the XMODEM
    sender's block padding and must NOT cause a rejection.
    """
    if len(packaged) < PACKAGE_MANIFEST_BYTES:
        return False, "too short"
    magic, version, project_id, payload_size, expected, expected_inv = \
        PACKAGE_MANIFEST_STRUCT.unpack(packaged[:PACKAGE_MANIFEST_BYTES])
    if magic != PACKAGE_MAGIC or version != PACKAGE_VERSION or project_id != PACKAGE_PROJECT_ID:
        return False, "format"
    if payload_size == 0:
        return False, "format"
    if payload_size > FW_MAX_IMAGE_BYTES:
        return False, "overflow"
    if len(packaged) < PACKAGE_MANIFEST_BYTES + payload_size:
        return False, "truncated"
    trailing = len(packaged) - PACKAGE_MANIFEST_BYTES - payload_size
    if trailing > PACKAGE_MAX_TRAILING_PAD:
        return False, "trailing padding"
    if (expected ^ expected_inv) != 0xFFFF:
        return False, "crc complement"
    payload = packaged[PACKAGE_MANIFEST_BYTES:PACKAGE_MANIFEST_BYTES + payload_size]
    if crc16_xmodem(payload) != expected:
        return False, "crc"
    return True, "ok"

def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <production.hex> [{DEFAULT_BIN}]", file=sys.stderr)
        sys.exit(2)
    hexpath = sys.argv[1]
    binpath = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_BIN
    try:
        mem = ihex_lite.parse_hex(hexpath)
    except (OSError, ValueError) as exc:
        print(f"cannot parse {hexpath}: {exc}", file=sys.stderr)
        sys.exit(1)
    prog = {a: v for a, v in mem.items() if PART_BASE <= a < PART_END}
    if not prog:
        print("no program bytes in [0x800000,0x840000)", file=sys.stderr)
        sys.exit(1)
    # INVARIANT: the *fua5 XMODEM slice carries ONLY
    # the program region [0x800000,0x840000). The per-partition UCA (0x7F3xxx /
    # 0x7FBxxx) and shared UCB FBOOT (0x7F40D0) live BELOW 0x800000 and are never
    # part of this image. A code-only serial update therefore cannot change a
    # partition's config fuses -- UCA provisioning is a flash-time concern only.
    assert all(a >= PART_BASE for a in prog), "UCA/UCB byte leaked into program slice"
    assert PART_END <= PART_BASE + 0x40000, "partition slice exceeds one 256KB partition"
    lo = PART_BASE
    hi = max(prog) + 1
    # round up to a 512-byte row boundary so rows align cleanly
    if hi % 512:
        hi += 512 - (hi % 512)
    raw = bytearray(b"\xff" * (hi - lo))
    for a, v in prog.items():
        raw[a - lo] = v
    # The firmware caps the PAYLOAD (the part that reaches flash) at
    # FW_MAX_IMAGE_BYTES -- partition minus the BTSEQ-protection row. The manifest
    # is metadata and never programmed, so it is excluded. Check before writing so
    # the host never emits a file that can only fail on the wire.
    if len(raw) > FW_MAX_IMAGE_BYTES:
        print(f"error: payload size {len(raw)} (0x{len(raw):X}) exceeds firmware limit "
              f"0x{FW_MAX_IMAGE_BYTES:X} (partition minus BTSEQ row); "
              f"raw span={hi-lo} (0x{hi-lo:X}), top populated addr=0x{max(prog):06X}. "
              "No file written.", file=sys.stderr)
        sys.exit(1)
    out = package_image(raw)
    ok, reason = validate_package(out)
    assert ok, reason
    with open(binpath, "wb") as f:
        f.write(out)
    used = len(prog)
    print(f"span=0x{lo:06X}..0x{hi:06X} payload={len(raw)} (0x{len(raw):X}) "
          f"file={len(out)} (0x{len(out):X}) bytes populated={used} "
          f"raw-padding={hi-lo-used} format=DBFW/v{PACKAGE_VERSION} "
          f"(manifest-first; XMODEM 1K ok)")

if __name__ == "__main__":
    main()
