.section ".text.boot"
.global _start

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
    // Configure MMU registers (but don't enable MMU yet)

    // Set MAIR_EL1 = 0x00FF4400
    // Attr0 = 0x00 (Device memory)
    // Attr1 = 0x44 (Normal memory, non-cacheable)
    // Attr2 = 0xFF (Normal memory, write-back cacheable)
    ldr x0, =0x00FF4400
    msr mair_el1, x0
    dsb sy          // Ensure MAIR write completes
    isb             // Synchronize context

    // Set TCR_EL1 for 39-bit address space
    // T0SZ = 25 (bits 5:0) - 2^(64-25) = 512GB address space
    // T1SZ = 25 (bits 21:16) - 2^(64-25) = 512GB address space
    // TG0 = 0 (4KB granule for TTBR0, bits 15:14)
    // TG1 = 2 (4KB granule for TTBR1, bits 31:30)
    // IPS = 1 (bits 34:32) - 36-bit physical address space (64GB)
    mov x2, #25
    orr x2, x2, x2, lsl #16
    orr x2, x2, #(2 << 30)
    orr x2, x2, #(1 << 32)    // IPS = 1 (36-bit PA)
    msr tcr_el1, x2
    dsb sy          // Ensure TCR write completes
    isb             // Synchronize context

    // Call physical_setup function (UART, PMM, MMU init, TTBR setup)
    bl physical_setup

    // Jump to C main
    bl main

    // Halt if main returns
halt:
    wfe
    b halt

// Set TTBR0_EL1 and TTBR1_EL1 registers
// Parameters: x0 = TTBR0 value, x1 = TTBR1 value
.global set_ttbr_registers
set_ttbr_registers:
    msr ttbr0_el1, x0
    msr ttbr1_el1, x1
    dsb sy          // Ensure TTBR writes complete
    isb             // Synchronize context
    ret

// Enable MMU by setting SCTLR_EL1.M bit
.global enable_mmu
enable_mmu:
    // Invalidate all TLB entries
    tlbi vmalle1
    dsb sy
    isb

    // Ensure all prior operations complete before enabling MMU
    dsb sy

    // Read current SCTLR_EL1
    mrs x0, sctlr_el1

    // Set bit 0 (M - MMU enable) ONLY
    // Don't enable caches yet to simplify debugging
    orr x0, x0, #(1 << 0)   // M bit

    // Write back to SCTLR_EL1
    msr sctlr_el1, x0
    isb

    ret
