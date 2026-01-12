.section ".text"

// ARM64 AAPCS64 calling convention:
// Callee-saved registers: x19-x28, x29 (FP), x30 (LR), SP
// Caller-saved registers: x0-x18
//
// Function: test_context_switch
// Purpose: Manually restore context and jump to process for testing
// Signature: void test_context_switch(void *context_frame)
// Parameters:
//   x0 = pointer to context frame (36 * 8 bytes)
//
// Context frame layout (from scheduler.c line 45):
// [sp_el0, ttbr0_el1, x2, xzr, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
//  x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
//  ELR, SPSR, x0, x1]
//
// Position mapping:
// 0:sp_el0, 1:ttbr0_el1, 2:x2, 3:xzr, 4:x3, 5:x4, 6:x5, 7:x6, 8:x7, 9:x8,
// 10:x9, 11:x10, 12:x11, 13:x12, 14:x13, 15:x14, 16:x15, 17:x16, 18:x17, 19:x18,
// 20:x19, 21:x20, 22:x21, 23:x22, 24:x23, 25:x24, 26:x25, 27:x26, 28:x27, 29:x28,
// 30:x29, 31:x30, 32:ELR/PC, 33:SPSR, 34:x0, 35:x1

.global test_context_switch
.align 4
test_context_switch:
    // Save callee-saved registers per AAPCS64
    // Need to save: x19-x28 (10 regs), x29 (FP), x30 (LR), SP
    // Total: 13 registers * 8 bytes = 104 bytes
    // Round up to 16-byte alignment: 112 bytes (14 * 8)

    stp x29, x30, [sp, #-112]!  // Save FP and LR, allocate stack space
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    stp x23, x24, [sp, #48]
    stp x25, x26, [sp, #64]
    stp x27, x28, [sp, #80]

    // Save original SP (before our frame) in case we need it
    // SP before decrement was sp + 112
    add x19, sp, #112
    str x19, [sp, #96]          // Save original SP

    // x0 contains pointer to context frame
    // We'll use it to load all registers
    mov x19, x0                  // Preserve context frame pointer

    // Load PC/ELR from position 32 (at offset 32*8 = 256 bytes)
    ldr x9, [x19, #256]          // x9 = PC to jump to

    // Load all general purpose registers from context frame
    // We'll do this in reverse order to avoid clobbering x19 too early

    // Load x1 (position 35)
    ldr x1, [x19, #280]

    // Load x0 (position 34)
    ldr x0, [x19, #272]

    // Skip SPSR (position 33) and ELR (position 32) - already have ELR in x9

    // Load x30 (position 31)
    ldr x30, [x19, #248]

    // Load x29 (position 30)
    ldr x29, [x19, #240]

    // Load x28 (position 29)
    ldr x28, [x19, #232]

    // Load x27 (position 28)
    ldr x27, [x19, #224]

    // Load x26 (position 27)
    ldr x26, [x19, #216]

    // Load x25 (position 26)
    ldr x25, [x19, #208]

    // Load x24 (position 25)
    ldr x24, [x19, #200]

    // Load x23 (position 24)
    ldr x23, [x19, #192]

    // Load x22 (position 23)
    ldr x22, [x19, #184]

    // Load x21 (position 22)
    ldr x21, [x19, #176]

    // Load x20 (position 21)
    ldr x20, [x19, #168]

    // Load x19 (position 20) - do this last since we're using x19
    ldr x10, [x19, #160]         // Temp save x19 value in x10

    // Load x18-x2 (positions 19-2)
    ldr x18, [x19, #152]
    ldr x17, [x19, #144]
    ldr x16, [x19, #136]
    ldr x15, [x19, #128]
    ldr x14, [x19, #120]
    ldr x13, [x19, #112]
    ldr x12, [x19, #104]
    ldr x11, [x19, #96]
    // x10 already has x19 value
    ldr x8, [x19, #72]
    ldr x7, [x19, #56]
    ldr x6, [x19, #48]
    ldr x5, [x19, #40]
    ldr x4, [x19, #32]
    ldr x3, [x19, #24]
    ldr x2, [x19, #16]

    // Now move x10 (which has x19 value) to x19
    mov x19, x10

    // Jump to the process entry point (in x9)
    blr x9

    // When process returns, restore our callee-saved registers
    ldp x19, x20, [sp, #16]
    ldp x21, x22, [sp, #32]
    ldp x23, x24, [sp, #48]
    ldp x25, x26, [sp, #64]
    ldp x27, x28, [sp, #80]
    ldp x29, x30, [sp], #112     // Restore FP, LR, and deallocate stack

    ret
