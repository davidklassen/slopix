.section ".text.boot"
.global _start
.global enable_mmu

_start:
    // Set stack pointer
    ldr x0, =__stack_top
    mov sp, x0

    // Clear BSS
    ldr x0, =__bss_start
    ldr x1, =__bss_end
clear_bss:
    cmp x0, x1
    b.ge bss_done
    str xzr, [x0], #8
    b clear_bss

bss_done:
    // Jump to C main
    bl main

    // Halt if main returns
halt:
    wfe
    b halt

// Enable MMU
// x0 = TTBR0, x1 = TTBR1
enable_mmu:
    // Disable MMU and caches first
    mrs x2, sctlr_el1
    bic x2, x2, #1              // Clear M bit (MMU)
    bic x2, x2, #(1 << 2)       // Clear C bit (data cache)
    bic x2, x2, #(1 << 12)      // Clear I bit (instruction cache)
    msr sctlr_el1, x2
    isb

    // Set up MAIR (Memory Attribute Indirection Register)
    // Index 0: Device-nGnRnE (0x00)
    // Index 1: Normal, Non-cacheable (0x44)
    // Index 2: Normal, Cacheable (0xFF)
    ldr x2, =0x000044FF
    msr mair_el1, x2

    // Set up TCR (Translation Control Register)
    // T0SZ = 25 (39-bit address space for TTBR0)
    // T1SZ = 25 (39-bit address space for TTBR1)
    // TG0 = 0 (4KB granule for TTBR0)
    // TG1 = 2 (4KB granule for TTBR1)
    // IPS = 0 (32-bit physical address)
    mov x2, #25
    orr x2, x2, x2, lsl #16     // T1SZ = 25
    orr x2, x2, #(2 << 30)      // TG1 = 4KB
    msr tcr_el1, x2

    // Set TTBR0 and TTBR1
    msr ttbr0_el1, x0
    msr ttbr1_el1, x1
    isb

    // Invalidate TLBs
    tlbi vmalle1
    dsb sy
    isb

    // Enable MMU, caches, and alignment checking
    mrs x2, sctlr_el1
    orr x2, x2, #1              // Set M bit (MMU)
    orr x2, x2, #(1 << 2)       // Set C bit (data cache)
    orr x2, x2, #(1 << 12)      // Set I bit (instruction cache)
    msr sctlr_el1, x2
    isb

    ret
