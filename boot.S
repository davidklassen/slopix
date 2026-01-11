.section ".text.boot"
.global _start

_start:
    // Set stack pointer (use physical address before MMU/transition)
    ldr x0, =__stack_top_phys
    mov sp, x0

    // Clear BSS (use physical addresses)
    ldr x0, =__bss_start_phys
    ldr x1, =__bss_end_phys
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

    // Invalidate instruction cache before enabling
    ic iallu        // Invalidate all instruction caches to PoU
    dsb sy          // Ensure invalidation completes
    isb             // Synchronize context

    // Ensure all prior operations complete before enabling MMU
    dsb sy

    // Read current SCTLR_EL1
    mrs x0, sctlr_el1

    // Enable MMU, instruction cache, and data cache
    // Kernel memory is mapped with MT_NORMAL (cacheable) attribute
    orr x0, x0, #(1 << 0)   // M bit - MMU enable
    orr x0, x0, #(1 << 2)   // C bit - Data cache enable
    orr x0, x0, #(1 << 12)  // I bit - Instruction cache enable

    // Write back to SCTLR_EL1
    msr sctlr_el1, x0
    isb

    ret

// Transition from physical to higher-half virtual addresses
// Must be called AFTER MMU is enabled
// On entry: MMU enabled, executing from physical addresses (TTBR0 identity mapping)
// On exit: Executing from higher-half (TTBR1), stack pointer updated
.global transition_to_higher_half
transition_to_higher_half:
    // Save return address (lr) because we'll be changing address spaces
    mov x3, x30

    // Load higher-half offset: 0xFFFFFF8000000000
    movz x1, #0x0000, lsl #0
    movk x1, #0x0000, lsl #16
    movk x1, #0xFF80, lsl #32
    movk x1, #0xFFFF, lsl #48

    // Get current stack pointer (physical)
    mov x2, sp

    // Convert SP to higher-half virtual address
    add x2, x2, x1

    // Update stack pointer to higher-half
    mov sp, x2

    // Update return address to higher-half
    add x3, x3, x1

    // Get address of next instruction (label 1f)
    adr x0, 1f

    // Convert to higher-half address
    add x0, x0, x1

    // Jump to higher-half - after this, PC will be in TTBR1 range
    br x0

1:  // Now executing from higher-half!
    // Restore return address and return
    mov x30, x3
    ret
