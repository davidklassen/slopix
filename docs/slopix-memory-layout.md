# SLOPIX Memory Layout

**Platform**: QEMU virt machine (aarch64)
**MMU Configuration**: Higher-half kernel, T0SZ=25, T1SZ=25 (39-bit VA), 4KB granule
**Status**: M5 Complete - MMU enabled, kernel executing from higher-half (TTBR1)

---

## Physical Memory Map

SLOPIX runs on the QEMU `virt` machine, which provides the following physical memory layout:

### Device Memory (MMIO)

| Device           | Base Address | Size     | Description                    |
|------------------|--------------|----------|--------------------------------|
| Flash            | 0x00000000   | 128 MB   | Boot firmware region           |
| **GIC Distributor** | **0x08000000** | **64 KB**    | **GICv2 interrupt controller (GICD)** |
| GIC CPU Interface | 0x08010000   | 64 KB    | GICv2 CPU interface (GICC)     |
| **UART0 (PL011)** | **0x09000000** | **4 KB**     | **Primary serial console**         |
| RTC (PL031)      | 0x09010000   | 4 KB     | Real-time clock                |
| GPIO (PL061)     | 0x09030000   | 4 KB     | GPIO controller                |

**Bold entries** are actively used by SLOPIX.

### RAM

| Region | Base Address   | Size      | Description              |
|--------|----------------|-----------|--------------------------|
| **RAM**    | **0x40000000** | **128 MB** | **Main memory (default)** |

**Notes**:
- RAM size is configurable with QEMU's `-m` flag (e.g., `-m 256M`)
- Kernel is loaded at 0x40000000
- RAM extends from 0x40000000 to 0x48000000 (128MB default)

**Reference**: See `docs/qemu-virt-platform.md` for complete platform specification.

---

## Virtual Address Space Layout

SLOPIX uses a **higher-half kernel** configuration with dual address spaces:
- **TTBR0** (lower-half): 0x0000_0000_0000 to 0x0000_7FFF_FFFF - Identity mapping for boot and devices
- **TTBR1** (higher-half): 0xFFFF_FF80_0000_0000 to 0xFFFF_FFFF_FFFF_FFFF - Kernel execution space

Both address spaces map to the same physical memory. The kernel boots using TTBR0 (physical addressing at 0x40000000) and transitions to TTBR1 (virtual addressing at 0xFFFF_FF80_4000_0000) after MMU enable.

### Address Space Overview (39-bit VA)

```
0x0000_0000_0000 ┌─────────────────────────────────┐
                 │                                 │
                 │  First 1GB                      │
                 │  (Devices, Stack, Low Memory)   │
                 │                                 │
0x0000_3FFF_FFFF ├─────────────────────────────────┤
0x0000_4000_0000 │                                 │
                 │  Second 1GB                     │
                 │  (Kernel Code, Data, Heap)      │
                 │                                 │
0x0000_7FFF_FFFF ├─────────────────────────────────┤
0x0000_8000_0000 │                                 │
                 │  Unmapped                       │
                 │                                 │
0x0000_FFFF_FFFF └─────────────────────────────────┘ ← TTBR0 39-bit limit

                 ...

0xFFFF_FF80_0000_0000 ┌─────────────────────────────────┐
                      │  TTBR1 Address Space            │
                      │  (Higher-half Kernel - ACTIVE)  │
                      │                                 │
                      │  First 1GB: Devices, Low Memory │
                      │  (0xFFFF_FF80_0000_0000+)       │
0xFFFF_FF80_3FFF_FFFF ├─────────────────────────────────┤
0xFFFF_FF80_4000_0000 │  Second 1GB: Kernel Code/Data  │
                      │  ** Kernel executes here **     │
                      │  (Maps to PA 0x40000000+)       │
0xFFFF_FF80_7FFF_FFFF ├─────────────────────────────────┤
                      │  Unmapped                       │
0xFFFF_FFFF_FFFF_FFFF └─────────────────────────────────┘
```

### First 1GB Region (0x00000000 - 0x3FFFFFFF)

**Coverage**: Devices, exception vectors, boot code, stack

