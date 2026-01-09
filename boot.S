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
    // Jump to C main
    bl main

    // Halt if main returns
halt:
    wfe
    b halt
