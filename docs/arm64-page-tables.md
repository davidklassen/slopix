# ARM64 Page Table Architecture for SLOPIX

**Source**: ARM Architecture Reference Manual for ARMv8-A (ARM ARM)
**Relevant Sections**: D4.2 (VMSAv8-64 Translation), D4.3 (Descriptor Formats)
**SLOPIX Configuration**: T0SZ=25 (39-bit VA), 4KB granule, identity mapping

---

## Introduction

ARM64 (ARMv8-A) uses a multi-level page table structure called VMSAv8-64 (Virtual Memory System Architecture version 8, 64-bit). The number of translation table levels and the starting level depend on:
1. **Virtual address size** (determined by T0SZ in TCR_EL1)
2. **Translation granule size** (4KB, 16KB, or 64KB)

SLOPIX uses:
- **T0SZ = 25** → 39-bit virtual address space (2^39 = 512GB)
- **4KB granule** → 4096-byte pages and translation tables
- **Identity mapping** → Virtual address = Physical address

---

## Starting Level Determination

**ARM ARM Reference**: D4.2.7 "Selection between TTBR0 and TTBR1"

The starting level of translation is determined by the input address (IA) size:
```
IA size = 64 - T0SZ
```

For SLOPIX with T0SZ = 25:
```
IA size = 64 - 25 = 39 bits
```

**Key Principle from ARM ARM**: For 4KB granule, Level 0 is skipped when IA ≤ 39 bits.

### Starting Level Table (4KB Granule)

| T0SZ | IA Bits | Address Space | Starting Level | Max Levels |
|------|---------|---------------|----------------|------------|
| 16   | 48-bit  | 256 TB        | Level 0        | 4          |
| 25   | 39-bit  | 512 GB        | **Level 1**    | **3**      |
| 28   | 36-bit  | 64 GB         | Level 1        | 3          |
| 34   | 30-bit  | 1 GB          | Level 2        | 2          |

**For SLOPIX**: Translation starts at **Level 1** (Level 0 is skipped).

---

## Translation Table Levels for SLOPIX

With 39-bit VA and 4KB granule, SLOPIX uses a **2-level** page table structure:

### Level 1 (Starting Level)
- **VA Bits Used**: [38:30] (9 bits)
- **Number of Entries**: 2^9 = 512 entries
- **Coverage Per Entry**: 1 GB (2^30 bytes)
- **Total Coverage**: 512 × 1 GB = 512 GB
- **Table Size**: 512 entries × 8 bytes = 4096 bytes (one page)
- **Pointed To By**: TTBR0_EL1

### Level 2 (Next Level)
- **VA Bits Used**: [29:21] (9 bits)
- **Number of Entries**: 2^9 = 512 entries
- **Coverage Per Entry**: 2 MB (2^21 bytes) - **block descriptor**
- **Total Coverage**: 512 × 2 MB = 1 GB (per L2 table)
- **Table Size**: 512 entries × 8 bytes = 4096 bytes (one page)
- **Pointed To By**: Level 1 table descriptors

### Level 3 (Not Used by SLOPIX)
- Would provide 4KB page granularity
- SLOPIX uses 2MB blocks at Level 2 instead

### Page Offset
- **VA Bits Used**: [20:0] (21 bits for 2MB blocks)
- **Purpose**: Offset within the 2MB memory block

---

## Table Descriptor Format

**ARM ARM Reference**: D4.3.1 "VMSAv8-64 translation table level 0, level 1, and level 2 descriptor formats"

Table descriptors provide the address of the next-level translation table.

### Bit Field Layout (64-bit descriptor)

```
 63  59 58       48 47            12 11  2  1  0
+------+-----------+----------------+-----+--+--+
| RES0 |   Ignored |  Table Address | IGN | 1| 1|
+------+-----------+----------------+-----+--+--+
 Upper               Next-level PA        Type Valid
 Attrs                                   (Table)
```

### Field Descriptions

| Bits    | Field         | Description                                      | SLOPIX Value |
|---------|---------------|--------------------------------------------------|--------------|
| [0]     | Valid         | Descriptor valid (must be 1)                     | 1            |
| [1]     | Type          | 1 = Table descriptor                             | 1            |
| [11:2]  | Ignored       | Ignored by hardware                              | 0            |
| [47:12] | Table Address | Physical address of next-level table (4KB aligned) | Varies     |
| [51:48] | Ignored       | Ignored by hardware                              | 0            |
| [58:52] | Ignored       | Ignored by hardware                              | 0            |
| [63:59] | RES0          | Reserved, must be 0 for EL1&0 stage 1           | 0            |