| Address Range         | Size   | Contents                           | Memory Type |
|-----------------------|--------|------------------------------------|-------------|
| 0x00000000-0x07FFFFFF | 128 MB | Flash / Low memory                 | Normal      |
| 0x08000000-0x08010000 | 64 KB  | **GIC Distributor (active)**       | **Device**  |
| 0x08010000-0x08020000 | 64 KB  | GIC CPU Interface                  | Device      |
| 0x09000000-0x09001000 | 4 KB   | **UART0 - Serial console (active)** | **Device**  |
| 0x09010000-0x10000000 | ~112 MB | Other devices                      | Device      |
| 0x10000000-0x3FFFFFFF | ~768 MB | Unmapped (no physical RAM here)    | Normal      |

**Key Locations**:
- Boot stack: Set by linker script, typically low addresses before kernel

(Note: Exception vectors are at 0x40003000 in the second 1GB - see below)

### Second 1GB Region (0x40000000 - 0x7FFFFFFF)

**Coverage**: Kernel code, data, heap, physical memory manager

| Address Range         | Size   | Contents                           |
|-----------------------|--------|--------------------------------------|
| 0x40000000-0x40003000 | 12 KB  | Kernel code (.text section)         |
| 0x40003000-0x40004000 | 4 KB   | Exception vector table              |
| 0x40004000-0x40006000 | 8 KB   | Kernel data (.data, .rodata)        |
| 0x40006000-0x40007000 | 4 KB   | PMM bitmap (tracks 128MB / 32768 pages) |
| 0x40007000-0x40008000 | 4 KB   | **L1 page table** (512 entries)     |
| 0x40008000-0x40009000 | 4 KB   | **L2_low page table** (512 entries) |
| 0x40009000-0x4000A000 | 4 KB   | **L2_kernel page table** (512 entries) |
| 0x4000A000-0x48000000 | ~127 MB | Free memory (managed by PMM)       |
| 0x48000000-0x7FFFFFFF | ~896 MB | Unmapped (no physical RAM)         |

**Key Locations**:
- Kernel entry: 0x40000000 (`_start` in `boot.S`)
- Exception vectors: 0x40003000 (pointed to by VBAR_EL1)
- BSS section: Immediately after .data
- Page tables: First pages allocated by PMM
- Heap: Remaining RAM managed by page allocator

---

## Page Table Structure

SLOPIX uses a 2-level page table hierarchy for both TTBR0 and TTBR1, as determined by T0SZ=25 and T1SZ=25 (39-bit VA) with 4KB granule.

### Overview

```
TTBR0_EL1 ──→ L1 Table @ 0x40007000 (lower-half, identity mapping)
              ├── L1[0] ──→ L2_low @ 0x40008000 (maps VA 0x00000000-0x3FFFFFFF → PA 0x00000000-0x3FFFFFFF)
              └── L1[1] ──→ L2_kernel @ 0x40009000 (maps VA 0x40000000-0x7FFFFFFF → PA 0x40000000-0x7FFFFFFF)

TTBR1_EL1 ──→ L1 Table @ 0x4000A000 (higher-half, kernel virtual addresses)
              ├── L1[0] ──→ L2_low @ 0x4000B000 (maps VA 0xFFFF_FF80_0000_0000-0xFFFF_FF80_3FFF_FFFF → PA 0x00000000-0x3FFFFFFF)
              └── L1[1] ──→ L2_kernel @ 0x4000C000 (maps VA 0xFFFF_FF80_4000_0000-0xFFFF_FF80_7FFF_FFFF → PA 0x40000000-0x7FFFFFFF)

Note: Both TTBR0 and TTBR1 map to identical physical addresses but use different virtual addresses.
      Kernel uses TTBR0 during boot, then transitions to TTBR1 after MMU enable.
```

### L1 Table (Level 1)

**Physical Address**: 0x40007000 (pointed to by TTBR0_EL1)
**Number of Entries**: 512 (one 4KB page)
**Coverage Per Entry**: 1 GB

