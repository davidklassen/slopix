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
.equ VIRTIO_BASE,   0x0a000000
.equ RAM_BASE,      0x40000000

// L2 index calculations (2MB per entry):
// GIC:    0x08000000 / 0x200000 = 64
// UART:   0x09000000 / 0x200000 = 72
// VIRTIO: 0x0a000000 / 0x200000 = 80
// RAM:    starts at entry 0 in L2_ram (L1[1] covers 0x40000000+)

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
    .fill 7, 8, 0                       // Entries 73-79: empty
    .quad VIRTIO_BASE + DEV_FLAGS       // Entry 80: VIRTIO (0x0a000000)
    .fill 431, 8, 0                     // Entries 81-511: empty

.balign 4096
identity_l2_ram:
    .quad 0x40000040000705
    .quad 0x40000040200705
    .quad 0x40000040400705
    .quad 0x40000040600705
    .quad 0x40000040800705
    .quad 0x40000040a00705
    .quad 0x40000040c00705
    .quad 0x40000040e00705
    .quad 0x40000041000705
    .quad 0x40000041200705
    .quad 0x40000041400705
    .quad 0x40000041600705
    .quad 0x40000041800705
    .quad 0x40000041a00705
    .quad 0x40000041c00705
    .quad 0x40000041e00705
    .quad 0x40000042000705
    .quad 0x40000042200705
    .quad 0x40000042400705
    .quad 0x40000042600705
    .quad 0x40000042800705
    .quad 0x40000042a00705
    .quad 0x40000042c00705
    .quad 0x40000042e00705
    .quad 0x40000043000705
    .quad 0x40000043200705
    .quad 0x40000043400705
    .quad 0x40000043600705
    .quad 0x40000043800705
    .quad 0x40000043a00705
    .quad 0x40000043c00705
    .quad 0x40000043e00705
    .quad 0x40000044000705
    .quad 0x40000044200705
    .quad 0x40000044400705
    .quad 0x40000044600705
    .quad 0x40000044800705
    .quad 0x40000044a00705
    .quad 0x40000044c00705
    .quad 0x40000044e00705
    .quad 0x40000045000705
    .quad 0x40000045200705
    .quad 0x40000045400705
    .quad 0x40000045600705
    .quad 0x40000045800705
    .quad 0x40000045a00705
    .quad 0x40000045c00705
    .quad 0x40000045e00705
    .quad 0x40000046000705
    .quad 0x40000046200705
    .quad 0x40000046400705
    .quad 0x40000046600705
    .quad 0x40000046800705
    .quad 0x40000046a00705
    .quad 0x40000046c00705
    .quad 0x40000046e00705
    .quad 0x40000047000705
    .quad 0x40000047200705
    .quad 0x40000047400705
    .quad 0x40000047600705
    .quad 0x40000047800705
    .quad 0x40000047a00705
    .quad 0x40000047c00705
    .quad 0x40000047e00705
    .fill 448, 8, 0


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
    .fill 7, 8, 0                       // Entries 73-79: empty
    .quad VIRTIO_BASE + DEV_FLAGS       // Entry 80: VIRTIO
    .fill 431, 8, 0                     // Entries 81-511: empty

.balign 4096
kernel_l2_ram:
    .quad 0x40000040000705
    .quad 0x40000040200705
    .quad 0x40000040400705
    .quad 0x40000040600705
    .quad 0x40000040800705
    .quad 0x40000040a00705
    .quad 0x40000040c00705
    .quad 0x40000040e00705
    .quad 0x40000041000705
    .quad 0x40000041200705
    .quad 0x40000041400705
    .quad 0x40000041600705
    .quad 0x40000041800705
    .quad 0x40000041a00705
    .quad 0x40000041c00705
    .quad 0x40000041e00705
    .quad 0x40000042000705
    .quad 0x40000042200705
    .quad 0x40000042400705
    .quad 0x40000042600705
    .quad 0x40000042800705
    .quad 0x40000042a00705
    .quad 0x40000042c00705
    .quad 0x40000042e00705
    .quad 0x40000043000705
    .quad 0x40000043200705
    .quad 0x40000043400705
    .quad 0x40000043600705
    .quad 0x40000043800705
    .quad 0x40000043a00705
    .quad 0x40000043c00705
    .quad 0x40000043e00705
    .quad 0x40000044000705
    .quad 0x40000044200705
    .quad 0x40000044400705
    .quad 0x40000044600705
    .quad 0x40000044800705
    .quad 0x40000044a00705
    .quad 0x40000044c00705
    .quad 0x40000044e00705
    .quad 0x40000045000705
    .quad 0x40000045200705
    .quad 0x40000045400705
    .quad 0x40000045600705
    .quad 0x40000045800705
    .quad 0x40000045a00705
    .quad 0x40000045c00705
    .quad 0x40000045e00705
    .quad 0x40000046000705
    .quad 0x40000046200705
    .quad 0x40000046400705
    .quad 0x40000046600705
    .quad 0x40000046800705
    .quad 0x40000046a00705
    .quad 0x40000046c00705
    .quad 0x40000046e00705
    .quad 0x40000047000705
    .quad 0x40000047200705
    .quad 0x40000047400705
    .quad 0x40000047600705
    .quad 0x40000047800705
    .quad 0x40000047a00705
    .quad 0x40000047c00705
    .quad 0x40000047e00705
    .fill 448, 8, 0
