# ARM64 (AArch64) System Register Specification

**Source**: ARM Architecture Reference Manual for ARMv8-A
**Applicable to**: SLOPIX kernel running at EL1

## SCTLR_EL1 - System Control Register (EL1)

Controls system features including MMU, caches, and alignment checking.

### Key Bit Fields

| Bit(s) | Field   | Description                                      | Reset |
|--------|---------|--------------------------------------------------|-------|
| 0      | M       | MMU enable for EL1&0 stage 1 address translation | 0     |
| 1      | A       | Alignment fault checking enable                  | 0     |
| 2      | C       | Data/unified cache enable                        | 0     |
| 3      | SA      | SP alignment check enable for EL1                | 0     |
| 4      | SA0     | SP alignment check enable for EL0                | 0     |
| 12     | I       | Instruction cache enable                         | 0     |
| 19     | WXN     | Write permission implies XN (execute never)      | 0     |
| 24     | E0E     | Endianness of data at EL0 (0=little, 1=big)     | 0     |
| 25     | EE      | Endianness of data at EL1 (0=little, 1=big)     | 0     |
| 26     | UCI     | Trap EL0 cache instructions to EL1               | 0     |

### Common Initialization Value

For basic MMU enable with caches:
```
SCTLR_EL1 = 0x1005  // M=1, C=1, I=1 (bits 0, 2, 12)
```

---

## TCR_EL1 - Translation Control Register (EL1)

Controls address translation and granule sizes.

### Key Bit Fields

| Bit(s) | Field      | Description                                    |
|--------|------------|------------------------------------------------|
| 5:0    | T0SZ       | Size offset for TTBR0_EL1 region              |
| 7:6    | Reserved   | RES0                                           |
| 9:8    | IRGN0      | Inner cacheability for TTBR0 table walks      |
| 11:10  | ORGN0      | Outer cacheability for TTBR0 table walks      |
| 13:12  | SH0        | Shareability for TTBR0 region                 |
| 15:14  | TG0        | Granule size for TTBR0 (00=4KB, 01=64KB, 10=16KB) |
| 21:16  | T1SZ       | Size offset for TTBR1_EL1 region              |
| 23     | A1         | ASID selection (0=TTBR0, 1=TTBR1)             |
| 25:24  | IRGN1      | Inner cacheability for TTBR1 table walks      |
| 27:26  | ORGN1      | Outer cacheability for TTBR1 table walks      |
| 29:28  | SH1        | Shareability for TTBR1 region                 |
| 31:30  | TG1        | Granule size for TTBR1                        |
| 34:32  | IPS        | Intermediate Physical Address Size            |

### T0SZ/T1SZ Values

`TxSZ` determines the size of the address space:
- Address space size = 2^(64-TxSZ) bytes
- For 39-bit address space: TxSZ = 25 (2^39 = 512GB)
- For 48-bit address space: TxSZ = 16 (2^48 = 256TB)

### Cacheability Values (IRGN/ORGN)

- `00`: Non-cacheable
- `01`: Write-back, write-allocate
- `10`: Write-through, no write-allocate
- `11`: Write-back, no write-allocate

### Shareability Values (SH)

- `00`: Non-shareable
- `01`: Reserved
- `10`: Outer shareable
- `11`: Inner shareable

---

## TTBR0_EL1 / TTBR1_EL1 - Translation Table Base Registers

Hold physical addresses of level 0 or level 1 translation tables.

### Format

| Bit(s) | Field   | Description                              |
|--------|---------|------------------------------------------|
| 63:48  | ASID    | Address Space ID (if using ASID)        |
| 47:1   | BADDR   | Base address of translation table       |
| 0      | CnP     | Common not Private (ARMv8.2+)           |

### Notes

- `BADDR` must be aligned to table size (4KB for 4KB granule)
- Which TTBR is used depends on VA[63:0]:
  - Top bits 0 → use TTBR0_EL1 (typically user space)
  - Top bits 1 → use TTBR1_EL1 (typically kernel space)

