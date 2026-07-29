#!/usr/bin/env python3
"""§8.1 host acceptance tests for the dual-partition UCA provisioning toolchain.

Stdlib only. Builds synthetic Intel-HEX inputs, runs gen/verify as subprocesses,
and asserts the expected PASS/FAIL outcome for each case:

  1. golden bundle (gen from a synthetic P1)          -> verify PASS
  2. P1-only hex (no P2 UCA)                           -> verify FAIL (P1!=P2)
  3. P2 main present but backup missing                -> verify FAIL
  4. P2 main != backup                                 -> verify FAIL
  5. FDEVOPT.ALTI2C2 = OFF (bit4=1) on P1              -> gen refuses (exit!=0)
  6. wrong DFP expectation                             -> verify FAIL
  7. MPLAB project device/DFP/compiler mismatch         -> verify FAIL
  8. deterministic regen (same input => identical out) -> byte-identical
  9. XMODEM extraction excludes UCA/UCB                 -> extract only 0x800000+
     plus the DBFW v2 package contract: manifest+CRC valid, payload corruption /
     wrong magic / wrong project ID rejected, and trailing sender block padding
     tolerated up to the cap but not past it (truncated / manifest-only rejected)
 10. corrupt Intel HEX checksum                         -> extract FAIL
 11. over-limit program image                            -> extract FAIL, no file
     11b. config-only bundle, and code inside the BTSEQ-protection row
                                                         -> verify FAIL
 12. payload exactly at the limit                        -> extract PASS (the cap
     applies to the payload; the 16-byte manifest never reaches flash)
 13. the DBFW constants in src/fw_update/fw_update.c match this toolchain's
     constants, and the payload-cap macro still has its expected formula

Section 13 is the one that guards against silent C-vs-Python drift: the package
format is defined twice (firmware + host tool) and nothing else would catch an
edit to one side only.

Run: python tools/test_dual_partition_hex.py
Exit 0 = all pass.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ihex_lite
import uca_manifest as M
import extract_p1_image as E

PY = sys.executable
GEN = os.path.join(HERE, "gen_dual_partition_hex.py")
VERIFY = os.path.join(HERE, "verify_dual_partition_hex.py")
EXTRACT = os.path.join(HERE, "extract_p1_image.py")
PROJECT_CONFIG = os.path.join(HERE, "..", "firmware.X", "nbproject",
                              "configurations.xml")

PASS = 0
FAIL = 1


def write_hex(path, byte_dict):
    lines = ihex_lite.emit_records(byte_dict)
    with open(path, "w", newline="\r\n") as f:
        for ln in lines:
            f.write(ln + "\n")
        f.write(ihex_lite.EOF_RECORD + "\n")


def w32bytes(base, value):
    return {base + i: (value >> (8 * i)) & 0xFF for i in range(4)}


def synth_p1(alti2c2_on=True, with_program=True):
    """A minimal but valid P1 hex: FICD+FDEVOPT main+backup, FBOOT=DUAL, and a few
    program bytes so structural checks have something to see."""
    mem = {}
    fdevopt = 0xFFFFFFEF if alti2c2_on else (0xFFFFFFEF | M.FDEVOPT_ALTI2C2)
    mem.update(w32bytes(M.UCA_P1_MAIN + M.OFF_FICD, 0xFFFF7FDF))
    mem.update(w32bytes(M.UCA_P1_BACKUP + M.OFF_FICD, 0xFFFF7FDF))
    mem.update(w32bytes(M.UCA_P1_MAIN + M.OFF_FDEVOPT, fdevopt))
    mem.update(w32bytes(M.UCA_P1_BACKUP + M.OFF_FDEVOPT, fdevopt))
    mem.update(w32bytes(M.UCB_FBOOT, 0xFFFFFFFE))   # BTMODE=DUAL
    if with_program:
        for i in range(16):
            mem[M.PROGRAM_REGION_LO + i] = i
    return mem


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


results = []


def check(name, cond, detail=""):
    results.append((name, cond, detail))
    tag = "ok  " if cond else "FAIL"
    print(f"  [{tag}] {name}{('  -- ' + detail) if detail and not cond else ''}")


def main():
    tmp = tempfile.mkdtemp(prefix="uca_test_")

    # --- 1: golden bundle from a good P1 -> verify PASS
    p1 = os.path.join(tmp, "p1.hex")
    write_hex(p1, synth_p1())
    bundle = os.path.join(tmp, "p1.bundle.hex")
    rc, out = run([PY, GEN, p1, "-o", bundle])
    check("gen golden succeeds", rc == PASS, out)
    rc, out = run([PY, VERIFY, bundle])
    check("verify golden PASS", rc == PASS, out)

    # --- 2: P1-only (no clone) -> verify FAIL (P1!=P2)
    rc, out = run([PY, VERIFY, p1])
    check("verify P1-only FAIL", rc == FAIL, out)

    # --- 3: P2 main present, backup missing -> FAIL
    mem = synth_p1()
    mem.update(w32bytes(M.UCA_P2_MAIN + M.OFF_FICD, 0xFFFF7FDF))
    mem.update(w32bytes(M.UCA_P2_MAIN + M.OFF_FDEVOPT, 0xFFFFFFEF))
    # note: NOT adding P2 backup
    h = os.path.join(tmp, "p2_no_backup.hex")
    write_hex(h, mem)
    rc, out = run([PY, VERIFY, h])
    check("verify P2 backup-missing FAIL", rc == FAIL, out)

    # --- 4: P2 main != backup -> FAIL
    mem = synth_p1()
    mem.update(w32bytes(M.UCA_P2_MAIN + M.OFF_FICD, 0xFFFF7FDF))
    mem.update(w32bytes(M.UCA_P2_BACKUP + M.OFF_FICD, 0xFFFF7FDF))
    mem.update(w32bytes(M.UCA_P2_MAIN + M.OFF_FDEVOPT, 0xFFFFFFEF))
    mem.update(w32bytes(M.UCA_P2_BACKUP + M.OFF_FDEVOPT, 0xFFFFFFAF))  # differ
    h = os.path.join(tmp, "p2_mismatch.hex")
    write_hex(h, mem)
    rc, out = run([PY, VERIFY, h])
    check("verify P2 main!=backup FAIL", rc == FAIL, out)

    # --- 5: P1 ALTI2C2 OFF -> gen refuses
    p1bad = os.path.join(tmp, "p1_alti2c2_off.hex")
    write_hex(p1bad, synth_p1(alti2c2_on=False))
    rc, out = run([PY, GEN, p1bad, "-o", os.path.join(tmp, "x.hex")])
    check("gen refuses ALTI2C2=OFF source", rc != PASS, out)

    # --- 6: wrong DFP expectation -> verify FAIL
    rc, out = run([PY, VERIFY, bundle, "--expect-dfp", "wrong/0.0.0"])
    check("verify wrong-DFP FAIL", rc == FAIL, out)
    rc, out = run([PY, VERIFY, bundle, "--expect-dfp", M.DFP,
                   "--expect-device", M.DEVICE])
    check("verify correct DFP+device PASS", rc == PASS, out)

    # --- 7: production verification checks actual MPLAB X project metadata.
    rc, out = run([PY, VERIFY, bundle, "--project-config", PROJECT_CONFIG,
                   "--configuration", "dsPIC33AK512"])
    check("verify project device+DFP+compiler PASS", rc == PASS, out)
    with open(PROJECT_CONFIG, "r", encoding="utf-8") as f:
        project_xml = f.read()
    metadata_mutations = (
        ("device", M.DEVICE, "dsPIC33AK256MPS512"),
        ("DFP", 'version="1.3.185"', 'version="0.0.0"'),
        ("compiler", "<languageToolchainVersion>3.31.01</languageToolchainVersion>",
         "<languageToolchainVersion>0.0.0</languageToolchainVersion>"),
    )
    for label, old, new in metadata_mutations:
        bad_config = os.path.join(tmp, f"bad_{label}.xml")
        with open(bad_config, "w", encoding="utf-8") as f:
            f.write(project_xml.replace(old, new, 1))
        rc, out = run([PY, VERIFY, bundle, "--project-config", bad_config,
                       "--configuration", "dsPIC33AK512"])
        check(f"verify project {label} mismatch FAIL", rc == FAIL, out)

    # --- 8: deterministic regen
    b2 = os.path.join(tmp, "p1.bundle2.hex")
    rc, out = run([PY, GEN, p1, "-o", b2])
    with open(bundle, "rb") as f:
        a = f.read()
    with open(b2, "rb") as f:
        b = f.read()
    check("regen byte-identical", a == b)

    # --- 9: XMODEM extract excludes UCA/UCB
    mem = synth_p1()
    # add program bytes AND config bytes; extractor must keep only program.
    for i in range(512):
        mem[M.PROGRAM_REGION_LO + 0x100 + i] = (i & 0xFF)
    h = os.path.join(tmp, "extract_src.hex")
    write_hex(h, mem)
    binout = os.path.join(tmp, "img.bin")
    rc, out = run([PY, EXTRACT, h, binout])
    check("extract succeeds", rc == PASS, out)
    if rc == PASS:
        size = os.path.getsize(binout)
        # image starts at 0x800000; must not contain any 0x7Fxxxx bytes (impossible
        # by construction). The firmware's cap applies to the PAYLOAD -- the part
        # that reaches flash -- not the file: the 16-byte manifest is metadata. The
        # cap is partition 0x40000 minus the last 512-byte BTSEQ-protection row.
        check("extract payload within firmware limit",
              (size - E.PACKAGE_MANIFEST_BYTES) <= E.FW_MAX_IMAGE_BYTES,
              f"payload=0x{size - E.PACKAGE_MANIFEST_BYTES:X}")
        with open(binout, "rb") as f:
            package = f.read()
        package_ok, reason = E.validate_package(package)
        check("reflash package manifest+CRC valid", package_ok, reason)
        # v2: manifest leads, payload follows and is 512-byte row aligned.
        check("reflash package payload is row aligned",
              ((len(package) - E.PACKAGE_MANIFEST_BYTES) % 512) == 0,
              f"size={len(package)}")
        corrupt = bytearray(package)
        corrupt[E.PACKAGE_MANIFEST_BYTES] ^= 0x01
        corrupt_ok, _ = E.validate_package(corrupt)
        check("reflash package detects payload corruption", not corrupt_ok)
        corrupt = bytearray(package)
        corrupt[0] ^= 0x01
        corrupt_ok, _ = E.validate_package(corrupt)
        check("reflash package rejects wrong magic", not corrupt_ok)
        corrupt = bytearray(package)
        corrupt[6] ^= 0x01
        corrupt_ok, _ = E.validate_package(corrupt)
        check("reflash package rejects wrong project ID", not corrupt_ok)

        # --- 9b: THE regression the manifest-first (v2) layout exists for.
        # XMODEM carries no length, so a sender pads its final block to the full
        # block size, conventionally with 0x1A. Those bytes land after our payload
        # and MUST NOT cause a rejection -- this is what broke XMODEM-1K under the
        # v1 trailing-manifest layout on real hardware.
        for pad_len in (1, 16, 128, 896, 1023, 1024):
            padded = package + (b"\x1a" * pad_len)
            padded_ok, padded_reason = E.validate_package(padded)
            check(f"tolerates {pad_len}B of 0x1A sender padding",
                  padded_ok, padded_reason)
        # 0x00-padding senders exist too; same requirement.
        check("tolerates 0x00 sender padding",
              E.validate_package(package + (b"\x00" * 896))[0])
        # The tolerance is bounded, and the bound must match the firmware's
        # FW_PACKAGE_MAX_TRAILING_PAD exactly -- at the limit PASS, one over FAIL.
        check("tolerates padding exactly at the cap",
              E.validate_package(package + (b"\x1a" * E.PACKAGE_MAX_TRAILING_PAD))[0])
        check("rejects padding one byte over the cap",
              not E.validate_package(
                  package + (b"\x1a" * (E.PACKAGE_MAX_TRAILING_PAD + 1)))[0])
        # But a truncated payload must still fail (padding tolerance is not
        # permission to accept a short image).
        check("rejects truncated payload",
              not E.validate_package(package[:-1])[0])
        check("rejects manifest-only file",
              not E.validate_package(package[:E.PACKAGE_MANIFEST_BYTES])[0])

    # --- 10: every public HEX consumer rejects a bad Intel HEX checksum.
    bad_checksum_hex = os.path.join(tmp, "extract_bad_checksum.hex")
    with open(h, "r", encoding="ascii") as f:
        lines = f.readlines()
    for index, line in enumerate(lines):
        record = bytearray.fromhex(line.strip()[1:])
        if record[3] == 0x00:
            record[-1] ^= 0x01
            lines[index] = ":" + record.hex().upper() + "\n"
            break
    with open(bad_checksum_hex, "w", encoding="ascii") as f:
        f.writelines(lines)
    bad_checksum_bin = os.path.join(tmp, "bad_checksum.bin")
    rc, out = run([PY, EXTRACT, bad_checksum_hex, bad_checksum_bin])
    check("extract rejects bad Intel HEX checksum", rc == FAIL, out)
    check("bad-checksum extract wrote no file", not os.path.exists(bad_checksum_bin))

    # --- 11: extractor refuses an over-limit image (writes nothing)
    mem = synth_p1()
    # populate a byte in the last row so the rounded slice would exceed 0x3FE00.
    mem[M.PROGRAM_REGION_LO + 0x3FE00] = 0x5A
    h2 = os.path.join(tmp, "extract_toobig.hex")
    write_hex(h2, mem)
    big = os.path.join(tmp, "toobig.bin")
    rc, out = run([PY, EXTRACT, h2, big])
    check("extract refuses over-limit image", rc == FAIL, out)
    check("extract wrote no file when over limit", not os.path.exists(big))

    # --- 11b: a verified bundle must attest a *serially updatable* image, not just
    # sane config words. Without these, a config-only bundle -- or one whose code
    # reaches the BTSEQ-protection row that the updater refuses to program -- would
    # get a PASS report that flashauto.ps1 accepts.
    noprog = os.path.join(tmp, "noprog.hex")
    write_hex(noprog, synth_p1(with_program=False))
    noprog_bundle = os.path.join(tmp, "noprog.bundle.hex")
    rc, out = run([PY, GEN, noprog, "-o", noprog_bundle])
    if rc == PASS:
        rc, out = run([PY, VERIFY, noprog_bundle])
        check("verify config-only bundle (no program) FAIL", rc == FAIL, out)
    else:
        check("verify config-only bundle (no program) FAIL", True, "gen refused first")

    # One byte inside the protected row must fail; the byte just below it must pass.
    for label, addr, want in (
        ("first byte of BTSEQ row", M.BTSEQ_PROTECTED_LO, FAIL),
        ("last byte below BTSEQ row", M.BTSEQ_PROTECTED_LO - 1, PASS),
    ):
        mem = synth_p1()
        mem[addr] = 0xA5
        h = os.path.join(tmp, f"row_{addr:06X}.hex")
        write_hex(h, mem)
        b = os.path.join(tmp, f"row_{addr:06X}.bundle.hex")
        rc, out = run([PY, GEN, h, "-o", b])
        if rc != PASS:
            check(f"verify {label}", False, f"gen failed: {out}")
            continue
        rc, out = run([PY, VERIFY, b])
        check(f"verify {label} -> {'FAIL' if want == FAIL else 'PASS'}",
              rc == want, out)

    # --- 12: the limit applies to the PAYLOAD, not the file. Under v2 the 16-byte
    # manifest is metadata that never reaches flash, so a payload of exactly
    # FW_MAX_IMAGE_BYTES (partition minus the BTSEQ row) is legal even though the
    # file is 16 bytes larger. Check 11 above covers the over-limit side.
    mem = synth_p1()
    mem[M.PROGRAM_REGION_LO + 0x3FDFF] = 0xA5  # raw rounds to exactly 0x3FE00
    h3 = os.path.join(tmp, "extract_at_limit.hex")
    write_hex(h3, mem)
    at_limit = os.path.join(tmp, "at_limit.bin")
    rc, out = run([PY, EXTRACT, h3, at_limit])
    check("extract accepts payload exactly at the limit", rc == PASS, out)
    if rc == PASS:
        check("at-limit payload excludes manifest from the cap",
              os.path.getsize(at_limit) == E.FW_MAX_IMAGE_BYTES + E.PACKAGE_MANIFEST_BYTES,
              f"size=0x{os.path.getsize(at_limit):X}")

    # --- 13: the DBFW package contract lives in BOTH src/fw_update/fw_update.c and
    # this toolchain. Nothing else notices if one side is edited alone, and the
    # failure mode is nasty: the board silently rejects every image the host builds.
    # Parse the C defines and compare.
    fw_c = os.path.join(HERE, "..", "src", "fw_update", "fw_update.c")
    nvm_h = os.path.join(HERE, "..", "src", "hal_nvm", "dspic33ak_nvm.h")
    try:
        with open(fw_c, "r", encoding="utf-8", errors="replace") as f:
            csrc = f.read()
        with open(nvm_h, "r", encoding="utf-8", errors="replace") as f:
            nvmsrc = f.read()
    except OSError as exc:
        check("firmware sources readable for constant check", False, str(exc))
        csrc = nvmsrc = ""

    if csrc and nvmsrc:
        def cmacro(text, name):
            """Raw replacement text of `#define <name> ...`, comment stripped."""
            m = re.search(r"^#define\s+" + re.escape(name) + r"\s+(.+?)\s*(?://.*)?$",
                          text, re.MULTILINE)
            return m.group(1).strip() if m else None

        def cdef(text, name):
            """Integer value of `#define <name> <literal>`, else None.

            Deliberately STRICT: after peeling wrapping parens, UINTxx_C() and
            u/U/l/L suffixes, the remainder must be a single integer literal or this
            returns None and the comparison below fails. A lenient "first number
            wins" parser would read `(2u + 1u)` as 2 and silently agree with a
            Python-side 2 -- precisely the drift this section exists to catch. It
            would also mis-read UINT32_C(...) as 32.
            """
            expr = cmacro(text, name)
            if expr is None:
                return None
            for _ in range(6):                        # peel until stable
                before = expr
                expr = re.sub(r"\bUINT\d+_C\s*\(([^()]*)\)", r"\1", expr).strip()
                if expr.startswith("(") and expr.endswith(")"):
                    expr = expr[1:-1].strip()
                expr = re.sub(r"(?<=[0-9a-fA-F])[uUlL]+\Z", "", expr).strip()
                if expr == before:
                    break
            m2 = re.fullmatch(r"0[xX][0-9a-fA-F]+|\d+", expr)
            return int(m2.group(0), 0) if m2 else None

        row_bytes = cdef(nvmsrc, "DSPIC33AK_NVM_ROW_BYTES")
        partition = cdef(csrc, "FW_PARTITION_BYTES")
        pairs = [
            ("manifest bytes", cdef(csrc, "FW_PACKAGE_MANIFEST_BYTES"),
             E.PACKAGE_MANIFEST_BYTES),
            ("format version", cdef(csrc, "FW_PACKAGE_VERSION"), E.PACKAGE_VERSION),
            ("project id", cdef(csrc, "FW_PACKAGE_PROJECT_ID"), E.PACKAGE_PROJECT_ID),
            ("max trailing pad", cdef(csrc, "FW_PACKAGE_MAX_TRAILING_PAD"),
             E.PACKAGE_MAX_TRAILING_PAD),
            ("partition bytes", partition, M.PARTITION_BYTES),
            ("nvm row bytes", row_bytes, M.NVM_ROW_BYTES),
        ]
        for label, c_val, py_val in pairs:
            check(f"firmware and host agree on {label}",
                  c_val is not None and c_val == py_val,
                  f"C={c_val!r} python={py_val!r}")
        # FW_MAX_IMAGE_BYTES is an expression, so comparing recomputed values is not
        # enough: C could become (FW_PARTITION_BYTES - 2 * ROW_BYTES) while both
        # operands keep their values, and a value-only check would still pass. Pin
        # the formula itself, then check the value it yields.
        cap_expr = cmacro(csrc, "FW_MAX_IMAGE_BYTES")
        expected_expr = "(FW_PARTITION_BYTES - DSPIC33AK_NVM_ROW_BYTES)"
        normalize = lambda s: re.sub(r"\s+", " ", s).strip() if s else s
        check("firmware payload cap is still partition minus exactly one row",
              normalize(cap_expr) == normalize(expected_expr),
              f"C={cap_expr!r} expected={expected_expr!r}")
        if partition is not None and row_bytes is not None:
            check("firmware and host agree on the payload cap value",
                  (partition - row_bytes) == M.MAX_PAYLOAD_BYTES,
                  f"C={partition - row_bytes:#x} python={M.MAX_PAYLOAD_BYTES:#x}")

    ok = all(c for _, c, _ in results)
    print(f"\n{'ALL PASS' if ok else 'FAILURES PRESENT'}: "
          f"{sum(1 for _,c,_ in results if c)}/{len(results)} checks passed")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