| L1 Index | VA Range                  | Points To            | Coverage        |
|----------|---------------------------|----------------------|-----------------|
| 0        | 0x00000000-0x3FFFFFFF     | L2_low @ 0x40008000  | First 1GB       |
| 1        | 0x40000000-0x7FFFFFFF     | L2_kernel @ 0x40009000 | Second 1GB    |
| 2-511    | 0x80000000-...            | Not configured (0)   | Unmapped        |

**Entry Format**: Table descriptors (see `docs/arm64-page-tables.md`)

Example from SLOPIX:
```
L1[0] = 0x40008003
  Bit [0]: 1 (Valid)
  Bit [1]: 1 (Table descriptor)
  Bits [47:12]: 0x40008 → Points to L2_low at PA 0x40008000
```

### L2_low Table (Level 2, First 1GB)

**Physical Address**: 0x40008000
**Number of Entries**: 512 (one 4KB page)
**Coverage Per Entry**: 2 MB (block descriptor)

Maps first 1GB (0x00000000-0x3FFFFFFF) with 2MB blocks:

| L2 Index | VA Range              | PA Range              | Memory Type | Purpose         |
|----------|-----------------------|-----------------------|-------------|-----------------|
| 0-3      | 0x00000000-0x007FFFFF | 0x00000000-0x007FFFFF | Normal      | Low memory      |
| 4        | 0x00800000-0x009FFFFF | 0x00800000-0x009FFFFF | Device      | GIC             |
| 5-7      | 0x00A00000-0x00FFFFFF | 0x00A00000-0x00FFFFFF | Device      | UART, other devices |
| 8-511    | 0x01000000-0x3FFFFFFF | 0x01000000-0x3FFFFFFF | Normal      | Unmapped (no RAM) |

**Entry Format**: Block descriptors for 2MB blocks

Example (normal memory):
```
L2_low[0] = 0x00000409
  Bit [0]: 1 (Valid)
  Bit [1]: 0 (Block descriptor)
  Bits [4:2]: 010 (AttrIndx = 2, MT_NORMAL)
  Bit [10]: 1 (AF = Access flag)
  Bits [47:21]: 0x00000 → PA 0x00000000
```

Example (device memory):
```
L2_low[4] = 0x00800001
  Bit [0]: 1 (Valid)
  Bit [1]: 0 (Block descriptor)
  Bits [4:2]: 000 (AttrIndx = 0, MT_DEVICE_nGnRnE)
  Bit [10]: 1 (AF = Access flag)
  Bits [47:21]: 0x00800 → PA 0x00800000 (covers GIC at 0x08000000)
```

### L2_kernel Table (Level 2, Second 1GB)

**Physical Address**: 0x40009000
**Number of Entries**: 512 (one 4KB page)
**Coverage Per Entry**: 2 MB (block descriptor)

Maps second 1GB (0x40000000-0x7FFFFFFF) with 2MB blocks:

| L2 Index | VA Range              | PA Range              | Memory Type | Purpose              |
|----------|-----------------------|-----------------------|-------------|----------------------|
| 0        | 0x40000000-0x401FFFFF | 0x40000000-0x401FFFFF | Normal      | Kernel code/data     |
| 1-3      | 0x40200000-0x407FFFFF | 0x40200000-0x407FFFFF | Normal      | Page tables, PMM     |
| 4-63     | 0x40800000-0x47FFFFFF | 0x40800000-0x47FFFFFF | Normal      | Free memory (128 MB) |
| 64-511   | 0x48000000-0x7FFFFFFF | 0x48000000-0x7FFFFFFF | Normal      | Beyond physical RAM  |

**Entry Format**: Block descriptors for 2MB blocks

Example:
```
L2_kernel[0] = 0x40000409
  Bit [0]: 1 (Valid)
  Bit [1]: 0 (Block descriptor)
  Bits [4:2]: 010 (AttrIndx = 2, MT_NORMAL)
  Bit [10]: 1 (AF = Access flag)
  Bits [47:21]: 0x40000 → PA 0x40000000
```

### TTBR1 Page Tables (Higher-Half Kernel)