**Important**: Bits [47:12] contain the physical address of the next-level table. The address must be 4KB-aligned (bits [11:0] are implicitly zero).

### SLOPIX Example

From `mmu.c`, L1[0] entry: `0x40008003`

```
Binary: 0000 0000 0000 0000 0100 0000 0000 0000 1000 0000 0000 0011
Hex:                         4    0    0    0    8    0    0    3

Bit [0]:     1  → Valid
Bit [1]:     1  → Table descriptor
Bits [47:12]: 0x40008 → Physical address 0x40008000 (L2 table)
```

This means L1[0] points to the L2 table at physical address **0x40008000**.

---

## Block Descriptor Format for 2MB Blocks

**ARM ARM Reference**: D4.3.1, D4.3.3 "Memory attribute fields"

Block descriptors at Level 2 define 2MB regions of physical memory.

### Bit Field Layout (64-bit descriptor)

```
 63 59 58    55 54 53 52 51    48 47             21 20  12 11 10 9  8 7  6 5 4   2  1  0
+-----+--------+--+--+--+--------+------------------+------+--+--+----+----+-+-----+--+--+
| Ign.| SW use |XN|PXN|C|  Upper |  Output Address  | RES0 |nG|AF| SH | AP |0|AtIx| 0| 1|
+-----+--------+--+--+--+--------+------------------+------+--+--+----+----+-+-----+--+--+
Upper  Software UXN          Phys  Block PA [47:21]      Not Acc Shr  Data  Block Attr Vld
Attrs  Use                   Attrs (2MB aligned)         Glb Flag ability Perm Index
```

### Field Descriptions

| Bits    | Field    | Description                                       | SLOPIX Values     |
|---------|----------|---------------------------------------------------|-------------------|
| [0]     | Valid    | Descriptor valid (must be 1)                      | 1                 |
| [1]     | Type     | 0 = Block descriptor (not table/page)             | 0                 |
| [4:2]   | AttrIndx | Index into MAIR_EL1 for memory attributes         | 0 (Device) or 2 (Normal) |
| [5]     | NS       | Non-secure bit                                    | 0                 |
| [7:6]   | AP[2:1]  | Data access permissions                           | 00 (kernel RW)    |
| [9:8]   | SH       | Shareability field                                | 00 (Non-shareable) |
| [10]    | AF       | Access flag (must be 1 to avoid access faults)    | 1                 |
| [11]    | nG       | Not global (0 = global, applies to all ASIDs)     | 0                 |
| [20:12] | RES0     | Reserved, must be 0                               | 0                 |
| [47:21] | OA       | Output address - physical address of 2MB block (2MB aligned) | Varies |
| [51:48] | RES0     | Reserved, must be 0                               | 0                 |
| [52]    | Contiguous | Hint that entry is part of contiguous set       | 0                 |
| [53]    | PXN      | Privileged execute-never                          | 0                 |
| [54]    | UXN/XN   | User/Execute-never bit                            | 0                 |
| [63:55] | Ignored  | Ignored by hardware, available for software use   | 0                 |

### Access Permissions (AP[2:1])

| AP[2:1] | EL1 Access | EL0 Access | Description            |
|---------|------------|------------|------------------------|
| 00      | RW         | None       | Kernel read-write only |
| 01      | RW         | RW         | User and kernel RW     |
| 10      | RO         | None       | Kernel read-only       |
| 11      | RO         | RO         | User and kernel RO     |

### Shareability (SH)

| SH[1:0] | Meaning          |
|---------|------------------|
| 00      | Non-shareable    |
| 01      | Reserved         |
| 10      | Outer shareable  |
| 11      | Inner shareable  |

### SLOPIX Example

From `mmu.c`, typical L2 entry for normal memory at 0x40000000: `0x40000409`

```
Binary: 0000 0000 0000 0000 0100 0000 0000 0000 0000 0100 0000 1001
Hex:                         4    0    0    0    0    4    0    9

Bit [0]:      1  → Valid
Bit [1]:      0  → Block descriptor
Bits [4:2]:   010 → AttrIndx = 2 (MT_NORMAL in MAIR_EL1)
Bit [5]:      0  → Secure
Bits [7:6]:   00 → AP = 00 (kernel RW, user no access)
Bits [9:8]:   00 → SH = 00 (non-shareable)
Bit [10]:     1  → AF = 1 (access flag set)
Bit [11]:     0  → Global (nG = 0)
Bits [47:21]: 0x40000 → Physical address 0x40000000 (2MB aligned)
```

This maps virtual address 0x40000000-0x401FFFFF to physical address 0x40000000-0x401FFFFF with normal memory attributes.

