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
    isb

    // Set TCR_EL1 for 39-bit address space
    // T0SZ = 25 (bits 5:0) - 2^(64-25) = 512GB address space
    // T1SZ = 25 (bits 21:16) - 2^(64-25) = 512GB address space
    // TG0 = 0 (4KB granule for TTBR0, bits 15:14)
    // TG1 = 2 (4KB granule for TTBR1, bits 31:30)
    mov x2, #25
    orr x2, x2, x2, lsl #16
    orr x2, x2, #(2 << 30)
    msr tcr_el1, x2
    isb

    // Jump to C main
    bl main

    // Halt if main returns
halt:
    wfe
    b halt
