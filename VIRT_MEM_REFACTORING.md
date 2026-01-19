# Virtual Memory Refactoring Design

This document describes the ideal memory layout, boot sequence, and MMU architecture for Slopix, incorporating lessons learned from the TTBR0/identity mapping issue.

## Table of Contents

1. [Memory Layout](#1-memory-layout)
2. [Boot Sequence](#2-boot-sequence)
3. [Page Table Architecture](#3-page-table-architecture)
4. [Design Principles](#4-design-principles)
5. [Testing Strategy](#5-testing-strategy)
6. [Prototype Kernel Specification](#6-prototype-kernel-specification)
7. [Refactoring Roadmap](#7-refactoring-roadmap)

---

## 1. Memory Layout

### 1.1 Physical Memory Map (QEMU virt)

```
0x0000_0000 - 0x07FF_FFFF   Flash (unused)
0x0800_0000 - 0x0800_FFFF   GIC Distributor (GICD)
0x0801_0000 - 0x0801_FFFF   GIC CPU Interface (GICC)
0x0900_0000 - 0x0900_FFFF   PL011 UART0
0x0a00_0000 - 0x0a00_3FFF   Virtio MMIO
0x4000_0000 - 0x47FF_FFFF   RAM (128MB)
```

### 1.2 Virtual Address Space Design

AArch64 with 48-bit virtual addresses uses two page table base registers:
- **TTBR0_EL1**: Translates addresses where bit[63:48] = 0x0000 (user space)
- **TTBR1_EL1**: Translates addresses where bit[63:48] = 0xFFFF (kernel space)

Hardware automatically selects TTBR based on the top bits of the virtual address.

```
┌─────────────────────────────────────────────────────────────────┐
│                    64-bit Virtual Address Space                  │
├─────────────────────────────────────────────────────────────────┤
│ 0x0000_0000_0000_0000 ─┬─ User Space (TTBR0, per-process)       │
│                        │  Code:  0x0000_0000_0000_0000          │
│                        │  Heap:  grows up                        │
│                        │  Stack: 0x0000_0000_8000_0000 (grows ↓)│
│ 0x0000_FFFF_FFFF_FFFF ─┴─────────────────────────────────────── │
│                        │                                         │
│        (hole)          │  Non-canonical addresses (fault)        │
│                        │                                         │
│ 0xFFFF_0000_0000_0000 ─┬─ Kernel Space (TTBR1, shared)          │
│                        │  Devices: 0xFFFF_0000_0800_0000 (GIC)  │
│                        │           0xFFFF_0000_0900_0000 (UART) │
│                        │  Kernel:  0xFFFF_0000_4000_0000 (code) │
│ 0xFFFF_FFFF_FFFF_FFFF ─┴─────────────────────────────────────── │
└─────────────────────────────────────────────────────────────────┘
```

### 1.3 Why Higher-Half Kernel is Required

**The Problem:**

When a user process runs, TTBR0 must point to that process's page table. If the kernel runs at low addresses (0x4000_0000), it relies on TTBR0's identity mapping. Switching TTBR0 to a user page table removes the kernel's code mapping, causing an immediate instruction fetch fault.

**The Solution:**

Link and run the kernel at high virtual addresses (0xFFFF_0000_4000_0000). The kernel uses TTBR1 exclusively. TTBR0 can freely switch between processes without affecting kernel execution.

```
Before (broken):                    After (correct):
┌──────────────┐                   ┌──────────────┐
│   TTBR0      │─→ Identity map    │   TTBR0      │─→ User page table
│   (switched) │   (has kernel!)   │   (switched) │   (user only)
├──────────────┤                   ├──────────────┤
│   TTBR1      │─→ (not used)      │   TTBR1      │─→ Kernel mapping
│              │                   │   (stable)   │   (always valid)
└──────────────┘                   └──────────────┘
Kernel at 0x4000_0000 → FAULT!     Kernel at 0xFFFF... → OK
```

### 1.4 Address Translation Scheme

All kernel virtual addresses follow a simple offset scheme:

```c
#define KERNEL_BASE  0xFFFF_0000_0000_0000

#define PA_TO_VA(pa) ((void *)((pa) + KERNEL_BASE))
#define VA_TO_PA(va) ((paddr_t)(va) - KERNEL_BASE)
```

Physical 0x4000_0000 → Virtual 0xFFFF_0000_4000_0000
Physical 0x0900_0000 → Virtual 0xFFFF_0000_0900_0000

---

## 2. Boot Sequence

### 2.1 Overview

With **static page tables** (pre-populated at compile time), the boot sequence simplifies dramatically:

| Stage | Running At | MMU State | Tasks |
|-------|------------|-----------|-------|
| 1. Configure MMU | PA 0x4000_0000 | OFF | Load TTBRs, set MAIR/TCR |
| 2. Enable MMU | PA 0x4000_0000 (identity mapped) | ON | Set SCTLR_EL1.M |
| 3. Prepare for C | Low VA 0x4000_xxxx (identity) | ON | VBAR, SP, FP/SIMD, BSS |
| 4. Jump to Kernel | High VA 0xFFFF_0000_4001_xxxx | ON | br to kernel_main |

**Key simplifications:**
- No early stack needed (no function calls before MMU)
- No runtime table construction (tables are static data)
- No VA→PA conversions for tables (tables in BOOT region have PA symbols)

**Important distinction:** After MMU enable, we're still running at **low VA** (0x4000_xxxx) via identity mapping. We only switch to **high VA** when we `br` to kernel_main. Between MMU enable and that jump, instruction fetch uses TTBR0 (identity), but data accesses to 0xFFFF... use TTBR1.

### 2.2 Stage 1: Configure MMU Registers (Physical Address)

```
Location: boot.S, .text.boot section
Running at: PA 0x4000_0000 (loaded by QEMU)
MMU: OFF

Tasks:
  1. Set MAIR_EL1 (memory attributes)
  2. Set TCR_EL1 (translation control)
  3. Set TTBR0_EL1 = identity_l0 (PA, since tables in BOOT region)
  4. Set TTBR1_EL1 = kernel_l0 (PA)
```

**No stack needed!** With static tables, there are no function calls (`bl`) before MMU enable. Everything is inline MSR instructions.

**No VA→PA conversion for tables!** Tables are in the BOOT region, so their symbols are already physical addresses.

```asm
_start:
    /* Tables are pre-populated - just load their addresses */
    ldr     x0, =0xFF00             /* MAIR: idx0=device, idx1=normal */
    msr     mair_el1, x0

    ldr     x0, =0x1B5103510        /* TCR: T0SZ=16, T1SZ=16, 4KB granule */
    msr     tcr_el1, x0

    ldr     x0, =identity_l0        /* PA (tables in BOOT region) */
    msr     ttbr0_el1, x0

    ldr     x0, =kernel_l0          /* PA */
    msr     ttbr1_el1, x0

    isb
```

### 2.3 Static Page Tables (Compile-Time)

Instead of runtime table construction, define tables as **static data** in assembly:

```
Location: tables.S, .tables section (in BOOT region)
Built at: Compile/link time
Runtime cost: Zero
```

**Why static tables are superior:**
- No runtime code to debug
- Assembler/linker resolves all addresses
- Tables in BOOT region → symbols are physical addresses
- No VA→PA conversion issues

**Two-region linker layout:**

```ld
MEMORY
{
    BOOT (rwx)   : ORIGIN = 0x40000000, LENGTH = 64K    /* PA */
    KERNEL (rwx) : ORIGIN = 0xFFFF000040010000, LENGTH = 128M - 64K  /* VA */
}

SECTIONS
{
    .text.boot : { *(.text.boot) } > BOOT
    .tables : ALIGN(4096) { *(.tables) } > BOOT   /* Tables at PA */
    .text : AT(0x40010000) { *(.text*) } > KERNEL /* Kernel at VA */
    /* ... */
}
```

Tables in BOOT region have PA symbols. Kernel in KERNEL region has VA symbols.

### 2.4 Stage 2: MMU Enable (Inline)

```
Location: boot.S, .text.boot section (inline, no function call)
Running at: PA 0x4000_0000
MMU: turning ON

Tasks:
  1. DSB SY (complete pending memory operations)
  2. TLBI VMALLE1 (invalidate TLB)
  3. DSB SY + ISB (synchronize)
  4. Set SCTLR_EL1.M = 1 (enable MMU)
  5. ISB (synchronize)
```

```asm
    dsb     sy
    tlbi    vmalle1
    dsb     sy
    isb

    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 0)       /* M bit - enable MMU */
    orr     x0, x0, #(1 << 2)       /* C bit - data cache */
    orr     x0, x0, #(1 << 12)      /* I bit - instruction cache */
    msr     sctlr_el1, x0
    isb
```

**The magic moment**: After setting SCTLR_EL1.M, every instruction fetch goes through the MMU. The identity mapping ensures VA 0x4000_xxxx = PA 0x4000_xxxx, so execution continues seamlessly.

**We are now at low VA (0x4000_xxxx)**, not high VA yet. The identity mapping in TTBR0 translates our instruction fetches.

### 2.5 Stage 3: Prepare for C (Still at Low VA)

```
Location: boot.S, .text.boot section
Running at: Low VA 0x4000_xxxx (via identity mapping in TTBR0)
MMU: ON

Tasks:
  1. Set VBAR_EL1 = vectors (high VA symbol, for future exceptions)
  2. Set SP = __stack_top (high VA - data access goes through TTBR1)
  3. Enable FP/SIMD
  4. Clear BSS (high VA - data access goes through TTBR1)
```

**Subtle point**: We're still fetching instructions from low VA (TTBR0 identity mapping), but data accesses to 0xFFFF... addresses go through TTBR1. So we can set SP and clear BSS using high VA symbols while our code runs at low VA.

```asm
    /* VBAR for future exceptions (high VA) */
    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

    /* Stack at high VA (no conversion needed - MMU handles it) */
    ldr     x0, =__stack_top
    mov     sp, x0

    /* Enable FP/SIMD for C code */
    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0
    isb

    /* Clear BSS at high VA */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
1:  cmp     x0, x1
    b.ge    2f
    str     xzr, [x0], #8
    b       1b
2:
```

### 2.6 Stage 4: Jump to High VA

```
Location: boot.S, .text.boot section
Running at: Low VA → High VA transition
MMU: ON
```

```asm
    /* Load high VA of kernel_main */
    ldr     x0, =kernel_main        /* x0 = 0xFFFF_0000_4001_XXXX */
    br      x0                      /* JUMP TO HIGH VA */
```

**This is the moment we switch to high VA.** Before `br`, instruction fetch uses TTBR0 (identity). After `br`, instruction fetch uses TTBR1 (kernel mapping).

From this point on, the kernel runs entirely from TTBR1. The identity mapping in TTBR0 is no longer used for kernel code - TTBR0 will later be used for user processes.

### 2.7 Complete Boot Code

Putting it all together - the entire boot sequence in ~40 lines:

```asm
/* boot.S */
.section .text.boot
.global _start

_start:
    /* Stage 1: Configure MMU registers (no stack needed) */
    ldr     x0, =0xFF00
    msr     mair_el1, x0

    ldr     x0, =0x1B5103510
    msr     tcr_el1, x0

    ldr     x0, =identity_l0        /* PA (BOOT region) */
    msr     ttbr0_el1, x0

    ldr     x0, =kernel_l0          /* PA (BOOT region) */
    msr     ttbr1_el1, x0
    isb

    /* Stage 2: Enable MMU */
    dsb     sy
    tlbi    vmalle1
    dsb     sy
    isb

    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 0)
    orr     x0, x0, #(1 << 2)
    orr     x0, x0, #(1 << 12)
    msr     sctlr_el1, x0
    isb

    /* Stage 3: Prepare for C (still at low VA, data at high VA) */
    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

    ldr     x0, =__stack_top
    mov     sp, x0

    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0
    isb

    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
1:  cmp     x0, x1
    b.ge    2f
    str     xzr, [x0], #8
    b       1b
2:

    /* Stage 4: Jump to high VA */
    ldr     x0, =kernel_main
    br      x0

.global halt
halt:
    wfe
    b       halt
```

---

## 3. Page Table Architecture

### 3.1 Table Hierarchy

AArch64 with 4KB granule and 48-bit VA uses 4-level tables:

```
L0 (512 entries) → L1 (512 entries) → L2 (512 entries) → L3 (512 entries)
   512GB each        1GB each           2MB each           4KB each
```

For our simple kernel, we use:
- L0 → L1 → L2 with 2MB block descriptors (no L3 for kernel)
- L0 → L1 → L2 → L3 with 4KB page descriptors (for user processes)

### 3.2 Static Kernel Tables

The kernel page tables are fixed at boot and never change:

```
Identity Tables (TTBR0, temporary):
  identity_l0[0] → identity_l1
  identity_l1[0] → identity_l2_dev (devices 0x0000_0000 - 0x3FFF_FFFF)
  identity_l1[1] → identity_l2_ram (RAM 0x4000_0000 - 0x7FFF_FFFF)

Kernel Tables (TTBR1, permanent):
  kernel_l0[0] → kernel_l1
  kernel_l1[0] → kernel_l2_dev (devices)
  kernel_l1[1] → kernel_l2_ram (RAM)
```

**Implementation**: Define tables as static data in assembly (tables.S), placed in BOOT region so symbols are physical addresses.

### 3.2.1 Table Memory Layout

8 tables × 4KB = 32KB total:

```
PA 0x400X_X000    identity_l0     (4KB, 1 entry used)
PA 0x400X_X000    identity_l1     (4KB, 2 entries used)
PA 0x400X_X000    identity_l2_dev (4KB, 2 entries used: GIC, UART)
PA 0x400X_X000    identity_l2_ram (4KB, 64 entries used: 128MB)
PA 0x400X_X000    kernel_l0       (4KB, 1 entry used)
PA 0x400X_X000    kernel_l1       (4KB, 2 entries used)
PA 0x400X_X000    kernel_l2_dev   (4KB, 2 entries used)
PA 0x400X_X000    kernel_l2_ram   (4KB, 64 entries used)
```

### 3.2.2 Static Table Implementation (tables.S)

Using assembly macros to avoid repetition:

```asm
/* tables.S - Static page tables */

/* Flags */
.equ PTE_VALID,     (1 << 0)
.equ PTE_TABLE,     (1 << 1)
.equ PTE_AF,        (1 << 10)
.equ PTE_SH_INNER,  (3 << 8)
.equ PTE_SH_OUTER,  (2 << 8)
.equ PTE_ATTR_DEV,  (0 << 2)
.equ PTE_ATTR_NORM, (1 << 2)
.equ PTE_UXN,       (1 << 54)
.equ PTE_PXN,       (1 << 53)

.equ TABLE_FLAGS,   (PTE_VALID | PTE_TABLE)
.equ DEV_FLAGS,     (PTE_VALID | PTE_AF | PTE_SH_OUTER | PTE_ATTR_DEV | PTE_UXN | PTE_PXN)
.equ RAM_FLAGS,     (PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_ATTR_NORM | PTE_UXN)

.equ RAM_BASE,      0x40000000
.equ GICD_BASE,     0x08000000
.equ UART_BASE,     0x09000000

/* Macro: Generate N consecutive 2MB RAM block entries */
.macro ram_blocks base, flags, count
    .set OFFSET, 0
    .rept \count
        .quad (\base + OFFSET) + \flags
        .set OFFSET, OFFSET + 0x200000
    .endr
.endm

.section .tables, "aw"

/* ============ Identity Tables (TTBR0) ============ */

.balign 4096
.global identity_l0
identity_l0:
    .quad identity_l1 + TABLE_FLAGS
    .fill 511, 8, 0

.balign 4096
.global identity_l1
identity_l1:
    .quad identity_l2_dev + TABLE_FLAGS
    .quad identity_l2_ram + TABLE_FLAGS
    .fill 510, 8, 0

.balign 4096
.global identity_l2_dev
identity_l2_dev:
    .fill 64, 8, 0                      /* Entries 0-63: empty */
    .quad GICD_BASE + DEV_FLAGS         /* Entry 64: GIC */
    .fill 7, 8, 0                       /* Entries 65-71: empty */
    .quad UART_BASE + DEV_FLAGS         /* Entry 72: UART */
    .fill 439, 8, 0                     /* Entries 73-511: empty */

.balign 4096
.global identity_l2_ram
identity_l2_ram:
    ram_blocks RAM_BASE, RAM_FLAGS, 64  /* 64 × 2MB = 128MB */
    .fill 448, 8, 0                     /* Entries 64-511: empty */

/* ============ Kernel Tables (TTBR1) ============ */

.balign 4096
.global kernel_l0
kernel_l0:
    .quad kernel_l1 + TABLE_FLAGS
    .fill 511, 8, 0

.balign 4096
.global kernel_l1
kernel_l1:
    .quad kernel_l2_dev + TABLE_FLAGS
    .quad kernel_l2_ram + TABLE_FLAGS
    .fill 510, 8, 0

.balign 4096
.global kernel_l2_dev
kernel_l2_dev:
    .fill 64, 8, 0
    .quad GICD_BASE + DEV_FLAGS
    .fill 7, 8, 0
    .quad UART_BASE + DEV_FLAGS
    .fill 439, 8, 0

.balign 4096
.global kernel_l2_ram
kernel_l2_ram:
    ram_blocks RAM_BASE, RAM_FLAGS, 64
    .fill 448, 8, 0
```

**Key features:**
- `.rept` directive generates repetitive entries
- `ram_blocks` macro generates all 64 RAM entries in one line
- Assembler/linker resolves `identity_l1 + TABLE_FLAGS` to correct PA
- Tables in BOOT region → symbols are physical addresses

### 3.3 Dynamic User Tables

User page tables are allocated at runtime:

```c
pte_t *uvm_create(void);                    /* Allocate L0 */
int uvm_map_page(pte_t *pt, va, pa, w, x);  /* Walk & map 4KB page */
void uvm_free(pte_t *pt);                   /* Free all levels */
```

User tables use 4-level (L0→L1→L2→L3) with 4KB pages for fine-grained mapping.

### 3.4 Page Table Entry Format

```
[63]    Reserved (must be 0 for non-secure)
[62:59] Reserved
[58:55] Reserved for SW use
[54]    UXN - User execute never
[53]    PXN - Privileged execute never
[52]    Contiguous hint
[51:48] Reserved
[47:12] Output address (PA of next table or page)
[11]    nG - Not global
[10]    AF - Access flag (must be 1)
[9:8]   SH - Shareability (00=none, 10=outer, 11=inner)
[7:6]   AP - Access permissions
[5]     NS - Non-secure
[4:2]   AttrIndx - Index into MAIR_EL1
[1]     Type (0=block, 1=table/page)
[0]     Valid
```

### 3.5 Memory Attributes (MAIR_EL1)

```c
#define MAIR_DEVICE  0x00  /* Device-nGnRnE (index 0) */
#define MAIR_NORMAL  0xFF  /* Normal, Write-Back (index 1) */
#define MAIR_VALUE   ((MAIR_NORMAL << 8) | MAIR_DEVICE)
```

Block/page descriptors reference these via AttrIndx[4:2]:
- `AttrIndx = 0` → Device memory (UART, GIC)
- `AttrIndx = 1` → Normal memory (RAM)

---

## 4. Design Principles

### 4.1 Single Source of Truth for Constants

Define all memory-related constants in one header:

```c
/* mem.h - single source of truth */
#define KERNEL_BASE     0xFFFF000000000000UL
#define RAM_BASE        0x40000000UL
#define RAM_SIZE        0x08000000UL  /* 128MB */
#define PAGE_SIZE       4096UL

#define UART0_PA        0x09000000UL
#define GICD_PA         0x08000000UL
#define GICC_PA         0x08010000UL

#define PA_TO_VA(pa)    ((void *)((paddr_t)(pa) + KERNEL_BASE))
#define VA_TO_PA(va)    ((paddr_t)(va) - KERNEL_BASE)
```

Assembly files can include a `.inc` version or use `.equ` directives that mirror these values.

### 4.2 Minimal Assembly

With static tables, assembly is minimal:

```
boot.S     - ~40 lines: configure MMU, enable, prepare for C, jump
tables.S   - ~80 lines: static table data with macros
```

No runtime table construction. No function calls before MMU. No VA→PA conversions for tables.

### 4.3 Clear Separation of Concerns

```
boot.S      - Boot sequence (configure, enable, jump)
tables.S    - Static page table data
mmu.c       - User page table management only (uvm_* functions)
```

The kernel table setup is entirely in tables.S (data) and boot.S (load TTBRs). mmu.c only handles dynamic user page tables.

### 4.4 Testable Invariants

Design so that key properties can be verified:
1. Kernel code runs from TTBR1 range
2. TTBR0 and TTBR1 point to different tables
3. Switching TTBR0 doesn't break kernel execution
4. PA_TO_VA and VA_TO_PA are inverses

---

## 5. Testing Strategy

### 5.1 Architectural Invariants

These tests verify the memory architecture is correct, not just that it "works":

#### Test 1: Kernel Runs from TTBR1 Range

```c
TEST(kernel_runs_from_high_address) {
    unsigned long pc;
    __asm__ volatile("adr %0, ." : "=r"(pc));

    /* PC should be in 0xFFFF_xxxx_xxxx_xxxx range */
    ASSERT((pc >> 48) == 0xFFFF, "Kernel must run at high VA");
    return 0;
}
```

#### Test 2: TTBR0 and TTBR1 Are Different

```c
TEST(ttbr0_and_ttbr1_are_different) {
    paddr_t ttbr0 = read_ttbr0_el1() & PTE_ADDR_MASK;
    paddr_t ttbr1 = read_ttbr1_el1() & PTE_ADDR_MASK;

    ASSERT_NE(ttbr0, ttbr1, "Must use separate page tables");
    return 0;
}
```

#### Test 3: TTBR0 Switch Preserves Kernel

```c
TEST(ttbr0_switch_preserves_kernel) {
    /* Save original TTBR0 */
    paddr_t original = read_ttbr0_el1();

    /* Create empty user page table */
    paddr_t empty_l0 = pmem_alloc();

    /* Switch TTBR0 to empty table */
    write_ttbr0_el1(empty_l0);
    tlbi_vmalle1();

    /* Kernel should still work (we're still here!) */
    unsigned long test = 42;

    /* Restore */
    write_ttbr0_el1(original);
    tlbi_vmalle1();
    pmem_free(empty_l0);

    ASSERT_EQ(test, 42, "Kernel must survive TTBR0 switch");
    return 0;
}
```

#### Test 4: VA/PA Conversion Round-Trip

```c
TEST(va_pa_conversion_roundtrip) {
    paddr_t pa = RAM_BASE + 0x1000;
    void *va = PA_TO_VA(pa);
    paddr_t pa2 = VA_TO_PA(va);

    ASSERT_EQ(pa, pa2, "PA->VA->PA must be identity");
    ASSERT((unsigned long)va >> 48 == 0xFFFF, "VA must be high");
    return 0;
}
```

#### Test 5: VBAR at High Address

```c
TEST(vbar_at_high_address) {
    unsigned long vbar;
    __asm__ volatile("mrs %0, vbar_el1" : "=r"(vbar));

    ASSERT((vbar >> 48) == 0xFFFF, "VBAR must be at high address");
    return 0;
}
```

### 5.2 Why These Tests Matter

The TTBR0/identity mapping bug would have been caught by:
- **Test 1**: Kernel was at 0x4000_xxxx, not 0xFFFF_xxxx
- **Test 3**: Switching TTBR0 would have crashed immediately in the test

Current tests only check that MMU is enabled and addresses are accessible. They don't verify the architectural contract.

---

## 6. Prototype Kernel Specification

### 6.1 Purpose

Validate the boot sequence and MMU design in isolation before refactoring the main kernel.

### 6.2 Features

**Include:**
- Linker script with BOOT/KERNEL regions
- boot.S with PA→VA transition
- Static or minimal page table setup
- MMU enable
- UART output (to prove we're running)
- Print current PC to show we're at high address

**Exclude:**
- Interrupts
- Timer
- Process management
- Memory allocator
- Tests framework

### 6.3 Success Criteria

```
1. Boots without hanging
2. Prints "Hello from 0xFFFF_0000_4001_XXXX"
3. PC value shown is in TTBR1 range
4. Can switch TTBR0 to empty table without crashing
```

### 6.4 File Structure

```
prototype/
  Makefile        - Build rules
  linker.ld       - Two-region layout (BOOT + KERNEL)
  boot.S          - Configure MMU, enable, jump to high VA (~40 lines)
  tables.S        - Static page table data with macros (~80 lines)
  main.c          - Print PC, test TTBR0 switch
  uart.c          - Minimal UART driver
```

Total assembly: ~120 lines (boot + tables). No runtime MMU setup code.

### 6.5 Prototype Linker Script

```ld
ENTRY(_start)

MEMORY
{
    BOOT (rwx)   : ORIGIN = 0x40000000, LENGTH = 64K
    KERNEL (rwx) : ORIGIN = 0xFFFF000040010000, LENGTH = 1M
}

SECTIONS
{
    /* Boot code and tables at physical address */
    .text.boot : { *(.text.boot) } > BOOT
    . = ALIGN(4096);
    .tables : { *(.tables) } > BOOT

    /* Kernel at high virtual address */
    .text : AT(0x40010000) { *(.text*) } > KERNEL
    .rodata : { *(.rodata*) } > KERNEL
    .data : { *(.data*) } > KERNEL

    . = ALIGN(8);
    __bss_start = .;
    .bss : { *(.bss*) } > KERNEL
    __bss_end = .;

    . = ALIGN(16);
    . = . + 0x4000;
    __stack_top = .;
}
```

**Key points:**
- Tables in BOOT → symbols are physical addresses
- Kernel in KERNEL → symbols are virtual addresses
- `AT(0x40010000)` sets physical load address for kernel sections

---

## 7. Refactoring Roadmap

### Phase 1: Build and Validate Prototype

**Goal**: Prove the design works in isolation.

**Tasks**:
1. Create prototype/ directory
2. Implement minimal boot sequence
3. Verify kernel runs at high address
4. Test TTBR0 switching doesn't crash

**Deliverable**: Working prototype that prints from high address.

### Phase 2: Add Architectural Tests to Main Kernel

**Goal**: Catch future regressions.

**Tasks**:
1. Add tests from Section 5.1 to tests/test_mmu.c
2. Verify they pass with current (messy) implementation
3. These tests become the contract for refactoring

**Deliverable**: New tests in test_mmu.c that verify architectural invariants.

### Phase 3: Clean Up mmu.c

**Goal**: Remove dead code.

**Tasks**:
1. Delete `mmu_init()`
2. Delete `build_identity_tables()`
3. Delete `build_kernel_tables()`
4. Delete `configure_mmu_registers()`
5. Delete unused helpers (`table_pa`, `EARLY_PA`, `make_table_desc`, `make_block_desc_*`)
6. Keep only: page table arrays, `uvm_*` functions, `walk`, `make_page_desc_user`, `freewalk`

**Deliverable**: mmu.c reduced to ~80 lines (user page table management only).

### Phase 4: Replace early_mmu.S with Static Tables

**Goal**: Replace runtime table construction with static tables.

**Approach**: Static tables in assembly with macros (as described in Section 3.2.2).

**Tasks**:
1. Create tables.S with static table data using `.rept` macro
2. Simplify boot.S to just load TTBRs and configure registers (~40 lines)
3. Delete early_mmu.S entirely
4. Update Makefile
5. Verify tests still pass

**Deliverable**:
- New tables.S (~80 lines of data)
- Simplified boot.S (~40 lines)
- Deleted: early_mmu.S (186 lines of complex runtime code)

### Phase 5: Update Documentation

**Goal**: Ensure DESIGN.md and ROADMAP.md reflect the architecture.

**Tasks**:
1. Update DESIGN.md boot sequence section
2. Update DESIGN.md memory map to show TTBR0/TTBR1 split clearly
3. Update ROADMAP.md Milestone 5 to emphasize higher-half requirement
4. Add note that higher-half kernel is prerequisite for user mode
5. Archive or delete this refactoring document

**Deliverable**: Accurate documentation that would prevent this issue for future developers.

---

## Appendix A: Current State Reference

### Dead Code in mmu.c (to be removed)

```c
static paddr_t table_pa(void *table);
#define EARLY_PA(arr)
static pte_t make_table_desc(paddr_t next_table_pa);
static pte_t make_block_desc_device(paddr_t pa);
static pte_t make_block_desc_normal(paddr_t pa, int exec);
static void build_identity_tables(void);
static void build_kernel_tables(void);
static void configure_mmu_registers(void);
void mmu_init(void);
```

### Active Code After Refactoring

**tables.S (new file):**
```asm
/* Static page table data - 8 tables with pre-populated entries */
identity_l0, identity_l1, identity_l2_dev, identity_l2_ram
kernel_l0, kernel_l1, kernel_l2_dev, kernel_l2_ram
```

**mmu.c (simplified):**
```c
/* User page table management only - no kernel table code */
static pte_t make_page_desc_user(paddr_t pa, int write, int exec);
static pte_t *walk(pte_t *pagetable, unsigned long va, int alloc);
static void freewalk(pte_t *pagetable, int level);
int uvm_map_page(pte_t *pagetable, unsigned long va, paddr_t pa, int write, int exec);
pte_t *uvm_create(void);
void uvm_free(pte_t *pagetable);
```

**boot.S (simplified):**
```asm
/* ~40 lines: configure MMU regs, enable, prepare for C, jump to kernel_main */
/* No function calls, no early stack, no VA→PA conversions for tables */
```

### Constants Currently Scattered

| Constant | Defined In | Should Be In |
|----------|------------|--------------|
| KERNEL_BASE | pmem.h, boot.S, early_mmu.S | mem.h only |
| RAM_BASE | mmu.h | mem.h |
| RAM_SIZE | mmu.h | mem.h |
| UART0_PHYS | uart.h, early_mmu.S | mem.h |
| GICD_PHYS | gic.h, early_mmu.S | mem.h |
