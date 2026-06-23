# hal_udid -- dsPIC33AK Unique Device ID (board-individual identity)

`src/hal_udid/dspic33ak_udid.{c,h}` reads the dsPIC33AK **UDID**: a 128-bit,
factory-programmed, read-only value that is **unique per physical die**. Use it to
tell one board apart from another (provisioning, per-unit logs, "which board am I?"
diagnostics).

It is NOT the same as:

| Identifier | What it is | Same across identical parts? |
|---|---|---|
| **DEVID** | device type / variant ID | yes (same for every part of a variant) |
| Debugger USB serial | identifies the PKOB4 / programmer | n/a (not the target) |
| **UDID** | the individual die | **no -- unique per chip** |

## Memory layout

Four 32-bit read-only words in the unified (linear) address space:

| Name | Address | Meaning |
|---|---|---|
| UDID1 | `0x007F2BE0` | bits 31:0 |
| UDID2 | `0x007F2BE4` | bits 63:32 |
| UDID3 | `0x007F2BE8` | bits 95:64 |
| UDID4 | `0x007F2BEC` | bits 127:96 |

Same addresses across the dsPIC33AK128MC106 and dsPIC33AK512MC510 /
dsPIC33AK512MPS512 families. Reference: the device Programming Specification,
"Unique Device ID" (Section 1.3).

On dsPIC33A the words are read with a plain `volatile const uint32_t *` -- the
program/UDID space is in the unified address space, so there is **no PSV or
table-read** setup (unlike classic dsPIC33C/E/F).

## API

```c
dspic33ak_udid_t udid;
if (DSPIC33AK_UDID_Read(&udid)) {
    // udid.word[0]=UDID1 ... udid.word[3]=UDID4
}
```

- `DSPIC33AK_UDID_Read()` reads the four words and returns `true` only when the
  value is plausible. It returns `false` for a NULL pointer, or for an all-zero /
  all-one read (the signatures of a failed / unprogrammed read).
- `DSPIC33AK_UDID_IsPlausible()` is the pure all-zero / all-one check (no hardware
  access).

## Boot banner

`src/main.c` prints the UDID on the startup banner so each board's serial log
self-identifies. `UDID128` is the four words concatenated **UDID4..UDID1** (high
word first):

```text
 device : dsPIC33AK512MPS512
 udid   : FFFFFFFF00EA010F5608000400D7794B
```

Two boards from the same production lot typically share UDID1/UDID2 (lot/wafer)
and differ in UDID3 (die X/Y position); always compare the full 128-bit value.
