// vim: ft=arm64asm
#define CPACR_EL1_FPEN (3 << 20)

.section .text.boot
.global _start

_start:
    // Stack
    ldr     x0, =__stack_top
    mov     sp, x0

    // Exception vectors
    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

    // Enable FP/SIMD
    mov     x0, #CPACR_EL1_FPEN
    msr     cpacr_el1, x0
    isb

    // Clear BSS
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    cmp     x0, x1
    b.ge    bss_done
bss_loop:
    str     xzr, [x0], #8
    cmp     x0, x1
    b.lt    bss_loop
bss_done:

    bl      kernel_main

halt:
    wfe
    b       halt
