// vim: ft=arm64asm
// Early MMU setup for AArch64 - runs before MMU is enabled
// Uses explicit physical address computation to avoid PC-relative adrp issues

.section .text.boot
.global early_mmu_setup

#define KERNEL_BASE     0xFFFF000000000000
#define RAM_BASE        0x40000000
#define GICD_PHYS       0x08000000
#define UART0_PHYS      0x09000000
#define BLOCK_SIZE_2MB  0x200000
#define RAM_SIZE        0x08000000

// Page table entry bits
#define PTE_VALID       (1 << 0)
#define PTE_TABLE       (1 << 1)
#define PTE_AF          (1 << 10)

// MAIR_EL1 attributes: index 0 = device, index 1 = normal
#define MAIR_DEVICE     0x00
#define MAIR_NORMAL     0xFF
#define MAIR_VALUE      ((MAIR_NORMAL << 8) | MAIR_DEVICE)

/*
 * early_mmu_setup - Set up page tables and MMU registers before MMU enable
 *
 * Requires page table symbols:
 *   identity_l0, identity_l1, identity_l2_dev, identity_l2_ram
 *   kernel_l0, kernel_l1, kernel_l2_dev, kernel_l2_ram
 *
 * Uses x0-x15 as scratch registers
 */
early_mmu_setup:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    // Load kernel base for VA to PA conversion
    movz    x15, #0xFFFF, lsl #48       // x15 = KERNEL_BASE = 0xFFFF000000000000

    // =========================================
    // Get physical addresses of page tables
    // (symbols are at high VA, subtract KERNEL_BASE)
    // =========================================
    ldr     x0, =identity_l0
    sub     x0, x0, x15                 // x0 = identity_l0 PA
    ldr     x1, =identity_l1
    sub     x1, x1, x15                 // x1 = identity_l1 PA
    ldr     x2, =identity_l2_dev
    sub     x2, x2, x15                 // x2 = identity_l2_dev PA
    ldr     x3, =identity_l2_ram
    sub     x3, x3, x15                 // x3 = identity_l2_ram PA

    ldr     x4, =kernel_l0
    sub     x4, x4, x15                 // x4 = kernel_l0 PA
    ldr     x5, =kernel_l1
    sub     x5, x5, x15                 // x5 = kernel_l1 PA
    ldr     x6, =kernel_l2_dev
    sub     x6, x6, x15                 // x6 = kernel_l2_dev PA
    ldr     x7, =kernel_l2_ram
    sub     x7, x7, x15                 // x7 = kernel_l2_ram PA

    // =========================================
    // Build identity tables (TTBR0)
    // =========================================

    // identity_l0[0] = table descriptor pointing to identity_l1
    // Table descriptor: PA | PTE_TABLE | PTE_VALID = PA | 0x3
    mov     x8, x1
    orr     x8, x8, #0x3                // PTE_VALID | PTE_TABLE
    str     x8, [x0]

    // identity_l1[0] = table descriptor pointing to identity_l2_dev
    mov     x8, x2
    orr     x8, x8, #0x3
    str     x8, [x1]

    // identity_l1[1] = table descriptor pointing to identity_l2_ram
    mov     x8, x3
    orr     x8, x8, #0x3
    str     x8, [x1, #8]

    // identity_l2_dev[64] = GICD_PHYS (0x08000000)
    // L2 index for 0x08000000 = (0x08000000 >> 21) & 0x1FF = 64
    // Block descriptor for device: PA | AF | SH_OUTER | ATTR_DEVICE(idx 0) | UXN | PXN | VALID
    // = PA | (1<<10) | (2<<8) | (0<<2) | (1<<54) | (1<<53) | 1 = PA | 0x601 | UXN | PXN
    mov     x8, #GICD_PHYS
    mov     x9, #0x601                  // AF(10) | SH_OUTER(8,9=2) | ATTR_DEV(0) | VALID(0)
    orr     x8, x8, x9
    movk    x8, #0x0060, lsl #48        // UXN(54) | PXN(53)
    str     x8, [x2, #(64 * 8)]

    // identity_l2_dev[72] = UART0_PHYS (0x09000000)
    // L2 index for 0x09000000 = (0x09000000 >> 21) & 0x1FF = 72
    mov     x8, #UART0_PHYS
    mov     x9, #0x601
    orr     x8, x8, x9
    movk    x8, #0x0060, lsl #48
    str     x8, [x2, #(72 * 8)]

    // RAM mappings in identity_l2_ram (64 blocks of 2MB = 128MB)
    // Block descriptor for normal: PA | AF | SH_INNER | ATTR_NORMAL(idx 1) | UXN | VALID
    // ATTR_NORMAL = index 1 (1<<2) = 4, SH_INNER = (3<<8) = 0x300
    // = PA | (1<<10) | (3<<8) | (1<<2) | (1<<54) | 1 = PA | 0x705 | UXN
    mov     x9, #0                      // block counter
    mov     x10, #RAM_BASE              // current PA
    mov     x11, #0x705                 // AF | SH_INNER | ATTR_NORMAL | VALID
1:
    mov     x8, x10
    orr     x8, x8, x11
    movk    x8, #0x0040, lsl #48        // UXN only (allow kernel execute)
    str     x8, [x3, x9, lsl #3]
    add     x9, x9, #1
    add     x10, x10, #BLOCK_SIZE_2MB
    cmp     x9, #64
    b.lt    1b

    // =========================================
    // Build kernel tables (TTBR1) - same mappings, different table addresses
    // =========================================

    // kernel_l0[0] = table descriptor pointing to kernel_l1
    mov     x8, x5
    orr     x8, x8, #0x3
    str     x8, [x4]

    // kernel_l1[0] = table descriptor pointing to kernel_l2_dev
    mov     x8, x6
    orr     x8, x8, #0x3
    str     x8, [x5]

    // kernel_l1[1] = table descriptor pointing to kernel_l2_ram
    mov     x8, x7
    orr     x8, x8, #0x3
    str     x8, [x5, #8]

    // kernel_l2_dev entries
    mov     x8, #GICD_PHYS
    mov     x9, #0x601
    orr     x8, x8, x9
    movk    x8, #0x0060, lsl #48
    str     x8, [x6, #(64 * 8)]

    mov     x8, #UART0_PHYS
    mov     x9, #0x601
    orr     x8, x8, x9
    movk    x8, #0x0060, lsl #48
    str     x8, [x6, #(72 * 8)]

    // kernel_l2_ram mappings
    mov     x9, #0
    mov     x10, #RAM_BASE
    mov     x11, #0x705
2:
    mov     x8, x10
    orr     x8, x8, x11
    movk    x8, #0x0040, lsl #48
    str     x8, [x7, x9, lsl #3]
    add     x9, x9, #1
    add     x10, x10, #BLOCK_SIZE_2MB
    cmp     x9, #64
    b.lt    2b

    // =========================================
    // Configure MMU registers
    // =========================================

    // MAIR_EL1: index 0 = device (0x00), index 1 = normal (0xFF)
    mov     x8, #MAIR_VALUE
    msr     mair_el1, x8

    // TCR_EL1: pre-computed value from mmu.h TCR_VALUE
    // T0SZ=16, T1SZ=16, TG0=4KB, TG1=4KB, SH0/1=inner, ORGN0/1=WB, IRGN0/1=WB, IPS=36bit
    ldr     x8, =0x1B5103510
    msr     tcr_el1, x8

    // TTBR0_EL1 = identity_l0 PA
    msr     ttbr0_el1, x0

    // TTBR1_EL1 = kernel_l0 PA
    msr     ttbr1_el1, x4

    isb

    ldp     x29, x30, [sp], #16
    ret
