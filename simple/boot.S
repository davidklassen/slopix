// vim: ft=arm64asm
// boot.S - Minimal boot sequence for higher-half kernel
//
// Four stages:
//   1. Configure MMU registers (no stack needed)
//   2. Enable MMU (still at PA via identity mapping)
//   3. Prepare for C (still at low VA, data at high VA)
//   4. Jump to high VA

.section .text.boot
.global _start

_start:
    // ========== Stage 1: Configure MMU (at PA 0x40000000) ==========

    // MAIR_EL1: memory attributes
    // Attr0 = 0x00 = Device-nGnRnE
    // Attr1 = 0xFF = Normal, Write-Back
    ldr     x0, =0xFF00
    msr     mair_el1, x0

    // TCR_EL1: translation control
    // T0SZ = 16 (48-bit VA for TTBR0)
    // T1SZ = 16 (48-bit VA for TTBR1)
    // TG0 = 0 (4KB granule for TTBR0)
    // TG1 = 2 (4KB granule for TTBR1)
    // IPS = 0 (32-bit PA, enough for 128MB)
    ldr     x0, =0x1B5103510
    msr     tcr_el1, x0

    // TTBR0_EL1 = identity_l0 (PA, since tables in BOOT region)
    ldr     x0, =identity_l0
    msr     ttbr0_el1, x0

    // TTBR1_EL1 = kernel_l0 (PA, since tables in BOOT region)
    ldr     x0, =kernel_l0
    msr     ttbr1_el1, x0

    isb

    // ========== Stage 2: Enable MMU (at PA, identity-mapped) ==========

    dsb     sy
    tlbi    vmalle1
    dsb     sy
    isb

    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 0)   // M bit - enable MMU
    orr     x0, x0, #(1 << 2)   // C bit - data cache
    orr     x0, x0, #(1 << 12)  // I bit - instruction cache
    msr     sctlr_el1, x0
    isb

    // ========== Stage 3: Prepare for C (at low VA 0x40000xxx) ==========
    // After MMU enable, instruction fetch uses TTBR0 (identity mapping).
    // Data access to 0xFFFF... goes through TTBR1.

    // Stack at high VA (MMU translates via TTBR1)
    ldr     x0, =__stack_top
    mov     sp, x0

    // Enable FP/SIMD for C code
    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0
    isb

    // Clear BSS at high VA (MMU translates via TTBR1)
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
1:  cmp     x0, x1
    b.ge    2f
    str     xzr, [x0], #8
    b       1b
2:

    // ========== Stage 4: Jump to high VA ==========
    // Load kernel_main address (high VA: 0xFFFF_0000_4001_xxxx)
    ldr     x0, =kernel_main
    br      x0

// halt - infinite loop
.global halt
halt:
    wfe
    b       halt
