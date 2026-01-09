.section ".text"

// Exception vector table for EL1
// Each entry is 128 bytes (0x80)
.align 11
.global exception_vector_table
exception_vector_table:
    // Current EL with SP0
    .align 7
    b exception_handler_sync          // Synchronous
    .align 7
    b exception_handler_irq            // IRQ
    .align 7
    b exception_handler_fiq            // FIQ
    .align 7
    b exception_handler_serror         // SError

    // Current EL with SPx
    .align 7
    b exception_handler_sync
    .align 7
    b exception_handler_irq
    .align 7
    b exception_handler_fiq
    .align 7
    b exception_handler_serror

    // Lower EL using AArch64
    .align 7
    b exception_handler_sync
    .align 7
    b exception_handler_irq
    .align 7
    b exception_handler_fiq
    .align 7
    b exception_handler_serror

    // Lower EL using AArch32
    .align 7
    b exception_handler_sync
    .align 7
    b exception_handler_irq
    .align 7
    b exception_handler_fiq
    .align 7
    b exception_handler_serror

// Exception handlers
exception_handler_sync:
    stp x29, x30, [sp, #-16]!
    bl handle_sync_exception
    ldp x29, x30, [sp], #16
    eret

exception_handler_irq:
    // Save ELR_EL1 and SPSR_EL1 first
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #-16]!

    // Save all general purpose registers to stack (x0-x30)
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x15, x16, [sp, #-16]!
    stp x13, x14, [sp, #-16]!
    stp x11, x12, [sp, #-16]!
    stp x9, x10, [sp, #-16]!
    stp x7, x8, [sp, #-16]!
    stp x5, x6, [sp, #-16]!
    stp x3, x4, [sp, #-16]!
    stp x1, x2, [sp, #-16]!
    stp xzr, x0, [sp, #-16]!

    // x0 = pointer to saved context on stack
    mov x0, sp

    // Call C IRQ handler (may do context switch)
    bl handle_irq_with_context

    // handle_irq_with_context returns potentially new stack pointer in x0
    mov sp, x0

    // Restore all registers from (potentially new) stack
    ldp xzr, x0, [sp], #16
    ldp x1, x2, [sp], #16
    ldp x3, x4, [sp], #16
    ldp x5, x6, [sp], #16
    ldp x7, x8, [sp], #16
    ldp x9, x10, [sp], #16
    ldp x11, x12, [sp], #16
    ldp x13, x14, [sp], #16
    ldp x15, x16, [sp], #16
    ldp x17, x18, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16

    // Restore ELR_EL1 and SPSR_EL1
    ldp x0, x1, [sp], #16
    msr elr_el1, x0
    msr spsr_el1, x1

    eret

exception_handler_fiq:
    stp x29, x30, [sp, #-16]!
    bl handle_fiq
    ldp x29, x30, [sp], #16
    eret

exception_handler_serror:
    stp x29, x30, [sp, #-16]!
    bl handle_serror
    ldp x29, x30, [sp], #16
    eret
