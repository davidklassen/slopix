// vim: ft=arm64asm
// AArch64 exception vectors
// Vector table must be 2KB aligned, each entry is 128 bytes

.section .text

// Trap frame: 272 bytes (34 * 8), 16-byte aligned
// [sp, #0]   - [sp, #240]: x0-x30
// [sp, #248]: SP_EL0
// [sp, #256]: ELR_EL1
// [sp, #264]: SPSR_EL1

.macro save_trap_frame
    sub     sp, sp, #272
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30, [sp, #240]

    mrs     x0, sp_el0
    str     x0, [sp, #248]

    mrs     x0, elr_el1
    str     x0, [sp, #256]

    mrs     x0, spsr_el1
    str     x0, [sp, #264]
.endm

.macro restore_trap_frame
    ldr     x0, [sp, #264]
    msr     spsr_el1, x0

    ldr     x0, [sp, #256]
    msr     elr_el1, x0

    ldr     x0, [sp, #248]
    msr     sp_el0, x0

    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30, [sp, #240]
    add     sp, sp, #272
.endm

.macro vector_entry label
    .balign 0x80
\label:
.endm

// Panic stub: load vector ID and branch to panic entry
.macro panic_vector id
    mov     x0, #\id
    b       panic_entry
.endm

// Panic entry point - ensure SP_EL1 and call C handler
panic_entry:
    msr     spsel, #1
    mrs     x1, elr_el1
    mrs     x2, esr_el1
    b       panic

// Vector table: 16 entries, 2KB aligned
.balign 0x800
.global vectors
vectors:

// Current EL with SP0
vector_entry vec_sync_sp0
    panic_vector 0

vector_entry vec_irq_sp0
    panic_vector 1

vector_entry vec_fiq_sp0
    panic_vector 2

vector_entry vec_serror_sp0
    panic_vector 3

// Current EL with SPx
vector_entry vec_sync_spx
    b       sync_entry

vector_entry vec_irq_spx
    b       irq_entry

vector_entry vec_fiq_spx
    panic_vector 6

vector_entry vec_serror_spx
    panic_vector 7

// Lower EL using AArch64
vector_entry vec_sync_lower64
    panic_vector 8

vector_entry vec_irq_lower64
    panic_vector 9

vector_entry vec_fiq_lower64
    panic_vector 10

vector_entry vec_serror_lower64
    panic_vector 11

// Lower EL using AArch32
vector_entry vec_sync_lower32
    panic_vector 12

vector_entry vec_irq_lower32
    panic_vector 13

vector_entry vec_fiq_lower32
    panic_vector 14

vector_entry vec_serror_lower32
    panic_vector 15


// Synchronous exception entry
sync_entry:
    save_trap_frame
    mov     x0, sp
    bl      sync_exception_handler
    restore_trap_frame
    eret

// IRQ entry
irq_entry:
    save_trap_frame
    mov     x0, sp
    bl      irq_handler
    restore_trap_frame
    eret