**Base Virtual Address**: 0xFFFF_FF80_0000_0000 (determined by T1SZ=25)
**Purpose**: Kernel execution space after MMU transition
**Status**: Active - kernel executes from TTBR1 addresses after boot

TTBR1 uses an identical structure to TTBR0 but maps higher-half virtual addresses to the same physical memory. This dual-mapping approach allows the kernel to transition from physical addressing during boot to virtual addressing after MMU enable.

#### TTBR1 L1 Table

**Physical Address**: Allocated dynamically by PMM (typically 0x4000A000)
**Number of Entries**: 512 (one 4KB page)
**Coverage Per Entry**: 1 GB

| L1 Index | VA Range                                  | Points To         | Physical Mapping       |
|----------|-------------------------------------------|-------------------|------------------------|
| 0        | 0xFFFF_FF80_0000_0000 - 0xFFFF_FF80_3FFF_FFFF | TTBR1 L2_low     | PA 0x00000000-0x3FFFFFFF |
| 1        | 0xFFFF_FF80_4000_0000 - 0xFFFF_FF80_7FFF_FFFF | TTBR1 L2_kernel  | PA 0x40000000-0x7FFFFFFF |
| 2-511    | Not configured                            | Unmapped (0)      | Unmapped               |

#### TTBR1 L2_low Table (Higher-Half First 1GB)

Maps higher-half VAs to devices and low memory with identical attributes as TTBR0 L2_low:

| L2 Index | VA Range (Higher-Half)                    | PA Range              | Memory Type | Purpose         |
|----------|-------------------------------------------|-----------------------|-------------|-----------------|
| 0-3      | 0xFFFF_FF80_0000_0000 - 0xFFFF_FF80_007F_FFFF | 0x00000000-0x007FFFFF | Normal      | Low memory      |
| 4        | 0xFFFF_FF80_0080_0000 - 0xFFFF_FF80_009F_FFFF | 0x00800000-0x009FFFFF | Device      | GIC             |
| 5-7      | 0xFFFF_FF80_00A0_0000 - 0xFFFF_FF80_00FF_FFFF | 0x00A00000-0x00FFFFFF | Device      | UART, devices   |
| 8-511    | 0xFFFF_FF80_0100_0000 - 0xFFFF_FF80_3FFF_FFFF | 0x01000000-0x3FFFFFFF | Normal      | Unmapped        |

#### TTBR1 L2_kernel Table (Higher-Half Second 1GB)

Maps higher-half kernel VAs to physical kernel memory - **this is where the kernel executes**:

| L2 Index | VA Range (Higher-Half)                    | PA Range              | Memory Type | Purpose              |
|----------|-------------------------------------------|-----------------------|-------------|----------------------|
| 0        | 0xFFFF_FF80_4000_0000 - 0xFFFF_FF80_401F_FFFF | 0x40000000-0x401FFFFF | Normal      | **Kernel code/data** |
| 1-3      | 0xFFFF_FF80_4020_0000 - 0xFFFF_FF80_407F_FFFF | 0x40200000-0x407FFFFF | Normal      | Page tables, PMM     |
| 4-63     | 0xFFFF_FF80_4080_0000 - 0xFFFF_FF80_47FF_FFFF | 0x40800000-0x47FFFFFF | Normal      | Free memory (128 MB) |
| 64-511   | 0xFFFF_FF80_4800_0000 - 0xFFFF_FF80_7FFF_FFFF | 0x48000000-0x7FFFFFFF | Normal      | Beyond physical RAM  |

#### Transition Process

The kernel's transition from TTBR0 to TTBR1 happens during boot:

1. **Boot Stage**: CPU starts execution at physical address 0x40000000 using TTBR0 (identity mapping)
2. **MMU Initialization**: Both TTBR0 and TTBR1 page tables are configured with identical physical mappings
3. **MMU Enable**: MMU is enabled with both translation tables active
4. **Transition**: Code jumps to `transition_to_higher_half` function which:
   - Computes higher-half address: `VA = PA + KERNEL_VIRT_OFFSET`
   - Updates PC to higher-half address (0xFFFF_FF80_4000_xxxx)
   - All subsequent execution uses TTBR1 virtual addresses
