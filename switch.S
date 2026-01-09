.section ".text"
.global switch_context

// void switch_context(cpu_context_t *old_ctx, cpu_context_t *new_ctx)
// x0 = old context, x1 = new context
switch_context:
    // Save old context (if old_ctx is not NULL)
    cbz x0, load_new

    // Save callee-saved registers
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]

    // Save stack pointer
    mov x9, sp
    str x9, [x0, #96]

    // Save return address (PC)
    adr x9, 1f
    str x9, [x0, #104]

    // Save DAIF (interrupt mask state)
    mrs x9, daif
    str x9, [x0, #112]

load_new:
    // Load new context
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]

    // Load stack pointer
    ldr x9, [x1, #96]
    mov sp, x9

    // Load DAIF (interrupt mask state)
    ldr x9, [x1, #112]
    msr daif, x9

    // Load PC and jump
    ldr x9, [x1, #104]
    br x9

1:
    ret
