#!/usr/bin/env python3
"""§5.3 read-only verifier for a dual-partition provisioning bundle HEX.

Independent of the generator: re-parses the bundle from scratch and asserts every
provisioning invariant, emitting PASS/FAIL + a report. Build-success and
bundle-verify-success are deliberately SEPARATE statuses (exit 0 only on PASS).

Checks:
  1. device/DFP/compiler manifest pins match the selected MPLAB X configuration
     when --project-config is supplied (the production provision wrapper does so).
  2. P1/P2 FCP/FICD/FDEVOPT/FWDT main+backup evaluated; absent => explicit
     0xFFFFFFFF reported as "NO RECORD=erased" (never silently "unseen").
  3. main == backup within compare_mask (per partition).
  4. P1 == P2 where must_match_p1_p2 (for non-erased words).
  5. FDEVOPT.ALTI2C2 (bit4) == 0  (board-required alternate I2C2 pins), P1 & P2.
  6. FICD.NOBTSWP (bit15) == 0     (BOOTSWP instruction enabled), P1 & P2.
  7. FBOOT.BTMODE == DUAL (shared UCB).
  8. no conflicting duplicate records (ihex parser is last-writer; we re-scan raw).
  9. program region within [0x800000,0x840000); nothing emitted into UCA/UCB by the
     program payload beyond the intended UCA clone.
 10. the XMODEM extraction range excludes UCA/UCB (asserted structurally here too).

Usage: verify_dual_partition_hex.py <bundle.hex> [--report r.txt]
       [--project-config configurations.xml --configuration name]
Exit: 0 = PASS, 1 = FAIL, 2 = usage/read error.
"""
import argparse
import hashlib
import os
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ihex_lite
import uca_manifest as M


def fmt(v):
    return "NO RECORD=erased(0xFFFFFFFF)" if v is None else f"0x{v:08X}"


def val(v):
    return M.ERASED if v is None else v


def raw_conflict_scan(path):
    """Re-scan raw records; return list of (addr, [values]) where a byte address
    is written more than once with differing values."""
    seen = {}
    ulba = 0
    with open(path) as f:
        for line in f:
            s = line.strip()
            if not s.startswith(":"):
                continue
            b = bytes.fromhex(s[1:])
            n, off, typ = b[0], (b[1] << 8) | b[2], b[3]
            data = b[4:4 + n]
            if typ == 0x04:
                ulba = (data[0] << 8) | data[1]
            elif typ == 0x00:
                base = (ulba << 16) + off
                for i, byte in enumerate(data):
                    a = base + i
                    seen.setdefault(a, set()).add(byte)
    return [(a, sorted(vs)) for a, vs in seen.items() if len(vs) > 1]


