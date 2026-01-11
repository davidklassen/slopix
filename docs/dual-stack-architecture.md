# ARM64 Dual-Stack Architecture

**Target Architecture**: ARMv8-A AArch64
**Document Purpose**: Reference for understanding ARM64's dual-stack system (SP_EL0 vs SP_EL1)
**Audience**: SLOPIX developers working on exception handling and context switching
**Date**: 2026-01-11

---

## Table of Contents

1. [Overview](#overview)
2. [Stack Pointer Registers](#stack-pointer-registers)
3. [Exception Flow Diagram](#exception-flow-diagram)
4. [Context Switching Requirements](#context-switching-requirements)
5. [Exception Entry Behavior](#exception-entry-behavior)
6. [ERET Return Behavior](#eret-return-behavior)
7. [Manual SP_EL0 Management](#manual-sp_el0-management)
8. [Memory Layout](#memory-layout)
9. [SLOPIX Implementation](#slopix-implementation)
10. [Common Pitfalls](#common-pitfalls)
11. [References](#references)

---

## Overview

### Why Two Stack Pointers?

ARM64 architecture provides **separate stack pointers** for different exception levels to enable secure, efficient context switching between privileged kernel code (EL1) and unprivileged user code (EL0).

**Key Benefits**:
1. **Security**: User code cannot corrupt kernel stack during exception handling
2. **Isolation**: Kernel operations use trusted memory, preventing user manipulation
3. **Efficiency**: No need to validate or switch stacks manually on every exception
4. **Clarity**: Code at each exception level has a dedicated, well-defined stack

### The Dual-Stack Concept

Each process running at EL0 requires **two separate stacks**:

- **User Stack (SP_EL0)**: Used when executing user code at EL0
- **Kernel Stack (SP_EL1)**: Used when handling exceptions/syscalls at EL1

When an exception occurs (syscall, interrupt, page fault), the processor switches from SP_EL0 to SP_EL1, allowing the kernel to execute safely on its own stack.

---

## Stack Pointer Registers

### SP_EL0 - User Stack Pointer

**Purpose**: Holds the stack pointer for code executing at EL0 (userspace).

**Characteristics**:
- Only accessible at EL0 when executing user code
- Readable/writable from EL1+ using MSR/MRS instructions
- Points to user-controlled memory (not trusted by kernel)
- Must be 16-byte aligned per AAPCS64 requirements

**Reading SP_EL0 from EL1**:
```assembly
mrs x0, sp_el0        // Read user stack pointer into x0
```

**Writing SP_EL0 from EL1**:
```assembly
msr sp_el0, x0        // Write x0 to user stack pointer
```

**Use Cases**:
- User programs use SP_EL0 for function calls, local variables, return addresses
- Kernel saves SP_EL0 during exception entry to preserve user stack state
- Kernel restores SP_EL0 before ERET to resume user execution

### SP_EL1 - Kernel Stack Pointer

**Purpose**: Holds the stack pointer for code executing at EL1 (kernel).

**Characteristics**:
- Used by kernel code for exception handling
- Each process needs a dedicated kernel stack for handling its exceptions
- Points to kernel-controlled memory (trusted)
- Must be 16-byte aligned

**Accessing SP_EL1**:
```assembly
// When SPSel=1, 'sp' refers to SP_EL1
mov x0, sp            // Read current SP_EL1
mov sp, x1            // Write to SP_EL1

// Can also use MSR/MRS explicitly
mrs x0, sp_el1        // Read SP_EL1 (from EL2+)
msr sp_el1, x0        // Write SP_EL1 (from EL2+)
```

### SPSel - Stack Pointer Select Register

**Purpose**: Controls which stack pointer is used when executing at EL1.

**Values**:
- **SPSel.SP = 0**: Use SP_EL0 at all exception levels (shared stack - not recommended)
- **SPSel.SP = 1**: Use SP_ELx at each exception level (dedicated stacks - recommended)

**Setting SPSel**:
```assembly
msr spsel, #1         // Use SP_EL1 at EL1 (handler mode - EL1h)
msr spsel, #0         // Use SP_EL0 at EL1 (thread mode - EL1t, rarely used)
```

**Execution Modes**:
- **EL1h** (handler mode): EL1 with SPSel=1, uses SP_EL1 (normal kernel mode)
- **EL1t** (thread mode): EL1 with SPSel=0, uses SP_EL0 (unusual, rarely used)

**Best Practice**: Always use SPSel=1 in kernel mode to ensure separate kernel stack.

---

## Exception Flow Diagram

### EL0 → EL1 Transition (Exception Entry)

```
User Process (EL0)                    Kernel Handler (EL1)
==================                    ====================

  User Code
     |
     | svc #0 (syscall)
     v
  ┌─────────────────────────────────────────────────────────┐
  │  HARDWARE AUTOMATICALLY:                                │
  │  1. ELR_EL1 ← PC (save return address)                  │
  │  2. SPSR_EL1 ← PSTATE (save processor state)            │
  │  3. ESR_EL1 ← exception syndrome                        │
  │  4. Switch to EL1                                       │
  │  5. PC ← VBAR_EL1 + 0x400 (jump to handler)             │
  │  6. Mask interrupts (DAIF ← 1111)                       │
  │  7. SP ← SP_EL1 (if SPSel=1)  ← AUTOMATIC STACK SWITCH  │
  └─────────────────────────────────────────────────────────┘
     |
     v
  Exception Handler (at EL1, using SP_EL1)
     |
     | SOFTWARE MUST MANUALLY:
     | 1. Save all registers x0-x30
     | 2. Read and save SP_EL0 (mrs x0, sp_el0)
     | 3. Save ELR_EL1, SPSR_EL1
     | 4. Handle exception
     |
     v
  Syscall Handler
  (executes on kernel stack - SP_EL1)
```

### EL1 → EL0 Transition (Exception Return)

```
Kernel Handler (EL1)                  User Process (EL0)
====================                  ==================

  Syscall Complete
     |
     | SOFTWARE MUST MANUALLY:
     | 1. Restore all registers x0-x30
     | 2. Restore SP_EL0 (msr sp_el0, x0)  ← MANUAL RESTORE!
     | 3. Restore ELR_EL1, SPSR_EL1
     | 4. Execute eret
     v
  ┌─────────────────────────────────────────────────────────┐
  │  ERET INSTRUCTION AUTOMATICALLY:                        │
  │  1. PC ← ELR_EL1 (jump to user return address)          │
  │  2. PSTATE ← SPSR_EL1 (restore processor state)         │
  │  3. Drop to EL0 (based on SPSR_EL1 bits)                │
  │  4. SP ← SP_EL0 (uses EL0 stack pointer)                │
  │  5. Unmask interrupts (based on SPSR_EL1 DAIF)          │
  └─────────────────────────────────────────────────────────┘
     |
     v
  User Code Resumes
  (executes on user stack - SP_EL0)
```

### Stack Pointer State at Each Stage

```
Stage                    Active Stack Pointer     Stack Location
-----                    --------------------     --------------
User code executing      SP_EL0                   User memory
Exception occurs         [Hardware switches]      [Automatic]
Exception handler        SP_EL1                   Kernel memory
Syscall processing       SP_EL1                   Kernel memory
Preparing to return      SP_EL1                   Kernel memory
After ERET               SP_EL0                   User memory
```

**Critical Point**: Hardware automatically switches to SP_EL1 on exception entry (when SPSel=1), but SP_EL0 must be manually saved/restored by software.

---

## Context Switching Requirements

### What Must Be Saved for EL0 Processes

When an EL0 process is interrupted, the following state must be preserved:

**General-Purpose Registers** (31 registers):
- x0-x30: All general-purpose registers
- x29: Frame pointer (FP)
- x30: Link register (LR)

**Stack Pointers** (2 stack pointers):
- **SP_EL0**: User stack pointer (must be saved with `mrs x0, sp_el0`)
- **SP_EL1**: Kernel stack pointer (implicit in context save location)

**Exception Link Registers**:
- **ELR_EL1**: Return address (user program counter)
- **SPSR_EL1**: Saved processor state (includes EL, interrupt masks, flags)

**Address Space**:
- **TTBR0_EL1**: Process page table base (for per-process memory isolation)

**Total Context Frame**: 36 quad-words (288 bytes)

### SLOPIX Context Frame Structure

```c
typedef struct {
    unsigned long x0, x1, x2, x3, x4, x5, x6, x7;
    unsigned long x8, x9, x10, x11, x12, x13, x14, x15;
    unsigned long x16, x17, x18, x19, x20, x21, x22, x23;
    unsigned long x24, x25, x26, x27, x28, x29, x30;
    unsigned long sp_el1;         // Kernel stack pointer
    unsigned long pc;             // Program counter (ELR_EL1)
    unsigned long pstate;         // Processor state (SPSR_EL1)
    unsigned long sp_el0;         // User stack pointer
    unsigned long ttbr0_el1;      // Process page table base
    unsigned char exception_level; // 0=EL0, 1=EL1
} cpu_context_t;
```

### Context Save Order (Exception Entry)

```assembly
exception_handler_el0_sync:
    // 1. Save general-purpose registers to kernel stack (SP_EL1)
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    // ... (save x4-x30)

    // 2. Save exception link registers
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #-16]!

    // 3. Save user stack pointer
    mrs x0, sp_el0           // Read SP_EL0
    str x0, [sp, #-8]!       // Save to context frame

    // 4. Save TTBR0 (if switching processes)
    mrs x0, ttbr0_el1
    str x0, [sp, #-8]!

    // Context now saved, call C handler
    mov x0, sp
    bl el0_sync_handler
```

### Context Restore Order (Exception Return)

```assembly
exception_return_to_el0:
    // 1. Restore TTBR0 (if switching processes)
    ldr x0, [sp], #8
    msr ttbr0_el1, x0
    dsb sy
    isb

    // 2. Restore user stack pointer
    ldr x0, [sp], #8
    msr sp_el0, x0           // Write SP_EL0 ← CRITICAL!

    // 3. Restore exception link registers
    ldp x0, x1, [sp], #16
    msr elr_el1, x0
    msr spsr_el1, x1

    // 4. Restore general-purpose registers
    ldp x2, x3, [sp], #16
    // ... (restore x4-x30)
    ldp x0, x1, [sp], #16

    // 5. Return to user mode
    eret                     // PC ← ELR_EL1, PSTATE ← SPSR_EL1
```

---

## Exception Entry Behavior

### What Hardware Does Automatically

When an exception occurs (SVC, IRQ, data abort, etc.), the ARM64 processor **automatically** performs:

1. **Save Return State**:
   - ELR_EL1 ← PC (address of instruction after exception source)
   - SPSR_EL1 ← PSTATE (processor state including EL, SPSel, DAIF, NZCV)

2. **Record Exception Information**:
   - ESR_EL1 ← exception syndrome (exception class, instruction-specific info)

3. **Change Execution State**:
   - Switch to EL1 (if coming from EL0)
   - Set PSTATE.DAIF ← 1111 (mask all interrupts)

4. **Jump to Handler**:
   - PC ← VBAR_EL1 + offset (based on exception type and source EL)
   - For EL0 synchronous exceptions: VBAR_EL1 + 0x400

5. **Stack Pointer Selection**:
   - If SPSel = 1: Use SP_EL1 (dedicated kernel stack) ✓ Recommended
   - If SPSel = 0: Use SP_EL0 (shared stack) ✗ Not recommended

**Important**: Hardware does NOT save general-purpose registers (x0-x30). This is the software's responsibility.

### What Software Must Do Manually

The exception handler must:

1. **Save General-Purpose Registers**:
   - Save x0-x30 to kernel stack
   - Order matters: save in reverse order of restoration

2. **Save Stack Pointers**:
   - Read SP_EL0 using `mrs x0, sp_el0`
   - Save x0 to context frame
   - SP_EL1 is implicit (current stack pointer)

3. **Process Exception**:
   - Read ESR_EL1 to determine exception cause
   - Dispatch to appropriate handler (syscall, page fault, etc.)

4. **Restore Context**:
   - Restore all registers in reverse order
   - Write SP_EL0 using `msr sp_el0, x0`
   - Restore ELR_EL1, SPSR_EL1

5. **Return to User**:
   - Execute `eret` instruction

---

## ERET Return Behavior

### What ERET Does

The **ERET** (Exception Return) instruction is the only way to lower the exception level and return from an exception.

**ERET Automatically**:
1. **PC ← ELR_EL1**: Jump to saved return address
2. **PSTATE ← SPSR_EL1**: Restore processor state (includes EL, interrupt masks, flags)
3. **Drop Exception Level**: Change to EL specified in SPSR_EL1 bits [3:0]
4. **Select Stack Pointer**: Use SP determined by SPSR_EL1 bit [0]:
   - SPSR_EL1[0] = 0: Use SP_EL0
   - SPSR_EL1[0] = 1: Use SP_ELx

### What ERET Does NOT Do

**CRITICAL GOTCHA**: ERET does **NOT** automatically restore SP_EL0.

**Why This Matters**:
- If SP_EL0 is modified during exception handling (e.g., process switch)
- And you don't manually restore it before ERET
- The user process will resume with the wrong stack pointer
- This causes immediate stack corruption and crashes

**Example of Bug**:
```assembly
// BUG: Forgetting to restore SP_EL0
exception_return:
    ldp x0, x1, [sp], #16
    msr elr_el1, x0
    msr spsr_el1, x1
    ldp x0, x1, [sp], #16
    eret                     // SP_EL0 NOT restored - BUG!
```

**Correct Implementation**:
```assembly
// CORRECT: Manually restore SP_EL0
exception_return:
    ldr x0, [sp], #8
    msr sp_el0, x0           // ← MUST restore SP_EL0 manually!

    ldp x0, x1, [sp], #16
    msr elr_el1, x0
    msr spsr_el1, x1
    ldp x0, x1, [sp], #16
    eret                     // Now safe
```

### ERET Prerequisites

Before executing ERET, ensure:

1. **ELR_EL1 is valid**: Points to valid instruction in target address space
2. **SPSR_EL1 is valid**: Contains valid PSTATE for target EL (bits [3:0])
3. **SP_EL0 restored**: If returning to EL0, SP_EL0 must be set correctly
4. **Registers restored**: All general-purpose registers hold correct values
5. **Memory barriers executed**: If page tables were modified, execute DSB/ISB

### SPSR_EL1 Format for EL0 Return

```
Bits [3:0]: Execution mode
    0b0000 (0x0): EL0t - EL0 with SP_EL0 (typical user mode)
    0b0100 (0x4): EL1t - EL1 with SP_EL0 (rare)
    0b0101 (0x5): EL1h - EL1 with SP_EL1 (kernel mode)

Bits [9:6]: DAIF - Interrupt masks
    D=1: Debug exceptions masked
    A=1: SError masked
    I=1: IRQ masked
    F=1: FIQ masked

Typical EL0 SPSR_EL1 value: 0x0 (EL0t, interrupts enabled)
```

---

## Manual SP_EL0 Management

### Reading SP_EL0

**From EL1**:
```assembly
mrs x0, sp_el0        // Read user stack pointer into x0
str x0, [x1]          // Store to memory
```

**Use Cases**:
- Saving user stack pointer during exception entry
- Inspecting user stack for debugging
- Switching between processes (save old, load new)

### Writing SP_EL0

**From EL1**:
```assembly
ldr x0, [x1]          // Load new SP_EL0 from memory
msr sp_el0, x0        // Write to user stack pointer
```

**Use Cases**:
- Restoring user stack pointer before ERET
- Setting initial stack pointer for new EL0 process
- Switching to new process (restore new process's SP_EL0)

### Complete Save/Restore Example

**Saving context on exception entry**:
```assembly
el0_exception_entry:
    // At entry: SP = SP_EL1 (kernel stack)

    // Save caller-saved registers first
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!

    // Save user stack pointer
    mrs x0, sp_el0              // Read SP_EL0
    str x0, [sp, #-8]!          // Save to context frame

    // Save exception link registers
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #-16]!

    // ... save remaining registers

    // Call C handler with context pointer
    mov x0, sp
    bl handle_exception
```

**Restoring context on exception return**:
```assembly
el0_exception_return:
    // At entry: SP points to context frame on SP_EL1

    // Restore exception link registers
    ldp x0, x1, [sp], #16
    msr elr_el1, x0
    msr spsr_el1, x1

    // Restore user stack pointer
    ldr x0, [sp], #8
    msr sp_el0, x0              // Write SP_EL0 ← CRITICAL!

    // Restore caller-saved registers
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16

    // Return to user mode
    eret
```

### Process Context Switch

**Switching from Process A to Process B**:
```c
void context_switch(process_t *from, process_t *to) {
    // Save current process context
    from->context.sp_el1 = current_sp();        // Kernel stack
    __asm__ volatile("mrs %0, sp_el0" : "=r"(from->context.sp_el0));
    from->context.ttbr0_el1 = read_ttbr0_el1();

    // Load next process context
    __asm__ volatile("msr sp_el0, %0" :: "r"(to->context.sp_el0));
    write_ttbr0_el1(to->context.ttbr0_el1);
    set_sp(to->context.sp_el1);

    // Memory barriers
    __asm__ volatile("dsb sy; isb");
}
```

---

## Memory Layout

### Stack Memory Regions

```
Higher Addresses
┌─────────────────────────────────────────────────┐
│  KERNEL ADDRESS SPACE (TTBR1_EL1)               │
│  0xFFFFFF80_00000000 - 0xFFFFFFFF_FFFFFFFF      │
│                                                  │
│  ┌──────────────────────────────────┐           │
│  │  Process A Kernel Stack (SP_EL1) │           │
│  │  Used when handling A's exceptions│           │
│  │  Size: 4KB-16KB per process       │           │
│  └──────────────────────────────────┘           │
│                                                  │
│  ┌──────────────────────────────────┐           │
│  │  Process B Kernel Stack (SP_EL1) │           │
│  └──────────────────────────────────┘           │
│                                                  │
│  [Each process has its own kernel stack]        │
│                                                  │
└─────────────────────────────────────────────────┘
                      │
                      │ Context Switch Changes Both
                      │ TTBR0_EL1 and Stack Pointers
                      │
┌─────────────────────────────────────────────────┐
│  USER ADDRESS SPACE (TTBR0_EL1)                 │
│  0x00000000_00000000 - 0x00007FFF_FFFFFFFF      │
│                                                  │
│  ┌──────────────────────────────────┐           │
│  │  Process A User Stack (SP_EL0)   │ ← Top     │
│  │  0x7FFFF000 - 0x80000000          │   (grows │
│  │  Used when running user code      │    down) │
│  └──────────────────────────────────┘           │
│                                                  │
│  [User Code]                                    │
│  [User Data]                                    │
│  [User Heap]                                    │
│                                                  │
└─────────────────────────────────────────────────┘
Lower Addresses

Note: Each process has its own TTBR0_EL1 page table,
      so Process B's user stack is at the same virtual
      address but maps to different physical memory.
```

### Stack Growth Direction

Both stacks grow **downward** (from high addresses to low addresses):

```
Stack Initialization:
┌────────────────┐ ← stack_base + stack_size (SP initialized here)
│                │
│   [unused]     │ ↓ Stack grows down
│                │
│   [frame 1]    │
│   [frame 2]    │
│   [frame 3]    │ ← SP (current stack pointer)
│                │
│   [unused]     │
└────────────────┘ ← stack_base (low address limit)
```

**Stack Pointer Initialization**:
```c
// Allocate 16KB stack
void *stack_base = pmm_alloc_pages(4);  // 4 × 4KB pages

// Initialize SP to top of stack
unsigned long sp = (unsigned long)stack_base + (4 * 4096);

// Ensure 16-byte alignment
sp = sp & ~0xFUL;
```

### Context Frame on Kernel Stack

```
Kernel Stack (SP_EL1) after exception entry:
┌────────────────┐ ← Original SP_EL1 (before exception)
│   x0           │
│   x1           │
│   x2           │
│   x3           │
│   ...          │
│   x29 (FP)     │
│   x30 (LR)     │
│   ELR_EL1      │
│   SPSR_EL1     │
│   SP_EL0       │ ← User stack pointer saved here
│   TTBR0_EL1    │
└────────────────┘ ← SP_EL1 (after saving context)
                    (passed to C handler as argument)
```

---

## SLOPIX Implementation

### Current Status (M5)

- ✓ SPSel set to 1 (using SP_EL1 at EL1)
- ✓ Exception handlers use kernel stack
- ✓ Context switching preserves SP (SP_EL1)
- ✗ No SP_EL0 support (processes run at EL1 only)
- ✗ No dual-stack management

### M6 Implementation Plan

**Phase 1: Add SP_EL0 to Context** (Step 1.1-1.8)
- Add `sp_el0` field to `cpu_context_t` structure
- Rename `sp` to `sp_el1` for clarity
- Initialize both stack pointers for each process

**Phase 4: EL0 Process Creation** (Step 4.1-4.6)
- Allocate user stack in user address space (TTBR0)
- Allocate kernel stack in kernel address space (TTBR1)
- Set `sp_el0` to user stack top
- Set `sp_el1` to kernel stack top

**Phase 5: Exception Handling** (Step 5.1-5.8)
- Add EL0 exception handlers to vector table (+0x400 offset)
- Save SP_EL0 on exception entry: `mrs x0, sp_el0`
- Restore SP_EL0 before ERET: `msr sp_el0, x0`
- Handle syscalls, page faults, interrupts from EL0

**Phase 6: Context Switching** (Step 6.1-6.4)
- Switch TTBR0_EL1 when switching between processes
- Save/restore SP_EL0 as part of context
- Ensure proper stack alignment in all paths

### Key Code Changes

**process.h**:
```c
typedef struct {
    unsigned long x0, x1, x2, ..., x30;
    unsigned long sp_el1;         // Kernel stack pointer
    unsigned long pc;             // ELR_EL1
    unsigned long pstate;         // SPSR_EL1
    unsigned long sp_el0;         // User stack pointer (NEW)
    unsigned long ttbr0_el1;      // Process page table (NEW)
    unsigned char exception_level; // 0=EL0, 1=EL1 (NEW)
} cpu_context_t;
```

**process.c - EL0 Process Creation**:
```c
process_t *process_create_el0(void (*entry)(void), unsigned int stack_size) {
    process_t *proc = malloc(sizeof(process_t));

    // Allocate kernel stack (for handling exceptions)
    void *kernel_stack = pmm_alloc_pages(4);  // 16KB
    proc->context.sp_el1 = (unsigned long)kernel_stack + (4 * 4096);

    // Allocate user stack (for user code)
    void *user_stack = mmu_alloc_user_stack(0x7FFF0000);  // 16KB
    proc->context.sp_el0 = (unsigned long)user_stack + (4 * 4096);

    // Set up initial state
    proc->context.pc = (unsigned long)entry;
    proc->context.pstate = PSTATE_EL0T_IRQ_ENABLED;  // 0x0
    proc->context.exception_level = 0;

    // Create per-process page table
    proc->context.ttbr0_el1 = (unsigned long)mmu_create_process_page_table();

    return proc;
}
```

**exceptions.S - EL0 Exception Handler**:
```assembly
.align 11
exception_vectors:
    // ... Current EL handlers ...

    // Lower EL AArch64 (offset 0x400)
    .align 7
el0_sync_handler:
    // Save registers
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    // ... save x4-x30 ...

    // Save user stack pointer
    mrs x0, sp_el0
    str x0, [sp, #-8]!

    // Save exception state
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #-16]!

    // Call C handler
    mov x0, sp
    bl el0_sync_handler

    // Restore exception state
    ldp x0, x1, [sp], #16
    msr elr_el1, x0
    msr spsr_el1, x1

    // Restore user stack pointer
    ldr x0, [sp], #8
    msr sp_el0, x0           // ← CRITICAL!

    // Restore registers
    ldp x2, x3, [sp], #16
    // ... restore x4-x30 ...
    ldp x0, x1, [sp], #16

    // Return to user mode
    eret
```

### Stack Allocation Strategy

**EL0 Process Requirements**:
1. User stack: 16KB in user address space (0x7FFF0000-0x7FFFFFFF)
2. Kernel stack: 16KB in kernel address space (TTBR1 region)

**EL1 Process Requirements**:
1. Kernel stack: 16KB (no user stack needed)

**Memory Usage**:
- EL0 process: 32KB per process (2 stacks)
- EL1 process: 16KB per process (1 stack)

---

## Common Pitfalls

### Pitfall 1: Forgetting to Restore SP_EL0

**Symptom**: User process crashes immediately after returning from syscall/exception.

**Cause**: ERET does not automatically restore SP_EL0. If you forget to manually restore it, the user process resumes with a corrupted stack pointer.

**Fix**:
```assembly
// Before ERET, ALWAYS restore SP_EL0
ldr x0, [sp, #SP_EL0_OFFSET]
msr sp_el0, x0           // ← Don't forget this!
eret
```

### Pitfall 2: Using SP_EL0 for Kernel Operations

**Symptom**: Kernel crashes when user stack is invalid or full.

**Cause**: SPSel=0 causes kernel to use user stack (SP_EL0), which is not trusted.

**Fix**:
```assembly
// At kernel initialization, set SPSel=1
msr spsel, #1            // Use SP_EL1 at EL1
```

### Pitfall 3: Stack Alignment Violations

**Symptom**: Alignment fault on exception entry/exit.

**Cause**: ARM64 requires SP to be 16-byte aligned at exception boundaries.

**Fix**:
```c
// When initializing stack pointer
unsigned long sp = (unsigned long)stack_base + stack_size;
sp = sp & ~0xFUL;        // Force 16-byte alignment
```

### Pitfall 4: Wrong SPSR_EL1 for EL0

**Symptom**: ERET fails or process takes immediate exception.

**Cause**: SPSR_EL1 bits [3:0] must be 0x0 for EL0t mode, not 0x5 (EL1h).

**Fix**:
```c
// For EL0 processes
proc->context.pstate = 0x0;  // EL0t, interrupts enabled

// NOT this:
// proc->context.pstate = 0x5;  // This is EL1h mode!
```

### Pitfall 5: Not Saving SP_EL0 During Context Switch

**Symptom**: Process A's user stack pointer is lost when switching to Process B.

**Cause**: Forgot to save SP_EL0 to process context.

**Fix**:
```c
// Before switching away from current process
__asm__ volatile("mrs %0, sp_el0" : "=r"(current->context.sp_el0));

// After switching to next process
__asm__ volatile("msr sp_el0, %0" :: "r"(next->context.sp_el0));
```

### Pitfall 6: Shared Kernel Stack Between Processes

**Symptom**: Processes corrupt each other's kernel stack during nested exceptions.

**Cause**: All processes use the same kernel stack pointer.

**Fix**: Each process must have its own dedicated kernel stack.
```c
// In process_create_el0()
void *kernel_stack = pmm_alloc_pages(4);  // Unique per process
proc->context.sp_el1 = (unsigned long)kernel_stack + (4 * 4096);
```

### Pitfall 7: Reading SP Instead of SP_EL0

**Symptom**: Saved stack pointer is SP_EL1, not the user's SP_EL0.

**Cause**: Using `mov x0, sp` instead of `mrs x0, sp_el0`.

**Fix**:
```assembly
// WRONG:
mov x0, sp               // Reads SP_EL1 (kernel stack)

// CORRECT:
mrs x0, sp_el0           // Reads SP_EL0 (user stack)
```

### Pitfall 8: Missing Memory Barriers After TTBR0 Switch

**Symptom**: Stale TLB entries cause wrong memory accesses after process switch.

**Cause**: MMU doesn't see new page table without proper barriers.

**Fix**:
```assembly
// After changing TTBR0_EL1
msr ttbr0_el1, x0
dsb sy                   // Data Synchronization Barrier
isb                      // Instruction Synchronization Barrier
```

---

## References

### ARM Architecture Reference Manual

- [Learn the architecture - AArch64 Exception Model](https://developer.arm.com/documentation/102412/0102/Privilege-and-Exception-levels)
- [SPSel: Stack Pointer Select](https://developer.arm.com/documentation/ddi0601/latest/AArch64-Registers/SPSel--Stack-Pointer-Select)
- [ERET: Exception Return](https://developer.arm.com/documentation/ddi0602/latest/Base-Instructions/ERET--Exception-Return-)
- [SP_EL0: Stack Pointer (EL0)](https://developer.arm.com/documentation/ddi0601/latest/AArch64-Registers/SP-EL0--Stack-Pointer--EL0-)

### Technical Articles

- [ARMv-8a EL0<->EL1 Switching](https://pyjamacafe.com/posts/arm64-day1-el0-el1-switching/)
- [AArch64 Exception Levels](https://krinkinmu.github.io/2021/01/04/aarch64-exception-levels.html)
- [ARM64 System calls](https://duetorun.com/blog/20230604/a64-svc/)
- [Linux ARM64: Introduce IRQ stack](https://lwn.net/Articles/657969/)

### SLOPIX Internal Documentation

- [arm64-userspace-el0.md](./arm64-userspace-el0.md) - Complete EL0 implementation guide
- [arm64-registers.md](./arm64-registers.md) - System register specifications
- [M6-ROADMAP.md](./M6-ROADMAP.md) - Implementation phases and steps
- [process.h](../process.h) - Process structure with cpu_context_t
- [exceptions.S](../exceptions.S) - Exception handlers and vector table

### Code Examples

- [ARM Trusted Firmware context.S](https://github.com/ARM-software/arm-trusted-firmware/blob/master/lib/el3_runtime/aarch64/context.S) - Reference context switching implementation
- [xv6-aarch64](https://github.com/k-mrm/xv6-aarch64) - Educational OS with EL0 support

---

## Summary

ARM64's dual-stack architecture provides:

1. **Security**: Kernel operations use trusted stack, immune to user manipulation
2. **Isolation**: Each process has separate user and kernel stacks
3. **Efficiency**: Hardware automatically switches to kernel stack on exception entry
4. **Clarity**: Explicit stack pointers (SP_EL0, SP_EL1) make code easier to understand

**Key Takeaways**:

- **SP_EL0**: User stack pointer, used at EL0, must be manually saved/restored
- **SP_EL1**: Kernel stack pointer, used at EL1, automatically selected when SPSel=1
- **SPSel Register**: Controls stack pointer selection (always use SPSel=1 for kernel)
- **ERET Gotcha**: Does NOT restore SP_EL0 automatically - must use `msr sp_el0, x0`
- **Each EL0 Process**: Requires TWO stacks (user + kernel)
- **Stack Alignment**: Both stacks must be 16-byte aligned

**Next Steps for SLOPIX M6**:

1. Add `sp_el0` and `sp_el1` fields to `cpu_context_t`
2. Allocate dual stacks for EL0 processes
3. Implement SP_EL0 save/restore in exception handlers
4. Test context switching between EL0 processes
5. Verify stack alignment in all code paths

For implementation details, see [M6-ROADMAP.md](./M6-ROADMAP.md) Phase 1 and Phase 6.

---

**Document Version**: 1.0
**Last Updated**: 2026-01-11
**Maintainer**: SLOPIX Team
