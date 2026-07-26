#!/usr/bin/env python3
"""Extract the flat program-flash slice [0x800000, end] from an Intel HEX file
into a self-describing binary, 0xFF-padded, for feeding the *fua5 XMODEM
receiver into the inactive partition. dsPIC33A program flash is flat/1:1 (no
phantom bytes), so a verbatim P1 slice is a bootable image for whichever
partition is inactive (HW remaps the active partition to 0x800000).

The final 128-byte XMODEM block contains 0xFF padding followed by a 16-byte
little-endian trailer:

  magic "DBFW", format version, project ID, payload size, CRC-16, ~CRC-16

The device requires this trailer before it enables *fca5. This prevents an
arbitrary or truncated file from being accepted merely because its XMODEM block
CRCs and flash read-back happened to succeed. It is a format/integrity check,
not a cryptographic signature.

The output is NOT partition-specific: the *same* reflash image is sent by XMODEM
to whichever partition is currently inactive (P1->P2 or P2->P1). The conventional
output name is therefore partition-agnostic: `reflash_image.bin` (the default when
no output path is given), NOT `p2_image.bin`.

Usage:
  python tools/extract_p1_image.py <production.hex> [reflash_image.bin]
"""
import sys
import struct

PART_BASE = 0x800000
PART_END  = 0x840000  # one partition = 256 KB; never include past this

# Max bytes the firmware *fua5 XMODEM receiver accepts (fw_update.c): one partition
# (0x40000) MINUS the last 512-byte row, which holds BTSEQ and is write-protected
# by the receiver. An image larger than this would be rejected mid-transfer by the
# board, so we refuse to emit one here rather than fail late on the wire.
FW_MAX_IMAGE_BYTES = 0x3FE00

XMODEM_BLOCK_BYTES = 128
PACKAGE_MAGIC = b"DBFW"
PACKAGE_VERSION = 1
# CRC-16/XMODEM("sulaolab/dspic33ak-hal-starter"). This is an accidental
# cross-project-file guard, not a secret or an authentication mechanism.
PACKAGE_PROJECT_ID = 0x3448
PACKAGE_TRAILER_BYTES = 16
PACKAGE_TRAILER_STRUCT = struct.Struct("<4sHHIHH")

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
    """Append one manifest-bearing XMODEM block to a row-aligned raw image."""
    pad_len = (-len(raw_image) - PACKAGE_TRAILER_BYTES) % XMODEM_BLOCK_BYTES
    payload = bytes(raw_image) + (b"\xFF" * pad_len)
    crc = crc16_xmodem(payload)
    trailer = PACKAGE_TRAILER_STRUCT.pack(
        PACKAGE_MAGIC,
        PACKAGE_VERSION,
        PACKAGE_PROJECT_ID,
        len(payload),
        crc,
        crc ^ 0xFFFF,
    )
    packaged = payload + trailer
    assert len(trailer) == PACKAGE_TRAILER_BYTES
    assert len(packaged) % XMODEM_BLOCK_BYTES == 0
    return packaged


def validate_package(packaged):
    """Return (ok, reason) using the same checks performed by the firmware."""
    if len(packaged) < PACKAGE_TRAILER_BYTES:
        return False, "too short"
    magic, version, project_id, payload_size, expected, expected_inv = \
        PACKAGE_TRAILER_STRUCT.unpack(packaged[-PACKAGE_TRAILER_BYTES:])
    if magic != PACKAGE_MAGIC or version != PACKAGE_VERSION or project_id != PACKAGE_PROJECT_ID:
        return False, "format"
    if payload_size + PACKAGE_TRAILER_BYTES != len(packaged):
        return False, "size"
    if (expected ^ expected_inv) != 0xFFFF:
        return False, "crc complement"
    if crc16_xmodem(packaged[:payload_size]) != expected:
        return False, "crc"
    return True, "ok"

def parse_hex(path):
    mem = {}
    ulba = 0
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line[0] != ":":
                continue
            b = bytes.fromhex(line[1:])
            count = b[0]
            offset = (b[1] << 8) | b[2]
            rectype = b[3]
            data = b[4:4 + count]
            if rectype == 0x00:      # data
                base = (ulba << 16) + offset
                for i, byte in enumerate(data):
                    mem[base + i] = byte
            elif rectype == 0x04:    # extended linear address
                ulba = (data[0] << 8) | data[1]
            elif rectype == 0x01:    # EOF
                break
            # ignore 02/03/05
    return mem

def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <production.hex> [{DEFAULT_BIN}]", file=sys.stderr)
        sys.exit(2)
    hexpath = sys.argv[1]
    binpath = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_BIN
    mem = parse_hex(hexpath)
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
    out = package_image(raw)
    # The firmware receiver caps the complete on-wire package at
    # FW_MAX_IMAGE_BYTES (partition minus the BTSEQ-protection row). Check after
    # adding the trailer block so the host never emits a file that can only fail
    # late on the wire.
    if len(out) > FW_MAX_IMAGE_BYTES:
        print(f"error: packaged image size {len(out)} (0x{len(out):X}) exceeds firmware limit "
              f"0x{FW_MAX_IMAGE_BYTES:X} (partition minus BTSEQ row); "
              f"raw span={hi-lo} (0x{hi-lo:X}), top populated addr=0x{max(prog):06X}. "
              "No file written.", file=sys.stderr)
        sys.exit(1)
    ok, reason = validate_package(out)
    assert ok, reason
    with open(binpath, "wb") as f:
        f.write(out)
    used = len(prog)
    print(f"span=0x{lo:06X}..0x{hi:06X} raw={hi-lo} (0x{hi-lo:X}) "
          f"package={len(out)} (0x{len(out):X}) bytes populated={used} "
          f"raw-padding={hi-lo-used} format=DBFW/v{PACKAGE_VERSION}")

if __name__ == "__main__":
    main()