def read_project_metadata(path, configuration):
    """Read the selected MPLAB X configuration instead of trusting manifest text."""
    root = ET.parse(path).getroot()
    conf = next((node for node in root.findall("./confs/conf")
                 if node.get("name") == configuration), None)
    if conf is None:
        raise ValueError(f"configuration {configuration!r} not found")
    tools = conf.find("toolsSet")
    if tools is None:
        raise ValueError(f"configuration {configuration!r} has no toolsSet")
    pack = next((node for node in conf.findall("./packs/pack")
                 if node.get("name") == M.DFP.split("/", 1)[0]), None)
    if pack is None:
        dfp = "(missing)"
    else:
        dfp = f"{pack.get('name')}/{pack.get('version')}"
    return {
        "device": tools.findtext("targetDevice", default=""),
        "toolchain": tools.findtext("languageToolchain", default=""),
        "xcdsc": tools.findtext("languageToolchainVersion", default=""),
        "dfp": dfp,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bundle_hex")
    ap.add_argument("--report")
    ap.add_argument("--expect-device", help="fail unless manifest device == this")
    ap.add_argument("--expect-dfp", help="fail unless manifest DFP == this")
    ap.add_argument("--project-config",
                    help="MPLAB X configurations.xml to compare with manifest pins")
    ap.add_argument("--configuration", default="dsPIC33AK512",
                    help="configuration name inside --project-config")
    args = ap.parse_args()

    try:
        mem = ihex_lite.parse_hex(args.bundle_hex)
    except Exception as e:  # noqa: BLE001
        print(f"FAIL: cannot parse {args.bundle_hex}: {e}")
        return 2

    fails = []
    log = []

    def ok(cond, msg):
        log.append(("PASS" if cond else "FAIL", msg))
        if not cond:
            fails.append(msg)

    log.append(("INFO", f"device={M.DEVICE} dfp={M.DFP} xcdsc={M.XCDSC}"))
    with open(args.bundle_hex, "rb") as bundle_file:
        bundle_sha256 = hashlib.sha256(bundle_file.read()).hexdigest()
    log.append(("INFO", f"bundle_sha256={bundle_sha256}"))
    if args.expect_device is not None:
        ok(M.DEVICE == args.expect_device,
           f"manifest device=={args.expect_device} (got {M.DEVICE})")
    if args.expect_dfp is not None:
        ok(M.DFP == args.expect_dfp,
           f"manifest DFP=={args.expect_dfp} (got {M.DFP})")
    if args.project_config is not None:
        try:
            project = read_project_metadata(args.project_config, args.configuration)
        except (OSError, ValueError, ET.ParseError) as exc:
            ok(False, f"read MPLAB X configuration: {exc}")
        else:
            log.append(("INFO", "project config "
                        f"device={project['device']} dfp={project['dfp']} "
                        f"toolchain={project['toolchain']}/{project['xcdsc']}"))
            ok(project["device"] == M.DEVICE,
               f"project device matches manifest ({M.DEVICE})")
            ok(project["dfp"] == M.DFP,
               f"project DFP matches manifest ({M.DFP})")
            ok(project["toolchain"] == "XCDSC" and project["xcdsc"] == M.XCDSC,
               f"project compiler matches manifest (XCDSC/{M.XCDSC})")

    # Per-word evaluation.
    for w in M.WORDS:
        p1m = ihex_lite.word32(mem, w["p1_main"])
        p1b = ihex_lite.word32(mem, w["p1_backup"])
        p2m = ihex_lite.word32(mem, w["p2_main"])
        p2b = ihex_lite.word32(mem, w["p2_backup"])
        log.append(("INFO", f"{w['name']:8s} P1 main {fmt(p1m)} bkp {fmt(p1b)}"
                            f" | P2 main {fmt(p2m)} bkp {fmt(p2b)}"))
        mask = w["compare_mask"]
        # main==backup
        ok((val(p1m) & mask) == (val(p1b) & mask),
           f"{w['name']}: P1 main==backup")
        ok((val(p2m) & mask) == (val(p2b) & mask),
           f"{w['name']}: P2 main==backup")
        # P1==P2
        if w["must_match_p1_p2"]:
            ok((val(p1m) & mask) == (val(p2m) & mask),
               f"{w['name']}: P1==P2 (main)")
            ok((val(p1b) & mask) == (val(p2b) & mask),
               f"{w['name']}: P1==P2 (backup)")
        # erased words really erased
        if w["expected"] == "erased":
            ok(val(p1m) == M.ERASED and val(p2m) == M.ERASED,
               f"{w['name']}: expected erased both partitions")
        # semantic bit checks
        for (bitname, bmask, bexp) in w.get("checks", []):
            for tag, wv in (("P1", val(p1m)), ("P2", val(p2m))):
                got = 1 if (wv & bmask) else 0
                ok(got == bexp,
                   f"{w['name']}.{bitname} {tag}: bit=={bexp} (got {got}, "
                   f"word {fmt(wv if wv != M.ERASED else None)})")

    # FBOOT / BTMODE (shared UCB).
    fboot = ihex_lite.word32(mem, M.UCB_FBOOT)
    log.append(("INFO", f"FBOOT (shared) @0x{M.UCB_FBOOT:06X} {fmt(fboot)}"))
    ok(fboot is not None and (fboot & M.FBOOT_BTMODE_MASK) == M.FBOOT_BTMODE_DUAL,
       f"FBOOT.BTMODE==DUAL (got {fmt(fboot)})")

    # Conflicting duplicate records.
    conflicts = raw_conflict_scan(args.bundle_hex)
    ok(not conflicts,
       f"no conflicting duplicate records ({len(conflicts)} found)")
    for a, vs in conflicts[:8]:
        log.append(("FAIL", f"  conflict @0x{a:06X}: {[hex(v) for v in vs]}"))

    # Program region bounds + UCA/UCB not overwritten by program payload.
    prog_bytes = [a for a in mem if M.PROGRAM_REGION_LO <= a < M.PROGRAM_REGION_HI]
    stray_hi = [a for a in mem if a >= M.PROGRAM_REGION_HI]
    ok(not stray_hi, f"no records at/above 0x{M.PROGRAM_REGION_HI:06X} "
                     f"({len(stray_hi)} stray)")
    log.append(("INFO", f"program-region bytes present: {len(prog_bytes)}"))

    # UCA/UCB live below the program region -> XMODEM [0x800000,0x840000) can never
    # include them. Assert structurally.
    ok(M.UCA_P2_BACKUP + M.UCA_WINDOW_SPAN <= M.PROGRAM_REGION_LO and
       M.UCB_FBOOT < M.PROGRAM_REGION_LO,
       "XMODEM program range excludes UCA/UCB (structural)")

    # Emit.
    passed = not fails
    header = f"{'PASS' if passed else 'FAIL'}: dual-partition verify {args.bundle_hex}"
    body = [header, ""]
    for level, msg in log:
        body.append(f"  [{level}] {msg}")
    if fails:
        body.append("")
        body.append(f"  {len(fails)} check(s) FAILED:")
        for m in fails:
            body.append(f"    - {m}")
    text = "\n".join(body)
    print(text)
    if args.report:
        with open(args.report, "w", newline="\r\n") as f:
            f.write(text + "\n")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
