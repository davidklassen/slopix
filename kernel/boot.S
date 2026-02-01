// vim: ft=arm64asm
// boot.S - Minimal boot sequence with static page tables
//
// Boot code runs at physical address 0x40000000.
// After MMU enable, jumps to kernel at high VA 0xFFFF000040000000.

.equ CPACR_EL1_FPEN, (3 << 20)

.equ MAIR_VALUE,       0xFF00
.equ TCR_VALUE,        0x1B5103510
.equ SCTLR_M,          (1 << 0)
.equ SCTLR_C,          (1 << 2)
.equ SCTLR_I,          (1 << 12)
.equ SCTLR_MMU_ENABLE, SCTLR_M | SCTLR_C | SCTLR_I
.equ KERNEL_BASE,      0xFFFF000000000000

.section .text.boot
.global _start

_start:
    // Save DTB address passed by QEMU in x0
    mov     x19, x0

    // Stage 1: Configure MMU (no stack needed)
    ldr     x0, =MAIR_VALUE
    msr     mair_el1, x0
    ldr     x0, =TCR_VALUE
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
    ldr     x1, =SCTLR_MMU_ENABLE
    orr     x0, x0, x1
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
.bss_loop:
    cmp     x0, x1
    b.ge    .bss_done
    str     xzr, [x0], #8
    b       .bss_loop
.bss_done:

    // Store DTB virtual address (PA + kernel base offset)
    ldr     x0, =_dtb_address
    ldr     x1, =KERNEL_BASE
    add     x1, x19, x1
    str     x1, [x0]

    // Stage 4: Jump to kernel at high VA
    ldr     x0, =kernel_main
    br      x0

.global halt
halt:
    wfe
    b       halt

.section .data
.global _dtb_address
_dtb_address:
    .quad 0