5. **Post-Transition**: Kernel runs entirely from higher-half, TTBR0 remains for potential future use (user space)

**Key Insight**: After transition, when kernel code executes at VA 0xFFFF_FF80_4000_0000+, the MMU uses TTBR1 to translate these addresses back to PA 0x40000000+, resulting in the same physical instruction fetches as before - but now with proper virtual addressing.

---

## Memory Attributes by Region

SLOPIX uses two memory attribute types defined in MAIR_EL1:

### MAIR_EL1 Configuration

```
MAIR_EL1 = 0x00FF4400

Attr0 (bits 7:0):   0x00 = Device-nGnRnE (non-Gathering, non-Reordering, no Early Write Ack)
Attr1 (bits 15:8):  0x44 = Normal memory, non-cacheable
Attr2 (bits 23:16): 0xFF = Normal memory, write-back cacheable, RW-allocate
```

### Memory Type Usage

| Address Range         | MAIR Index | Memory Type         | Purpose                |
|-----------------------|------------|---------------------|------------------------|
| 0x00000000-0x07FFFFFF | 2 (Normal) | Normal, write-back  | Low memory, flash      |
| 0x08000000-0x0FFFFFFF | 0 (Device) | Device-nGnRnE       | **GIC, UART, devices** |
| 0x10000000-0x3FFFFFFF | 2 (Normal) | Normal, write-back  | Unmapped region        |
| 0x40000000-0x7FFFFFFF | 2 (Normal) | Normal, write-back  | **Kernel, RAM**        |

**Why Device Memory for MMIO?**
- **Non-Gathering**: Each access appears exactly once on bus
- **Non-Reordering**: Accesses appear in program order
- **No Early Write Acknowledgement**: Writes complete before returning

This ensures hardware registers behave correctly (no read/write combining, no reordering).

**Why Normal Memory for RAM?**
- **Write-back caching**: Better performance
- **Read/Write allocate**: Cache lines filled on access
- Allows speculative reads, out-of-order execution

---

## Boot Requirements: Critical Mappings

Before enabling the MMU, the following regions **MUST** be mapped:

### 1. Kernel Code Region
**Address**: 0x40000000+
**Why**: After MMU enable, CPU fetches next instruction using virtual addressing. If unmapped → **instruction abort fault**.

### 2. Exception Vector Table
**Address**: 0x40003000 (VBAR_EL1)
**Why**: If any fault occurs after MMU enable, CPU jumps to exception vectors. If unmapped → **exception loop (double fault)**.

### 3. Stack Region
**Address**: Varies (set by linker script, typically low memory or kernel region)
**Why**: Function calls, local variables require stack access. If unmapped → **data abort fault**.

### 4. UART Region
**Address**: 0x09000000
**Why**: Printf uses UART for output. If unmapped → **data abort fault on printf**.
**Memory Type**: **Must be Device memory** (not Normal) for hardware access.

### 5. GIC Region
**Address**: 0x08000000 (GICD), 0x08010000 (GICC)
**Why**: Interrupt acknowledgement and EOI require GIC access. If unmapped → **data abort in IRQ handler**.
**Memory Type**: **Must be Device memory** (not Normal) for hardware access.

### 6. Physical Memory Manager Structures
**Address**: 0x40006000+ (bitmap, page tables)
**Why**: Page allocation/free after MMU enable needs PMM bitmap access. If unmapped → **data abort**.

---

## Translation Examples

### Example 1: TTBR0 Translation - VA 0x09000004 (UART via Identity Mapping)

Let's trace how the MMU translates a UART register access using TTBR0:

```
VA: 0x09000004

Step 1: Determine which TTBR to use
  VA[63] = 0 → Use TTBR0 (lower-half address space)

Step 2: Extract L1 index
  VA[38:30] = 0b000000000 = 0
  TTBR0 L1[0] → L2_low @ 0x40008000

Step 3: Extract L2 index
  VA[29:21] = (0x09000004 >> 21) & 0x1FF = 72

  Verification: Each L2 entry covers 2MB = 0x200000
  Entry 72 covers: 72 × 0x200000 = 0x09000000-0x091FFFFF ✓

Step 4: Read L2_low[72]
  L2_low[72] points to block at PA 0x09000000

Step 5: Add block offset
  Block base: 0x09000000
  VA[20:0]: 0x00004
  Final PA: 0x09000000 + 0x004 = 0x09000004 (UART data register)
```

