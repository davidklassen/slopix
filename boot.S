// vim: ft=arm64asm
// boot.S - Minimal boot sequence with static page tables
//
// Boot code runs at physical address 0x40000000.
// After MMU enable, jumps to kernel at high VA 0xFFFF000040000000.

.equ CPACR_EL1_FPEN, (3 << 20)

.section .text.boot
.global _start

_start:
    // Stage 1: Configure MMU (no stack needed)
    ldr     x0, =0xFF00             // MAIR: idx0=device, idx1=normal
    msr     mair_el1, x0
    ldr     x0, =0x1B5103510        // TCR: T0SZ=16, T1SZ=16, 4KB granule
    msr     tcr_el1, x0
    ldr     x0, =identity_l0        // TTBR0 (identity, VA=PA)
    msr     ttbr0_el1, x0
    ldr     x0, =kernel_l0          // TTBR1 (kernel, high VA)
    msr     ttbr1_el1, x0
    isb

    // Stage 2: Enable MMU
    dsb     sy
    tlbi    vmalle1
    dsb     sy
    isb
    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 0)       // M - MMU enable
    orr     x0, x0, #(1 << 2)       // C - data cache
    orr     x0, x0, #(1 << 12)      // I - instruction cache
    msr     sctlr_el1, x0
    isb

    // Stage 3: Prepare for C (MMU on, high VA works)
    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

    ldr     x0, =__stack_top
    mov     sp, x0

    mov     x0, #CPACR_EL1_FPEN     // Enable FP/SIMD
    msr     cpacr_el1, x0
    isb

    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
1:  cmp     x0, x1
    b.ge    2f
    str     xzr, [x0], #8
    b       1b
2:

    // Stage 4: Jump to kernel at high VA
    ldr     x0, =kernel_main
    br      x0

.global halt
halt:
    wfe
    b       halt