---

## MAIR_EL1 - Memory Attribute Indirection Register

Defines memory attributes for up to 8 attribute indices.

### Format

64-bit register divided into 8 attribute fields (Attr0-Attr7):
- Bits 7:0   → Attr0
- Bits 15:8  → Attr1
- Bits 23:16 → Attr2
- ... (8 bytes total)

### Memory Attribute Encoding

Each 8-bit Attr field encodes memory type:

**Normal Memory** (bits 7:4 != 0000):
- Bits 7:4: Outer cacheability
- Bits 3:0: Inner cacheability

Cacheability values:
- `0000`: Non-cacheable
- `0100`: Write-through, transient
- `01RW`: Write-through, R=read-allocate, W=write-allocate
- `10RW`: Write-back, transient, R=read-allocate, W=write-allocate
- `11RW`: Write-back, R=read-allocate, W=write-allocate

**Device Memory** (bits 7:4 = 0000):
- `0000 0000`: Device-nGnRnE (most restrictive)
- `0000 0100`: Device-nGnRE
- `0000 1000`: Device-nGRE
- `0000 1100`: Device-GRE

### Common Values

```
MAIR_EL1 = 0x000000000044FF00
  Attr0 (0x00): Device-nGnRnE
  Attr1 (0xFF): Normal memory, write-back read/write-allocate
  Attr2 (0x44): Normal memory, non-cacheable
```

---

## SPSR_EL1 - Saved Program Status Register

Saves processor state when taking an exception to EL1.

### Key Bit Fields

| Bit(s) | Field | Description                              |
|--------|-------|------------------------------------------|
| 3:0    | M     | Exception level and SP selection mode    |
| 4      | M4    | Execution state (0=AArch64, 1=AArch32)   |
| 6      | F     | FIQ mask bit                             |
| 7      | I     | IRQ mask bit                             |
| 8      | A     | SError mask bit                          |
| 9      | D     | Debug exception mask bit                 |
| 20     | IL    | Illegal execution state bit              |
| 21     | SS    | Software step bit                        |
| 28     | V     | Overflow condition flag                  |
| 29     | C     | Carry condition flag                     |
| 30     | Z     | Zero condition flag                      |
| 31     | N     | Negative condition flag                  |

### M Field Values (bits 3:0)

- `0b0000` (0x0): EL0t - EL0 with SP_EL0
- `0b0100` (0x4): EL1t - EL1 with SP_EL0
- `0b0101` (0x5): EL1h - EL1 with SP_EL1
- `0b1000` (0x8): EL2t - EL2 with SP_EL0
- `0b1001` (0x9): EL2h - EL2 with SP_EL2

---

## ELR_EL1 - Exception Link Register

Holds the address to return to when executing `ERET` from EL1.

### Format

64-bit register containing return address:
- Set by hardware on exception entry
- Used by `ERET` instruction to restore PC
- Can be read/written via `MRS`/`MSR` instructions

---

## ESR_EL1 - Exception Syndrome Register

Provides information about the exception that caused entry to EL1.

### Key Bit Fields

| Bit(s) | Field | Description                              |
|--------|-------|------------------------------------------|
| 31:26  | EC    | Exception Class                          |
| 25     | IL    | Instruction length (0=16bit, 1=32bit)    |
| 24:0   | ISS   | Instruction Specific Syndrome            |

### Exception Class (EC) Values

- `0x00`: Unknown reason
- `0x01`: Trapped WFI/WFE instruction
- `0x15`: SVC instruction execution in AArch64
- `0x20`: Instruction abort from lower EL
- `0x21`: Instruction abort from same EL
- `0x24`: Data abort from lower EL
- `0x25`: Data abort from same EL

---

## References

- ARM Architecture Reference Manual for ARMv8-A: https://developer.arm.com/documentation/ddi0487/
- ARM Cortex-A Series Programmer's Guide: https://developer.arm.com/documentation/den0024/
- Jon's ARM Reference (unofficial): https://arm.jonpalmisc.com/
