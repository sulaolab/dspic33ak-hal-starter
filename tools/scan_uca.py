#!/usr/bin/env python3
# Read-only scan of an Intel HEX for the dual-partition UCA config words.
# Reports the value present at each address, or "NO RECORD (=erased 0xFFFFFFFF)".
import sys

import ihex_lite

if len(sys.argv) != 2:
    sys.exit("usage: scan_uca.py <production-or-bundle.hex>")
HEX = sys.argv[1]

try:
    mem = ihex_lite.parse_hex(HEX)
except (OSError, ValueError) as exc:
    sys.exit(f"cannot parse {HEX}: {exc}")

targets = [
    ("UCA1/P1 FCP     main", 0x7F3000), ("UCA1/P1 FCP     backup", 0x7F3800),
    ("UCA1/P1 FICD    main", 0x7F3010), ("UCA1/P1 FICD    backup", 0x7F3810),
    ("UCA1/P1 FDEVOPT main", 0x7F3020), ("UCA1/P1 FDEVOPT backup", 0x7F3820),
    ("UCA1/P1 FWDT    main", 0x7F3030), ("UCA1/P1 FWDT    backup", 0x7F3830),
    ("UCA2/P2 FCP     main", 0x7FB000), ("UCA2/P2 FCP     backup", 0x7FB800),
    ("UCA2/P2 FICD    main", 0x7FB010), ("UCA2/P2 FICD    backup", 0x7FB810),
    ("UCA2/P2 FDEVOPT main", 0x7FB020), ("UCA2/P2 FDEVOPT backup", 0x7FB820),
    ("UCA2/P2 FWDT    main", 0x7FB030), ("UCA2/P2 FWDT    backup", 0x7FB830),
    ("UCB FBOOT (shared)   ", 0x7F40D0),
]
print(f"# {HEX}")
for name, a in targets:
    w = ihex_lite.word32(mem, a)
    if w is None:
        print(f"  {name} @0x{a:06X}  NO RECORD (=erased 0xFFFFFFFF)")
    else:
        note = ""
        if a in (0x7F3010, 0x7F3810, 0x7FB010, 0x7FB810):
            note = ("  NOBTSWP(bit15)=0 -> BOOTSWP ENABLED" if not (w >> 15) & 1
                    else "  NOBTSWP(bit15)=1 -> BOOTSWP DISABLED")
        if a in (0x7F3020, 0x7F3820, 0x7FB020, 0x7FB820):
            note = f"  ALTI2C2(bit4)={'0->ON(ASCL2/ASDA2)' if not (w>>4)&1 else '1->OFF(SCL2/SDA2)'}"
        print(f"  {name} @0x{a:06X}  0x{w:08X}{note}")

# also report any records anywhere in 0x7FB000..0x7FB840
present = [a for a in mem if 0x7FB000 <= a <= 0x7FB83F]
print(f"\n# bytes present in P2-UCA region 0x7FB000..0x7FB83F: {len(present)}")