---

## Translation Walk Example

Let's trace how the MMU translates virtual address **0x40000074** (kernel code):

### Step 1: Extract Level 1 Index

```
VA: 0x40000074 = 0b 0 10000000 000000000 000000000 000001110100

Bits [38:30] = 0b 000000001 = 1

L1 index = 1
```

Look up `l1_table[1]`.

### Step 2: Read L1[1] Entry

From SLOPIX page tables:
```
l1_table[1] = 0x40009003
```

Decode:
- Bit [0] = 1 → Valid
- Bit [1] = 1 → Table descriptor
- Bits [47:12] = 0x40009 → Next-level table at PA 0x40009000

This is the `l2_table_kernel` table.

### Step 3: Extract Level 2 Index

```
Bits [29:21] = 0b 000000000 = 0

L2 index = 0
```

Look up `l2_table_kernel[0]` at physical address 0x40009000.

### Step 4: Read L2[0] Entry

From SLOPIX page tables:
```
l2_table_kernel[0] = 0x40000409
```

Decode:
- Bit [0] = 1 → Valid
- Bit [1] = 0 → Block descriptor
- Bits [47:21] = 0x40000 → Physical address 0x40000000

This is a 2MB block covering PA 0x40000000-0x401FFFFF.

### Step 5: Add Block Offset

```
Block base PA:    0x40000000
VA bits [20:0]:   0x00074
Final PA:         0x40000074
```

**Result**: Virtual address 0x40000074 translates to physical address **0x40000074**.

---

## Common Mistakes and Gotchas

### 1. Wrong Number of Table Levels

**Mistake**: Using 3 or 4 levels when only 2 are needed.

**SLOPIX Bug Example**: Initial implementation incorrectly used:
```
TTBR0 → L0[0] → L1[0] → L2_low     (3 levels - WRONG!)
         L0[1] = 0 (not set!)
```

When translating VA 0x40000000:
- Extract L0 index from VA[38:30] = 1
- Read L0[1] = 0 (invalid!)
- **Translation fault at level 1**

**Correct Structure**:
```
TTBR0 → L1[0] → L2_low      (2 levels - CORRECT)
        L1[1] → L2_kernel
```

Level 0 is **skipped** for 39-bit VA with 4KB granule.

### 2. Missing Access Flag (AF)

**Symptom**: Access flag fault on first access to a page.

**Fix**: Always set bit 10 (AF) = 1 in block/page descriptors:
```c
entry = phys_addr | ... | PTE_AF | PTE_VALID;
```

### 3. Incorrect Block Address Alignment

**Symptom**: Translation fault or incorrect physical address.

**Rule**:
- Level 1 block addresses must be 1GB-aligned (bits [29:0] = 0)
- Level 2 block addresses must be 2MB-aligned (bits [20:0] = 0)

**Check**: `phys_addr & 0x1FFFFF` must be 0 for 2MB blocks.

### 4. Not Mapping Exception Vectors

**Symptom**: System hangs or loops when an exception occurs after MMU enable.

**Fix**: Ensure VBAR_EL1 address is mapped. SLOPIX has exception vectors at 0x40003000, which must be covered by page tables.

### 5. Forgetting Device Memory for MMIO

**Symptom**: Hardware registers don't respond, unexpected behavior with peripherals.

**Fix**: Use Device memory attributes (AttrIndx = 0, MT_DEVICE_nGnRnE) for MMIO regions:
```c
if (phys_addr >= 0x08000000 && phys_addr < 0x10000000) {
    attr = MT_DEVICE_nGnRnE;  // For GIC, UART
}
```

### 6. Missing Memory Barriers

**Symptom**: MMU reads stale page table entries, translation faults.

**Fix**: Use DSB (Data Synchronization Barrier) after writing page tables:
```assembly
    dsb sy      // Ensure all page table writes complete
    isb         // Instruction synchronization barrier
```

---

## References

- **ARM Architecture Reference Manual for ARMv8-A**: https://developer.arm.com/documentation/ddi0487/
  - Section D4.2: The VMSAv8-64 address translation system
  - Section D4.2.7: Selection between TTBR0 and TTBR1
  - Section D4.3: VMSAv8-64 translation table format descriptors
  - Section D4.3.1: Translation table descriptor formats
  - Section D4.3.3: Memory attribute fields

- **ARM Cortex-A Series Programmer's Guide for ARMv8-A**: https://developer.arm.com/documentation/den0024/

- **SLOPIX Implementation**:
  - `mmu.c`: Page table setup
  - `memory.h`: PTE bit definitions
  - `boot.S`: TCR_EL1 and MMU configuration