### Example 2: TTBR1 Translation - VA 0xFFFF_FF80_4000_0100 (Kernel Code via Higher-Half)

Let's trace how the MMU translates a kernel code access using TTBR1:

```
VA: 0xFFFF_FF80_4000_0100 (higher-half kernel address)

Step 1: Determine which TTBR to use
  VA[63] = 1 → Use TTBR1 (higher-half address space)

  Note: For T1SZ=25, TTBR1 handles VAs >= 0xFFFF_FF80_0000_0000
        This VA falls in TTBR1 range ✓

Step 2: Extract L1 index from lower 39 bits
  VA[38:30] = 0b000000001 = 1
  TTBR1 L1[1] → TTBR1 L2_kernel @ 0x4000C000

Step 3: Extract L2 index
  VA[29:21] = (0x40000100 >> 21) & 0x1FF = 0

  Verification: Entry 0 covers 0xFFFF_FF80_4000_0000 - 0xFFFF_FF80_401F_FFFF ✓

Step 4: Read TTBR1 L2_kernel[0]
  TTBR1 L2_kernel[0] points to block at PA 0x40000000

Step 5: Add block offset
  Block base: 0x40000000
  VA[20:0]: 0x00100
  Final PA: 0x40000000 + 0x100 = 0x40000100 (kernel code)

Key Insight: The higher-half VA 0xFFFF_FF80_4000_0100 translates to the same
            physical address (PA 0x40000100) as TTBR0's VA 0x40000100 would.
            This dual-mapping allows the kernel to be accessible via both
            address spaces during the MMU transition.
```

---

## Implementation Status

### ✅ Higher-Half Kernel (M5 Complete)

**Status**: Fully implemented and operational

The kernel now executes from higher-half virtual addresses mapped via TTBR1:
- **Virtual Base**: 0xFFFF_FF80_4000_0000
- **Physical Base**: 0x40000000
- **Dual Address Spaces**: Both TTBR0 (identity) and TTBR1 (higher-half) configured
- **Transition**: Successful PC update from physical to virtual addressing after MMU enable
- **Benefits Achieved**:
  - Clean separation of kernel (higher-half) and potential user space (lower-half)
  - Standard convention for 64-bit kernels maintained
  - TTBR0 reserved for future per-process user address spaces

### 🔜 Future Enhancements (M6 and Beyond)

**Per-Process Page Tables** (required for userspace):
- Each user process will have its own TTBR0 page table
- Context switching will save/restore TTBR0_EL1 per process
- TTBR1 remains fixed (kernel always accessible from all processes)
- User address space: 0x0000_0000_0000_0000 to 0x0000_FFFF_FFFF_FFFF (full lower-half)

**EL0 User Programs**:
- Processes will execute at EL0 with restricted permissions
- System calls will transition EL0 → EL1 for kernel services
- Page table entries already configured with PTE_KERNEL for security
- Memory isolation between user processes via separate TTBR0 tables

**Current Limitation**:
- Single TTBR0 page table shared globally (no per-process isolation yet)
- All code executes at EL1 (no EL0 user mode yet)
- Future M6 milestone will implement process address space switching

---

## References

- **ARM64 Page Tables**: `docs/arm64-page-tables.md` - Detailed descriptor formats and translation
- **Platform Specification**: `docs/qemu-virt-platform.md` - Complete QEMU virt memory map
- **ARM Registers**: `docs/arm64-registers.md` - MAIR_EL1, TCR_EL1, TTBR0_EL1 specifications

- **SLOPIX Implementation**:
  - `mmu.c`: Page table setup code
  - `memory.h`: Memory layout constants and PTE definitions
  - `boot.S`: Early MMU register configuration
  - `linker.ld`: Kernel memory layout
