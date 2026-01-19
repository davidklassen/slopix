/* vim: ft=arm64asm */
#define CPACR_EL1_FPEN (3 << 20)
#define KERNEL_BASE    0xFFFF000000000000

/*
 * Boot code runs at physical address 0x40000000 before MMU enable.
 * Symbols are linked at high virtual addresses (0xFFFF000040000000).
 * Convert VA to PA by subtracting KERNEL_BASE.
 */

.section .text.boot
.global _start

_start:
    /* Stack - convert high VA to physical address */
    ldr     x0, =__stack_top
    movz    x1, #0xFFFF, lsl #48
    sub     x0, x0, x1
    mov     sp, x0

    /* Enable FP/SIMD */
    mov     x0, #CPACR_EL1_FPEN
    msr     cpacr_el1, x0
    isb

    /* Clear BSS - convert high VAs to physical addresses */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    movz    x2, #0xFFFF, lsl #48
    sub     x0, x0, x2
    sub     x1, x1, x2
    cmp     x0, x1
    b.ge    bss_done
bss_loop:
    str     xzr, [x0], #8
    cmp     x0, x1
    b.lt    bss_loop
bss_done:

    /* Set up page tables (runs at PA, all addresses computed explicitly) */
    bl      early_mmu_setup

    /* Enable MMU (mmu_enable is in .text.boot, runs at PA) */
    bl      mmu_enable

    /*
     * MMU is now enabled with identity mapping in TTBR0.
     * We're still running at low address. Jump to high address
     * so kernel runs from TTBR1 exclusively.
     */

    /* Set exception vectors to high address */
    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

    /* Set stack to high address */
    ldr     x0, =__stack_top
    mov     sp, x0

    /* Jump to kernel_main at high virtual address */
    ldr     x0, =kernel_main
    br      x0

halt:
    wfe
    b       halt
