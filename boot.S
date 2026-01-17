// vim: ft=arm64asm
// CPACR_EL1: FPEN bits [21:20] = 0b11 enables FP/SIMD at EL1 and EL0
#define CPACR_EL1_FPEN (3 << 20)

.section .text.boot
.global _start

_start:
    // Enable FP/SIMD for EL1
    mov     x0, #CPACR_EL1_FPEN
    msr     cpacr_el1, x0
    isb

    ldr     x0, =__stack_top
    mov     sp, x0

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

    // Halt if main returns
halt:
    wfe
    b       halt
