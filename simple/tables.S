// vim: ft=arm64asm
// tables.S - Static page tables for higher-half kernel
//
// Two sets of tables:
//   - Identity tables (TTBR0): VA = PA for boot trampoline
//   - Kernel tables (TTBR1): VA = PA + 0xFFFF_0000_0000_0000
//
// Both map the same physical memory with identical structure:
//   L0[0] -> L1
//   L1[0] -> L2_dev (devices 0x0000_0000 - 0x3FFF_FFFF)
//   L1[1] -> L2_ram (RAM 0x4000_0000 - 0x7FFF_FFFF)

// Page table entry flags
.equ PTE_VALID,     (1 << 0)
.equ PTE_TABLE,     (1 << 1)
.equ PTE_AF,        (1 << 10)
.equ PTE_SH_INNER,  (3 << 8)
.equ PTE_SH_OUTER,  (2 << 8)
.equ PTE_ATTR_DEV,  (0 << 2)    // MAIR index 0 = Device
.equ PTE_ATTR_NORM, (1 << 2)    // MAIR index 1 = Normal
.equ PTE_UXN,       (1 << 54)
.equ PTE_PXN,       (1 << 53)

// Combined flags
.equ TABLE_FLAGS,   (PTE_VALID | PTE_TABLE)
.equ DEV_FLAGS,     (PTE_VALID | PTE_AF | PTE_SH_OUTER | PTE_ATTR_DEV | PTE_UXN | PTE_PXN)
.equ RAM_FLAGS,     (PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_ATTR_NORM | PTE_UXN)

// Physical addresses
.equ GICD_BASE,     0x08000000
.equ UART_BASE,     0x09000000
.equ RAM_BASE,      0x40000000

// L2 index calculations (2MB per entry):
// GIC:  0x08000000 / 0x200000 = 64
// UART: 0x09000000 / 0x200000 = 72
// RAM:  starts at entry 0 in L2_ram (L1[1] covers 0x40000000+)

// Macro: Generate N consecutive 2MB block entries
.macro ram_blocks base, flags, count
    .set ADDR, \base
    .rept \count
        .quad ADDR + \flags
        .set ADDR, ADDR + 0x200000
    .endr
.endm


.section .tables, "aw"

// ============ Identity Tables (TTBR0) ============

.balign 4096
.global identity_l0
identity_l0:
    .quad identity_l1 + TABLE_FLAGS     // Entry 0: -> L1
    .fill 511, 8, 0                     // Entries 1-511: empty

.balign 4096
identity_l1:
    .quad identity_l2_dev + TABLE_FLAGS // Entry 0: devices (0x0 - 0x3FFFFFFF)
    .quad identity_l2_ram + TABLE_FLAGS // Entry 1: RAM (0x40000000 - 0x7FFFFFFF)
    .fill 510, 8, 0                     // Entries 2-511: empty

.balign 4096
identity_l2_dev:
    .fill 64, 8, 0                      // Entries 0-63: empty
    .quad GICD_BASE + DEV_FLAGS         // Entry 64: GIC (0x08000000)
    .fill 7, 8, 0                       // Entries 65-71: empty
    .quad UART_BASE + DEV_FLAGS         // Entry 72: UART (0x09000000)
    .fill 439, 8, 0                     // Entries 73-511: empty

.balign 4096
identity_l2_ram:
    ram_blocks RAM_BASE, RAM_FLAGS, 64  // Entries 0-63: 128MB RAM (64 x 2MB)
    .fill 448, 8, 0                     // Entries 64-511: empty


// ============ Kernel Tables (TTBR1) ============

.balign 4096
.global kernel_l0
kernel_l0:
    .quad kernel_l1 + TABLE_FLAGS       // Entry 0: -> L1
    .fill 511, 8, 0                     // Entries 1-511: empty

.balign 4096
kernel_l1:
    .quad kernel_l2_dev + TABLE_FLAGS   // Entry 0: devices
    .quad kernel_l2_ram + TABLE_FLAGS   // Entry 1: RAM
    .fill 510, 8, 0                     // Entries 2-511: empty

.balign 4096
kernel_l2_dev:
    .fill 64, 8, 0                      // Entries 0-63: empty
    .quad GICD_BASE + DEV_FLAGS         // Entry 64: GIC
    .fill 7, 8, 0                       // Entries 65-71: empty
    .quad UART_BASE + DEV_FLAGS         // Entry 72: UART
    .fill 439, 8, 0                     // Entries 73-511: empty

.balign 4096
kernel_l2_ram:
    ram_blocks RAM_BASE, RAM_FLAGS, 64  // Entries 0-63: 128MB RAM
    .fill 448, 8, 0                     // Entries 64-511: empty
