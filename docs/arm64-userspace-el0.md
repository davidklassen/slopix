# ARM64 Userspace (EL0) Implementation Guide for SLOPIX

**Target Architecture**: ARMv8-A AArch64
**Document Purpose**: Complete reference for implementing EL0 userspace in SLOPIX
**Date**: 2026-01-11

---

## Table of Contents

1. [Exception Levels Overview](#1-exception-levels-overview)
2. [System Call Mechanism](#2-system-call-mechanism)
3. [AAPCS64 Calling Convention for Syscalls](#3-aapcs64-calling-convention-for-syscalls)
4. [Memory Protection and Page Table Permissions](#4-memory-protection-and-page-table-permissions)
5. [Context Switching Between EL0 and EL1](#5-context-switching-between-el0-and-el1)
6. [ERET Instruction](#6-eret-instruction)
7. [Critical ARM64 Userspace Gotchas](#7-critical-arm64-userspace-gotchas)
8. [Implementation Checklist](#8-implementation-checklist)
9. [References](#9-references)

---

## 1. Exception Levels Overview

### 1.1 Exception Level Hierarchy

ARMv8 architecture defines four Exception Levels (EL0-EL3) where EL0 < EL1 < EL2 < EL3 in terms of privilege:

- **EL0**: Unprivileged - Normal application code (userspace)
- **EL1**: Privileged - Operating system kernel
- **EL2**: Virtualization - Hypervisor
- **EL3**: Secure Monitor - Secure firmware

**Note**: Support for EL0 and EL1 are mandatory for all ARM64 implementations.

### 1.2 EL0 Restrictions

Code executing at EL0 has the following restrictions:

1. **No system register access** - Cannot read or write most system registers
2. **No hardware access** - Cannot directly access hardware peripherals
3. **No MMU configuration** - Cannot modify page tables or MMU settings
4. **No exception vector control** - Cannot modify VBAR_EL1
5. **Limited visibility** - Cannot read CPU ID registers or feature registers

**Key Principle**: EL0 code must use system calls (via SVC instruction) to request any privileged operations from EL1.

### 1.3 EL1 Privileges

EL1 (kernel) has extensive privileges:

1. **System register access** - Can read/write most EL1 system registers
2. **MMU control** - Full control over TTBR0_EL1, TTBR1_EL1, TCR_EL1, MAIR_EL1
3. **Interrupt management** - Can configure GIC, enable/disable interrupts
4. **Exception handling** - Controls exception vector table via VBAR_EL1
5. **Memory management** - Sets up and modifies page tables

### 1.4 Exception Level Transitions

**Critical Rule**:
- Processor can only move to **higher** exception level when an **exception occurs**
- Processor can only move to **lower** exception level by executing **exception return** (ERET)

**Transition Mechanisms**:
- EL0 → EL1: SVC instruction, IRQ/FIQ, synchronous exceptions (data abort, instruction abort)
- EL1 → EL0: ERET instruction after handling exception

### 1.5 Memory Access Permissions

Memory accesses are checked differently based on exception level:

- **EL0 accesses**: Checked against **unprivileged** access permissions (AP[1]=1 required)
- **EL1/EL2/EL3 accesses**: Checked against **privileged** access permissions

This separation allows kernel to set up memory regions that:
- Kernel can access but user cannot (AP[2:1] = 00)
- Both can access (AP[2:1] = 01 or 11)

---

## 2. System Call Mechanism

### 2.1 SVC Instruction

The **SVC (Supervisor Call)** instruction is the gateway from userspace to kernel:

```assembly
svc #0      // Triggers synchronous exception to EL1
```

**What happens automatically when SVC executes**:

1. **Exception level changes**: Processor switches from EL0 to EL1
2. **Save return address**: PC is saved to ELR_EL1 (Exception Link Register)
3. **Save processor state**: PSTATE is saved to SPSR_EL1 (Saved Program Status Register)
4. **Jump to handler**: PC is set to VBAR_EL1 + exception_offset
5. **ESR_EL1 populated**: Exception Syndrome Register records the exception details

**Important**: The SVC instruction does NOT save general-purpose registers (x0-x30). The exception handler must save these explicitly.

### 2.2 ESR_EL1 - Exception Syndrome Register

The ESR_EL1 register identifies the cause of the exception:

```
 63    32 31  26 25 24    0
+--------+------+--+-------+
| RES0   |  EC  |IL|  ISS  |
+--------+------+--+-------+
         Exception Instruction Instruction
         Class     Length    Specific
                              Syndrome
```

**Key Fields**:
- **EC [31:26]**: Exception Class
  - `0x15`: SVC instruction executed from AArch64 state
  - `0x20`: Instruction Abort from lower EL
  - `0x24`: Data Abort from lower EL
- **ISS [24:0]**: Instruction Specific Syndrome
  - For SVC: ISS[15:0] holds the immediate argument from SVC instruction
  - Can be used for syscall number (Linux uses x8 register instead)

**Reading ESR_EL1 in exception handler**:

```c
unsigned long esr;
__asm__ volatile("mrs %0, esr_el1" : "=r"(esr));

unsigned int ec = (esr >> 26) & 0x3F;  // Extract Exception Class
unsigned int iss = esr & 0xFFFFFF;     // Extract ISS

if (ec == 0x15) {
    // This is a SVC from AArch64
    handle_syscall();
}
```

### 2.3 Exception Vector Table Layout

VBAR_EL1 points to a 2KB-aligned exception vector table with 16 entries:

```
Offset    Exception Type                        Description
------    --------------                        -----------
+0x000    Synchronous from Current EL (SP_EL0)  Not typically used
+0x080    IRQ/vIRQ from Current EL (SP_EL0)
+0x100    FIQ/vFIQ from Current EL (SP_EL0)
+0x180    SError/vSError from Current EL (SP_EL0)

+0x200    Synchronous from Current EL (SP_ELx)  Kernel exceptions
+0x280    IRQ/vIRQ from Current EL (SP_ELx)     Kernel interrupts
+0x300    FIQ/vFIQ from Current EL (SP_ELx)
+0x380    SError/vSError from Current EL (SP_ELx)

+0x400    Synchronous from Lower EL (AArch64)   ** USER SYSCALLS **
+0x480    IRQ/vIRQ from Lower EL (AArch64)      User interrupts
+0x500    FIQ/vFIQ from Lower EL (AArch64)
+0x580    SError/vSError from Lower EL (AArch64)

+0x600    Synchronous from Lower EL (AArch32)   32-bit compatibility
+0x680    IRQ/vIRQ from Lower EL (AArch32)
+0x700    FIQ/vFIQ from Lower EL (AArch32)
+0x780    SError/vSError from Lower EL (AArch32)
```

**For EL0 syscalls**: The handler at **VBAR_EL1 + 0x400** is invoked.

**SLOPIX Current Implementation**: The exception handler at offset 0x400 currently jumps to the same `exception_handler_sync` as kernel exceptions. This needs enhancement to distinguish userspace syscalls.

### 2.4 Exception Handler Flow

**Full syscall flow**:

1. **Userspace** executes `svc #0` at EL0
2. **Hardware** automatically:
   - Saves PC to ELR_EL1
   - Saves PSTATE to SPSR_EL1
   - Sets ESR_EL1 with EC=0x15
   - Switches to EL1
   - Jumps to VBAR_EL1 + 0x400
3. **Exception handler** (assembly):
   - Saves all general-purpose registers to kernel stack
   - Saves x0, x1, ELR_EL1, SPSR_EL1 to stack
   - Calls C handler with stack pointer as argument
4. **C handler** (`handle_syscall_from_el0`):
   - Reads ESR_EL1 to confirm SVC
   - Reads x8 for syscall number
   - Reads x0-x7 for arguments
   - Dispatches to syscall implementation
   - Places return value in x0
5. **Return path** (assembly):
   - Restores all registers from stack
   - Restores ELR_EL1 and SPSR_EL1
   - Executes ERET to return to EL0

---

## 3. AAPCS64 Calling Convention for Syscalls

### 3.1 Standard AAPCS64 Overview

The ARM Architecture Procedure Call Standard (AAPCS64) defines register usage:

**Parameter Registers**:
- x0-x7: Used to pass arguments to functions
- x8: Indirect result location register (for large return values)

**Return Value**:
- x0: Primary return value (64-bit or pointer)
- x1: Secondary return value (for 128-bit returns)

**Callee-Saved Registers**:
- x19-x28: Must be preserved across function calls
- x29: Frame pointer (FP)
- x30: Link register (LR)

**Caller-Saved Registers**:
- x0-x18: May be clobbered by called function

### 3.2 Linux ARM64 Syscall Convention

Linux ARM64 uses a modified convention for syscalls:

**Syscall Number**: x8 register holds the syscall number
**Arguments**: x0-x7 hold up to 8 arguments
**Return Value**: x0 contains the return code
**Invocation**: `svc #0` (immediate value ignored)

**Example Syscall** (write syscall):

```assembly
mov x8, #64         // Syscall number for write
mov x0, #1          // fd = stdout
ldr x1, =message    // buf = pointer to message
mov x2, #13         // count = 13 bytes
svc #0              // Execute syscall
// x0 now contains return value (bytes written or -errno)
```

### 3.3 Register Preservation

**Critical Rule**: The kernel syscall handler must preserve all registers that user code expects to be preserved.

**Registers to save on entry**:
- x0-x30: All general-purpose registers
- SP (SP_EL0): User stack pointer
- ELR_EL1: User program counter
- SPSR_EL1: User processor state

**Registers to modify**:
- x0: Set to syscall return value
- SP_EL1: Kernel uses its own stack (not user stack)

**Registers preserved on return**:
- x1-x30: Restored to original values (unless syscall modifies them by design)
- SP_EL0: Restored
- ELR_EL1: Points to instruction after SVC (return address)
- SPSR_EL1: Restored to user state

### 3.4 Stack Alignment

**Critical Requirement**: ARM64 requires SP to be 16-byte aligned when:
- Taking exceptions
- Returning from exceptions
- Calling functions

**SLOPIX Current Implementation** (from `process.c`):

```c
// Context frame is 264 bytes (33 * 8), offset 8 modulo 16
// Initialize SP with offset 8 so after scheduler subtracts 264,
// SP becomes 16-byte aligned
proc->context.sp_el1 = (((unsigned long)stack + stack_size - 8) & ~0xFUL) + 8;
```

This ensures proper alignment for exception entry/exit.

---

## 4. Memory Protection and Page Table Permissions

### 4.1 Page Table Entry Permissions

ARM64 uses several permission bits in page table descriptors to enforce memory protection:

**Key Permission Bits** (from ARM64 page table descriptor):

```
 54   53   52   10   7   6   5
+----+----+----+----+----+----+
| UXN| PXN| Cont| AF | AP[2:1] | NS |
+----+----+----+----+----+----+
```

### 4.2 Access Permission (AP[2:1])

The AP field controls read/write access at different privilege levels:

| AP[2:1] | EL1 Access | EL0 Access | Use Case                    |
|---------|------------|------------|-----------------------------|
| 00      | RW         | None       | Kernel-only data            |
| 01      | RW         | RW         | User read-write data        |
| 10      | RO         | None       | Kernel read-only data       |
| 11      | RO         | RO         | Shared read-only data       |

**SLOPIX Definitions** (from `memory.h`):

```c
#define PTE_KERNEL  (0UL << 6)   // AP[2:1] = 00 (kernel RW, user no access)
#define PTE_USER    (1UL << 6)   // AP[2:1] = 01 (kernel RW, user RW)
#define PTE_RO      (2UL << 6)   // AP[2:1] = 10 (kernel RO, user no access)
```

**For userspace pages**: Use `PTE_USER` to allow EL0 access.

### 4.3 Execute-Never Bits (UXN and PXN)

These bits control code execution permissions:

**UXN (Unprivileged Execute Never) - Bit 54**:
- 0: EL0 can execute code from this page
- 1: EL0 cannot execute code from this page
- Only applies to EL0 accesses

**PXN (Privileged Execute Never) - Bit 53**:
- 0: EL1+ can execute code from this page
- 1: EL1+ cannot execute code from this page
- Only applies to EL1/EL2/EL3 accesses

**Security Best Practices**:

1. **User data pages**: Set UXN=1 to prevent execution (W^X policy)
2. **User code pages**: Set UXN=0 and PXN=1 to prevent kernel from executing user code (mitigates ret2usr attacks)
3. **Kernel code pages**: Set PXN=0, UXN=1
4. **Kernel data pages**: Set PXN=1, UXN=1

**Linux Implementation**:
Linux sets the PXN bit on every user-space page table entry, ensuring that even if the kernel is tricked into jumping to a user address, the CPU will fault.

### 4.4 Hierarchical Permissions

Page table descriptors at upper levels can enforce permissions across entire ranges:

**UXNTable and PXNTable bits** in table descriptors:
- If UXNTable=1 in a table descriptor, all subsequent pages are treated as UXN=1
- If PXNTable=1 in a table descriptor, all subsequent pages are treated as PXN=1

This allows efficient permission enforcement without setting bits on every leaf entry.

### 4.5 Separate User and Kernel Address Spaces

**TTBR0_EL1 and TTBR1_EL1**:

ARM64 provides two translation base registers:

- **TTBR0_EL1**: User address space (typically 0x0000_0000_0000_0000 to 0x0000_FFFF_FFFF_FFFF)
- **TTBR1_EL1**: Kernel address space (typically 0xFFFF_0000_0000_0000 to 0xFFFF_FFFF_FFFF_FFFF)

**Advantages**:
1. User and kernel page tables are completely separate
2. Switching processes only requires changing TTBR0_EL1 (TTBR1_EL1 remains constant)
3. Kernel mappings are always available at EL1 without TLB flushes
4. Provides isolation between user and kernel memory

**SLOPIX Current State**: Currently uses TTBR1_EL1 for higher-half kernel (0xFFFF_FF80_4000_0000+). TTBR0_EL1 is set up but needs per-process configuration for userspace.

### 4.6 Separate User and Kernel Stacks

**Critical Requirement**: User processes and kernel must use separate stacks.

**Stack Pointers**:
- **SP_EL0**: User stack pointer (used when executing at EL0)
- **SP_EL1**: Kernel stack pointer (used when executing at EL1)

**SPSel Register**: Controls which stack pointer is active at EL1:
- SPSel.SP = 0: Use SP_EL0 at EL1 (share stack with user - NOT recommended)
- SPSel.SP = 1: Use SP_EL1 at EL1 (dedicated kernel stack - CORRECT)

**On exception from EL0**:
1. Hardware does NOT automatically switch stack pointer
2. Exception handler must save SP_EL0 and switch to kernel stack
3. All exception handling executes on kernel stack (SP_EL1)

**On return to EL0**:
1. Restore SP_EL0 from saved context
2. ERET automatically uses SP_EL0 when returning to EL0

**Security Rationale**:
- User stack is in user-controlled memory - cannot be trusted
- Kernel must use its own stack to prevent user code from corrupting kernel execution
- Separate stacks prevent stack overflow from one mode affecting the other

---

## 5. Context Switching Between EL0 and EL1

### 5.1 Key System Registers

**ELR_EL1 (Exception Link Register)**:
- Holds the return address when exception is taken to EL1
- Automatically set by hardware on exception entry
- Must be manually restored before ERET

**SPSR_EL1 (Saved Program Status Register)**:
- Holds the processor state (PSTATE) from before the exception
- Automatically set by hardware on exception entry
- Must be manually restored before ERET

**PSTATE** includes:
- Exception level (bits [3:2])
- Stack pointer selection (bit [0])
- Interrupt masks (DAIF bits)
- Condition flags (NZCV)

### 5.2 Exception Entry (EL0 → EL1)

**Hardware automatically**:

1. Sets ELR_EL1 = PC (return address, points to instruction after SVC)
2. Sets SPSR_EL1 = PSTATE (saves processor state)
3. Sets ESR_EL1 = exception syndrome
4. Changes exception level to EL1
5. Sets PC = VBAR_EL1 + offset (jumps to handler)
6. Masks interrupts (sets DAIF)

**Handler must manually**:

1. Save all general-purpose registers (x0-x30)
2. Save SP_EL0 (user stack pointer)
3. Switch to kernel stack (SP_EL1)
4. Handle the exception
5. Restore all registers
6. Execute ERET

**SLOPIX Implementation** (from `exceptions.S`):

```assembly
exception_handler_sync:
    // Save x0, x1 first
    stp x0, x1, [sp, #-16]!

    // Save ELR_EL1, SPSR_EL1
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #-16]!

    // Save remaining registers x2-x30
    stp x29, x30, [sp, #-16]!
    // ... (save x2-x28)

    // Call C handler with stack pointer
    mov x0, sp
    bl handle_sync_exception_with_context

    // Restore all registers
    // ... (restore in reverse order)

    // Restore ELR_EL1, SPSR_EL1
    ldp x0, x1, [sp], #16
    msr elr_el1, x0
    msr spsr_el1, x1

    // Restore x0, x1
    ldp x0, x1, [sp], #16

    eret
```

### 5.3 Context Frame Layout

**SLOPIX Context Frame** (264 bytes total, 33 quad-words):

```
Stack Layout (growing downward):
+----------------+ <- SP on entry (highest address)
| x0             | +0
| x1             | +8
| ELR_EL1        | +16
| SPSR_EL1       | +24
| x29 (FP)       | +32
| x30 (LR)       | +40
| x27            | +48
| x28            | +56
| ... (x25-x2)   |
| xzr (padding)  | +256
+----------------+ <- SP after save (lowest address)
```

**C Structure** (from `process.h`):

```c
typedef struct {
    unsigned long x0, x1, x2, x3, x4, x5, x6, x7;
    unsigned long x8, x9, x10, x11, x12, x13, x14, x15;
    unsigned long x16, x17, x18, x19, x20, x21, x22, x23;
    unsigned long x24, x25, x26, x27, x28, x29, x30;
    unsigned long sp;       // Stack pointer (SP_EL0)
    unsigned long pc;       // Program counter (ELR_EL1)
    unsigned long pstate;   // Processor state (SPSR_EL1)
} cpu_context_t;
```

### 5.4 Stack Pointer Selection (SPSel)

**SPSel Register** determines which stack pointer is used:

- **SPSel.SP = 0**: Use SP_EL0 at all exception levels
- **SPSel.SP = 1**: Use SP_ELx (dedicated stack for each EL)

**Setting SPSel**:

```assembly
msr spsel, #1       // Use SP_EL1 at EL1 (recommended)
```

**Exception Modes**:

- **EL1h** (handler mode): EL1 with SPSel=1 (dedicated SP_EL1 stack)
- **EL1t** (thread mode): EL1 with SPSel=0 (shared SP_EL0 stack - rarely used)

**Best Practice**: Always use EL1h mode (SPSel=1) for kernel to ensure separate kernel stack.

### 5.5 Process Context Switch

**Full context switch** when switching between processes at EL0:

1. **Save current process state**:
   - Exception handler already saved registers to current process's kernel stack
   - Copy context from stack to `current_process->context`
   - Save SP_EL0, ELR_EL1, SPSR_EL1

2. **Switch address space**:
   - Update TTBR0_EL1 to new process's page table
   - Invalidate TLB for old ASID (or flush all)
   - Execute `tlbi` instruction

3. **Load next process state**:
   - Load `next_process->context` to kernel stack
   - Set SP_EL1 to next process's kernel stack
   - Restore SP_EL0, ELR_EL1, SPSR_EL1

4. **Return to userspace**:
   - Restore all general-purpose registers from context
   - Execute ERET (returns to next process at EL0)

---

## 6. ERET Instruction

### 6.1 ERET Overview

**ERET (Exception Return)** is the only way to return from an exception and lower the exception level.

**Syntax**:
```assembly
eret
```

**What ERET does atomically**:

1. **Restore PC**: PC ← ELR_ELn (where n is current EL)
2. **Restore PSTATE**: PSTATE ← SPSR_ELn
3. **Lower Exception Level**: If SPSR_ELn indicates lower EL, drops to that EL
4. **Resume execution**: Starts executing at restored PC with restored state

### 6.2 ERET Behavior Details

**Target Exception Level**: Determined by SPSR_ELn bits [3:2]:
- 0b00: EL0t (user mode)
- 0b01: EL1t (kernel thread mode)
- 0b10: EL1h (kernel handler mode)
- 0b11: Reserved

**Stack Pointer**: Determined by SPSR_ELn bit [0]:
- 0: Use SP_EL0
- 1: Use SP_ELx

**Interrupt State**: Restored from SPSR_ELn DAIF bits:
- D: Debug exceptions mask
- A: SError mask
- I: IRQ mask
- F: FIQ mask

### 6.3 ERET Preconditions

Before executing ERET, ensure:

1. **ELR_ELn is valid**: Points to valid instruction in target address space
2. **SPSR_ELn is valid**: Contains valid PSTATE value for target EL
3. **Registers restored**: All general-purpose registers are restored
4. **Stack pointer**: SP is at correct position after restoring registers
5. **Memory barriers**: DSB/ISB executed if page tables were modified

**Invalid ERET causes**: Exception return with an invalid PSTATE value is unpredictable.

### 6.4 Example: Returning to EL0

```assembly
// Exception handler has saved context and handled the exception
// Now returning to user process at EL0

    // Restore general-purpose registers from stack
    ldp x2, xzr, [sp], #16
    ldp x3, x4, [sp], #16
    // ... (restore x5-x30)

    // Restore ELR_EL1 (user PC) and SPSR_EL1 (user PSTATE)
    ldp x0, x1, [sp], #16
    msr elr_el1, x0     // Return address in user code
    msr spsr_el1, x1    // PSTATE with EL=0, SP=0 (use SP_EL0)

    // Restore x0, x1
    ldp x0, x1, [sp], #16

    // Return to EL0
    eret                // PC ← ELR_EL1, PSTATE ← SPSR_EL1, drop to EL0
```

After ERET:
- Processor is at EL0
- Executing at address from ELR_EL1
- Using SP_EL0 as stack pointer
- Registers have user values
- Interrupts enabled/disabled per SPSR_EL1

### 6.5 Creating Initial User Process

To start a user process, set up context for ERET:

```c
// Set up initial user process context
process->context.pc = (unsigned long)user_entry_point;  // ELR_EL1
process->context.sp_el0 = (unsigned long)user_stack_top; // SP_EL0

// PSTATE for EL0: EL=0, SP=0, interrupts enabled
// Bits: [3:2]=00 (EL0), [0]=0 (use SP_EL0), DAIF=0000 (interrupts enabled)
process->context.pstate = 0x0;  // EL0t with interrupts enabled

// Initialize argument registers
process->context.x0 = argc;
process->context.x1 = (unsigned long)argv;
// x2-x30 = 0
```

Scheduler restores this context and executes ERET, starting the user process at EL0.

---

## 7. Critical ARM64 Userspace Gotchas

### 7.1 Register Preservation Failures

**Issue**: SVC instruction does NOT save general-purpose registers.

**Symptom**: User register values corrupted after syscall.

**Fix**: Exception handler MUST save all registers (x0-x30) explicitly:

```assembly
exception_handler_sync:
    stp x0, x1, [sp, #-16]!
    // Save ALL registers x0-x30
    // Only then safe to use registers
```

**SLOPIX Status**: Current implementation correctly saves all registers.

### 7.2 Stack Alignment Violations

**Issue**: ARM64 requires 16-byte stack alignment on exception entry/exit.

**Symptom**: Alignment fault when taking exception.

**Rule**: SP must be 16-byte aligned when:
- Taking exceptions
- Returning from exceptions (before ERET)
- Calling functions (AAPCS64 requirement)

**Fix**: Ensure initial SP setup accounts for context frame size:

```c
// Context frame is 264 bytes (not 16-byte aligned)
// Initialize SP with offset 8 so after pushing 264 bytes,
// SP becomes 16-byte aligned
proc->context.sp_el1 = (((unsigned long)stack + stack_size - 8) & ~0xFUL) + 8;
```

**SLOPIX Status**: Current implementation handles this correctly.

### 7.3 Forgetting Separate Kernel Stack

**Issue**: Using user stack (SP_EL0) for kernel exception handling.

**Symptom**: Kernel crashes when user stack is invalid or full.

**Security Risk**: User can control kernel stack, leading to privilege escalation.

**Fix**: Always switch to kernel stack on exception entry:

```assembly
exception_handler_sync:
    // At entry, still using SP_EL1 (kernel stack) because SPSel=1
    // SP_EL0 contains user stack - DO NOT USE IT
    // Save context to SP_EL1 (kernel stack)
    stp x0, x1, [sp, #-16]!  // sp = SP_EL1
```

**SLOPIX Status**: Uses SPSel=1, so exceptions use SP_EL1 automatically. Need to ensure each process has dedicated kernel stack.

### 7.4 Incorrect SPSR_EL1 Setup

**Issue**: SPSR_EL1 contains invalid PSTATE value for target exception level.

**Symptom**: ERET fails, system hangs, or takes immediate exception.

**Critical Fields in PSTATE/SPSR_EL1**:
- Bits [3:2]: Exception level (must be ≤ current EL)
- Bit [0]: Stack pointer select
- Bits [9:6]: DAIF interrupt masks

**Fix**: Set SPSR_EL1 correctly for EL0:

```c
// For EL0 with interrupts enabled
#define PSTATE_EL0_IRQ_ENABLED 0x0    // EL=0, SP=0, DAIF=0

process->context.pstate = PSTATE_EL0_IRQ_ENABLED;
```

**Common Mistake**: Leaving bits set from previous PSTATE that are invalid for EL0.

### 7.5 Missing Memory Barriers

**Issue**: Page table updates not visible to MMU after modification.

**Symptom**: TLB contains stale entries, causing translation faults or data corruption.

**Fix**: Use barriers after modifying page tables:

```c
// After updating page tables
__asm__ volatile("dsb ishst");  // Data Synchronization Barrier
__asm__ volatile("tlbi vmalle1"); // Invalidate TLB
__asm__ volatile("dsb ish");    // Ensure TLB invalidation completes
__asm__ volatile("isb");        // Instruction Synchronization Barrier
```

**SLOPIX Status**: Uses `dsb sy` after page table initialization.

### 7.6 Not Setting PXN on User Pages

**Issue**: Kernel can execute code from user pages.

**Security Risk**: ret2usr attack - attacker tricks kernel into executing user-controlled code with kernel privileges.

**Fix**: Set PXN (bit 53) on all user pages:

```c
#define PTE_USER_CODE  (PTE_VALID | PTE_USER | PTE_UXN_CLEAR | PTE_PXN_SET)
```

This ensures even if kernel jumps to user address, CPU will fault.

**SLOPIX Status**: Current PTE_USER definition does not set PXN - needs addition.

### 7.7 Forgetting to Map User Stack

**Issue**: User process accesses unmapped stack memory.

**Symptom**: Data abort from EL0 when accessing stack.

**Fix**: Allocate and map user stack in TTBR0_EL1 with user permissions:

```c
// Allocate user stack pages
void *user_stack = allocate_user_pages(STACK_PAGES);

// Map with user permissions
map_user_pages(user_stack, STACK_PAGES, PTE_USER | PTE_UXN);

// Set SP_EL0 to top of stack
process->context.sp_el0 = (unsigned long)user_stack + STACK_SIZE;
```

**SLOPIX Status**: Not yet implemented - current processes run at EL1.

### 7.8 Exception Vector Offset Confusion

**Issue**: Using wrong vector table offset for EL0 exceptions.

**Symptom**: Wrong exception handler executes, system hangs.

**Offset for EL0 exceptions**:
- Synchronous (SVC): VBAR_EL1 + **0x400**
- IRQ: VBAR_EL1 + **0x480**
- FIQ: VBAR_EL1 + **0x500**

**Fix**: Ensure vector table has handlers at correct offsets:

```assembly
.align 11                      // 2KB alignment
exception_vector_table:
    // ... Current EL entries at +0x000 to +0x3FF

    .align 7                   // 128-byte alignment
    // +0x400: Synchronous from Lower EL
    b exception_handler_el0_sync

    .align 7
    // +0x480: IRQ from Lower EL
    b exception_handler_el0_irq
```

**SLOPIX Status**: Vector table exists but all offsets jump to same handler - needs separation for EL0 handlers.

### 7.9 Validating User Pointers

**Issue**: User passes invalid pointer in syscall argument.

**Security Risk**: Kernel dereferences invalid pointer, crashes or leaks kernel data.

**Fix**: Validate all user pointers before dereferencing:

```c
bool is_user_address(void *addr) {
    unsigned long va = (unsigned long)addr;
    // Check if address is in user space (TTBR0 range)
    return va < 0x0000800000000000UL;  // 39-bit user VA limit
}

ssize_t sys_write(int fd, const char *buf, size_t count) {
    if (!is_user_address(buf)) {
        return -EFAULT;  // Invalid user pointer
    }
    // Safe to copy from user buffer
    // ...
}
```

**SLOPIX Status**: Not yet implemented - needed for userspace.

### 7.10 Interrupt State on ERET

**Issue**: Returning to EL0 with interrupts disabled.

**Symptom**: User process runs with interrupts masked, scheduler never preempts it.

**Fix**: Ensure SPSR_EL1 has interrupts enabled (DAIF bits clear):

```c
#define PSTATE_DAIF_CLEAR  0x0    // D=0, A=0, I=0, F=0 (all unmasked)
#define PSTATE_EL0_DEFAULT (PSTATE_DAIF_CLEAR)

process->context.pstate = PSTATE_EL0_DEFAULT;
```

**SLOPIX Status**: Current PSTATE_EL1H_IRQ_ENABLED is 0x5, which is EL1h mode. For EL0, use 0x0.

---

## 8. Implementation Checklist

### Phase 1: Exception Infrastructure

- [x] Exception vector table installed (VBAR_EL1)
- [x] Exception handlers save full context
- [x] Handlers distinguish exception sources via ESR_EL1
- [ ] Separate handlers for EL0 exceptions (offset +0x400)
- [ ] Extract syscall number from x8 register
- [ ] Dispatch syscalls to implementations

### Phase 2: Memory Management

- [x] TTBR1_EL1 set up for higher-half kernel
- [x] TTBR0_EL1 allocated and initialized
- [ ] Per-process TTBR0_EL1 page tables
- [ ] User page permissions (PTE_USER)
- [ ] UXN bit set on user data pages
- [ ] PXN bit set on user code pages
- [ ] Copy-on-write for user pages
- [ ] Demand paging for user heap/stack

### Phase 3: Process Management

- [x] Process structure with cpu_context_t
- [x] Stack allocation for kernel processes
- [ ] Separate kernel stack per process (for syscall handling)
- [ ] User stack allocation and mapping
- [ ] Set SPSR_EL1 to EL0 mode (0x0)
- [ ] Set ELR_EL1 to user entry point
- [ ] Set SP_EL0 to user stack top

### Phase 4: System Calls

- [ ] Syscall number definitions
- [ ] Syscall dispatcher (reads x8, calls implementation)
- [ ] Basic syscalls: exit, write, read, brk
- [ ] User pointer validation
- [ ] Copy to/from user space functions
- [ ] Error handling (return -errno in x0)

### Phase 5: User Program Loading

- [ ] ELF loader (parse headers, load segments)
- [ ] Map user code sections (executable, read-only)
- [ ] Map user data sections (read-write, non-executable)
- [ ] Set up user stack (read-write, non-executable)
- [ ] Set up user heap (managed via brk/mmap)
- [ ] Initialize arguments (argc/argv in x0/x1)

### Phase 6: Context Switching

- [x] Save/restore general-purpose registers
- [x] Save/restore ELR_EL1, SPSR_EL1
- [ ] Switch TTBR0_EL1 on process switch
- [ ] Invalidate TLB for old process
- [ ] Handle ASID (Address Space ID) for TLB efficiency

### Phase 7: Testing

- [ ] Test syscall from EL0 (SVC instruction)
- [ ] Test data access from user space
- [ ] Test code execution from user space
- [ ] Test kernel protection (user can't write kernel)
- [ ] Test stack isolation (user stack vs kernel stack)
- [ ] Test context switch between user processes
- [ ] Test invalid user pointers (should return -EFAULT)

---

## 9. References

### ARM Architecture Reference Manual

- [Learn the architecture - AArch64 Exception Model](https://developer.arm.com/documentation/102412/0102/Privilege-and-Exception-levels)
- [ESR_EL1: Exception Syndrome Register](https://developer.arm.com/documentation/ddi0601/latest/AArch64-Registers/ESR-EL1--Exception-Syndrome-Register--EL1-)
- [SPSel: Stack Pointer Select](https://developer.arm.com/documentation/ddi0601/latest/AArch64-Registers/SPSel--Stack-Pointer-Select)

### Technical Articles

- [AArch64 Exception Levels (Medium)](https://medium.com/@om.nara/aarch64-exception-levels-60d3a74280e6)
- [ARMv-8a EL0<->EL1 Switching](https://pyjamacafe.com/posts/arm64-day1-el0-el1-switching/)
- [ARM64 System calls](https://duetorun.com/blog/20230604/a64-svc/)
- [Anatomy of Linux system call in ARM64](https://eastrivervillage.com/blog/2018/06/11/anatomy-of-linux-system-call-in-arm64/)
- [AAPCS64 Procedure Call Standard (Medium)](https://medium.com/@tunacici7/aarch64-procedure-call-standard-aapcs64-abi-calling-conventions-machine-registers-a2c762540278)

### Memory Protection

- [ARM64 Normal Memory Attributes (Medium)](https://medium.com/@om.nara/arm64-normal-memory-attributes-6086012fa0e3)
- [Emerging Defense in Android Kernel - PXN/UXN](https://keenlab.tencent.com/en/2016/06/01/Emerging-Defense-in-Android-Kernel/)
- [AArch64 MMU Programming](https://lowenware.com/blog/aarch64-mmu-programming/)
- [ARM Documentation - Permissions attributes](https://developer.arm.com/documentation/102376/0100/Permissions-attributes)

### Context Switching

- [AArch64 Exception Levels - Context Switching](https://krinkinmu.github.io/2021/01/04/aarch64-exception-levels.html)
- [Accessing stack pointer on AArch64](https://forums.raspberrypi.com/viewtopic.php?t=226080)

### SLOPIX Internal Documentation

- `/Users/davidklassen/work/davidklassen/slopix/docs/arm64-page-tables.md` - Page table architecture
- `/Users/davidklassen/work/davidklassen/slopix/docs/arm64-registers.md` - System register reference
- `/Users/davidklassen/work/davidklassen/slopix/exceptions.S` - Current exception handlers
- `/Users/davidklassen/work/davidklassen/slopix/process.h` - Process structure and context
- `/Users/davidklassen/work/davidklassen/slopix/memory.h` - Page table entry definitions

---

## Summary

This document provides a complete reference for implementing ARM64 userspace (EL0) in SLOPIX:

1. **Exception Levels**: EL0 is unprivileged, must use SVC to access kernel
2. **System Calls**: SVC instruction with x8=syscall number, x0-x7=arguments
3. **Calling Convention**: ARM64 Linux uses x8 for syscall number, preserves registers
4. **Memory Protection**: AP bits control access, UXN/PXN prevent execution, separate page tables
5. **Context Switching**: Save all registers, ELR_EL1, SPSR_EL1, use separate kernel stack
6. **ERET**: Only way to return to lower EL, atomically restores PC and PSTATE
7. **Gotchas**: Register preservation, stack alignment, memory barriers, PXN on user pages

**Next Steps**: Implement separate EL0 exception handlers, set up per-process user page tables, create syscall dispatcher, and load first user program.
