# M6 Userspace Implementation Roadmap

**Project**: SLOPIX - Educational ARM64 OS Kernel
**Milestone**: M6 - Userspace (EL0) Support
**Date**: 2026-01-11
**Tech Lead**: ARM Expert Review

---

## Executive Summary

SLOPIX has successfully completed M1-M5, establishing a solid foundation with:
- ✅ Boot, UART, printf (M1)
- ✅ GIC, timer, exception handlers (M2)
- ✅ Physical page allocator (M3)
- ✅ Preemptive multitasking at EL1 (M4)
- ✅ MMU with dual page tables (TTBR0/TTBR1) (M5)

**Current Status**: All processes run at EL1 (kernel mode). M6 will enable EL0 (userspace) execution.

**Critical Findings**:
- 6 bugs/issues identified that block M6 implementation
- 1 critical security vulnerability in page table permissions
- Context frame size inconsistency between components
- Comprehensive documentation exists but implementation incomplete

---

## Part 1: ARM64 Specification Reference

### 1.1 ARM Architecture Reference Manual (ARM DDI 0487)

**Relevant Sections for M6**:

#### Exception Levels (D1.1)
- **EL0**: Unprivileged execution (userspace)
- **EL1**: Privileged execution (kernel)
- **Transitions**: EL0→EL1 via exceptions, EL1→EL0 via ERET

#### System Call Mechanism (D1.17)
- **SVC instruction**: Supervisor Call - triggers synchronous exception
- **ESR_EL1**: Exception Syndrome Register
  - EC field [31:26]: Exception Class
  - EC = 0x15: SVC instruction from AArch64 state (EL0)
  - EC = 0x15: SVC instruction from AArch64 state (same EL)
  - ISS field [24:0]: Immediate value from SVC instruction

#### Stack Pointers (B1.3.2)
- **SP_EL0**: User stack pointer (used at EL0)
- **SP_EL1**: Kernel stack pointer (used at EL1 when SPSel=1)
- **SPSel bit**: Selects which SP to use at current EL
- **Critical**: Hardware does NOT automatically save/restore SP_EL0

#### Exception Entry (D1.10.2)
**Hardware automatically**:
1. Saves PC → ELR_EL1
2. Saves PSTATE → SPSR_EL1
3. Updates PSTATE (sets mode, masks interrupts)
4. Sets PC to VBAR_EL1 + offset
5. Records exception info in ESR_EL1

**Hardware does NOT**:
- Save general-purpose registers (x0-x30)
- Save SP_EL0 (must be done manually)
- Save TTBR0_EL1 (must be done manually)

#### Exception Return - ERET (D1.10.3)
**Hardware automatically**:
1. Restores PC ← ELR_EL1
2. Restores PSTATE ← SPSR_EL1
3. Changes exception level based on SPSR_EL1 mode bits

**Hardware does NOT**:
- Restore general-purpose registers
- Restore SP_EL0 (must be done manually before ERET)

#### Exception Vector Table (D1.10.2)
Base address in VBAR_EL1, offsets for each exception type:

| Offset | Exception Type | Source |
|--------|---------------|---------|
| 0x000 | Synchronous | Current EL with SP0 |
| 0x080 | IRQ/vIRQ | Current EL with SP0 |
| 0x100 | FIQ/vFIQ | Current EL with SP0 |
| 0x180 | SError/vSError | Current EL with SP0 |
| 0x200 | Synchronous | Current EL with SPx |
| 0x280 | IRQ/vIRQ | Current EL with SPx |
| 0x300 | FIQ/vFIQ | Current EL with SPx |
| 0x380 | SError/vSError | Current EL with SPx |
| **0x400** | **Synchronous** | **Lower EL (AArch64)** ← **SYSCALLS** |
| **0x480** | **IRQ/vIRQ** | **Lower EL (AArch64)** |
| 0x500 | FIQ/vFIQ | Lower EL (AArch64) |
| 0x580 | SError/vSError | Lower EL (AArch64) |
| 0x600 | Synchronous | Lower EL (AArch32) |
| 0x680 | IRQ/vIRQ | Lower EL (AArch32) |
| 0x700 | FIQ/vFIQ | Lower EL (AArch32) |
| 0x780 | SError/vSError | Lower EL (AArch32) |

### 1.2 Page Table Permissions (D5.3.3)

#### AP (Access Permissions) Bits [7:6]

| AP[2:1] | Privileged | Unprivileged | Description |
|---------|-----------|--------------|-------------|
| 00 | RW | No access | Kernel only |
| 01 | RW | RW | User accessible |
| 10 | RO | No access | Kernel read-only |
| 11 | RO | RO | User read-only |

#### Execute-Never Bits

- **UXN (bit 54)**: User Execute Never
  - 0 = Execution allowed at EL0
  - 1 = Execution forbidden at EL0
- **PXN (bit 53)**: Privileged Execute Never
  - 0 = Execution allowed at EL1/EL2/EL3
  - 1 = Execution forbidden at EL1/EL2/EL3

**Security Best Practice**: Set PXN=1 on all user-accessible pages to prevent kernel from executing user code (prevents ret2usr attacks).

### 1.3 AAPCS64 Calling Convention

**ARM Procedure Call Standard for AArch64**:

#### Register Usage
- **x0-x7**: Parameter/result registers
- **x8**: Indirect result location, syscall number in Linux
- **x9-x15**: Temporary registers (caller-saved)
- **x16-x17**: Intra-procedure-call temporary registers
- **x18**: Platform register (platform-specific use)
- **x19-x28**: Callee-saved registers
- **x29**: Frame pointer
- **x30**: Link register (return address)
- **SP**: Stack pointer (must be 16-byte aligned at function boundaries)

#### Syscall Convention (Linux ARM64 Standard)
- **x8**: Syscall number
- **x0-x7**: Arguments (up to 8)
- **x0**: Return value
- **All other registers preserved** across syscall

---

## Part 2: Open Source Implementation Analysis

### 2.1 Linux Kernel (ARM64)

**Reference**: `arch/arm64/kernel/entry.S`

#### Key Insights

1. **Separate EL0 Exception Handlers**:
   - Linux has distinct handlers for EL0→EL1 transitions
   - `el0_sync`, `el0_irq`, `el0_fiq`, `el0_error` at offset 0x400+

2. **Stack Switching**:
   ```assembly
   kernel_entry 0  // 0 = from EL0
   mrs x25, sp_el0  // Save user stack
   // ... save x0-x30 to kernel stack
   ```

3. **Syscall Dispatch**:
   ```assembly
   ldr x16, [tbl, scno, lsl #3]  // Load syscall handler from table
   blr x16                        // Call handler
   ```

4. **Return to Userspace**:
   ```assembly
   kernel_exit 0   // 0 = to EL0
   msr sp_el0, x25  // Restore user stack
   eret
   ```

### 2.2 seL4 Microkernel (ARM64)

**Reference**: `src/arch/arm/64/machine/`

#### Key Insights

1. **Context Structure**:
   ```c
   struct user_context {
       word_t registers[31];  // x0-x30
       word_t sp_el0;
       word_t elr_el1;
       word_t spsr_el1;
   };
   ```

2. **Per-Process Page Tables**:
   - Each process has own TTBR0_EL1
   - Context switch updates TTBR0_EL1
   - Requires TLB invalidation (TLBI instruction)

3. **Fast Path Syscalls**:
   - Inline syscall handlers for common operations
   - Avoid full context save for simple syscalls

### 2.3 Zephyr RTOS (ARM64)

**Reference**: `arch/arm64/core/`

#### Key Insights

1. **Minimal Context Switching**:
   - Only saves callee-saved registers (x19-x28, x29, x30)
   - Assumes caller-saved already on stack

2. **Stack Alignment**:
   - ARM64 requires 16-byte SP alignment
   - Context frame size must be multiple of 16

3. **EL0 Stack Allocation**:
   - Allocates both kernel and user stacks per thread
   - Separate stack pools for efficiency

---

## Part 3: Current Code Review - Bugs & Issues

### 3.1 CRITICAL SECURITY BUG: PTE_USER_CODE Missing PXN

**File**: `memory.h:57`
**Severity**: CRITICAL
**Impact**: Kernel can execute user code (ret2usr vulnerability)

```c
#define PTE_USER_CODE    (PTE_USER)  // WRONG: Missing PXN
```

**Problem**:
- User code pages should have PXN=1 to prevent kernel execution
- Current definition allows kernel to jump into user code
- Classic privilege escalation vector

**Evidence**: Test confirms this is wrong (`tests/test_pte_bits.c:53-54`):
```c
// PXN = 0 (but wait, should be 1 for security!)
ASSERT_EQ((pte >> 53) & 1, 0);
```

**Fix**:
```c
#define PTE_USER_CODE    (PTE_USER | PTE_PXN)  // User RW+X, kernel cannot execute
```

---

### 3.2 BUG: Exception Handlers Don't Save/Restore SP_EL0

**File**: `exceptions.S:49-181`
**Severity**: HIGH
**Impact**: User stack pointer corrupted on context switch

**Current Code**: Saves 34 registers (272 bytes):
```assembly
exception_handler_irq:
    stp x0, x1, [sp, #-16]!
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    // ... saves x2-x28 ...
    stp x2, xzr, [sp, #-16]!
    // Total: 34 quad-words = 272 bytes
```

**Problem**:
- SP_EL0 is never saved to stack
- When switching from EL0 process A to EL0 process B:
  1. Exception entry: SP_EL0 still has process A's user stack
  2. Handler saves context to kernel stack (SP_EL1)
  3. Handler switches to process B
  4. Handler restores process B's context (but SP_EL0 unchanged!)
  5. ERET: Process B runs with process A's user stack → CORRUPTION

**Required Fix**: Add SP_EL0 and TTBR0_EL1 to save/restore sequence:
```assembly
exception_handler_irq:
    stp x0, x1, [sp, #-16]!
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #-16]!
    // ... save x2-x30 (32 bytes total) ...

    // NEW: Save SP_EL0 and TTBR0_EL1
    mrs x0, sp_el0
    mrs x1, ttbr0_el1
    stp x0, x1, [sp, #-16]!

    // Total: 36 quad-words = 288 bytes
```

---

### 3.3 BUG: Scheduler Doesn't Save/Restore TTBR0_EL1

**File**: `scheduler.c:38-203`
**Severity**: HIGH
**Impact**: All EL0 processes share same page table

**Current Code**: Saves/restores x0-x30, PC, PSTATE but NOT TTBR0_EL1

**Problem**:
- TTBR0_EL1 determines which page table is used for user addresses (0x0000_0000_0000_0000 - 0x0000_FFFF_FFFF_FFFF)
- Each process needs its own TTBR0_EL1 for memory isolation
- Without saving/restoring, all processes see the same memory

**Required Fix**:
1. Save TTBR0_EL1 in context save (scheduler.c:98-132)
2. Restore TTBR0_EL1 in context restore (scheduler.c:164-200)
3. Update TTBR0_EL1 system register on switch:
   ```c
   __asm__ volatile("msr ttbr0_el1, %0" :: "r"(next->context.ttbr0_el1));
   __asm__ volatile("tlbi vmalle1");  // Invalidate TLB
   __asm__ volatile("dsb sy");
   __asm__ volatile("isb");
   ```

---

### 3.4 BUG: Context Frame Size Inconsistency

**Files**: `process.h`, `exceptions.S`, `scheduler.c`
**Severity**: MEDIUM
**Impact**: Will cause stack corruption when SP_EL0/TTBR0_EL1 are used

**Current State**:
- `process.h:58`: `CONTEXT_FRAME_SIZE = 36` (includes sp_el0, ttbr0_el1)
- `exceptions.S`: Saves 34 registers (272 bytes)
- `scheduler.c:54,165`: Uses `34` for frame allocation

**Problem**:
- Header says 36 but code uses 34
- When SP_EL0/TTBR0_EL1 are added to exception handlers (to fix bug 3.2), scheduler must also use 36
- Mismatch will corrupt stack

**Fix**: Update scheduler.c to use CONTEXT_FRAME_SIZE (36) instead of hardcoded 34

---

### 3.5 BUG: No Separate EL0 Exception Handlers

**File**: `exceptions.S:28-46`
**Severity**: MEDIUM
**Impact**: Cannot distinguish EL0→EL1 from EL1→EL1 transitions

**Current Code**: All vectors jump to same handlers:
```assembly
    // Lower EL using AArch64 (offset 0x400)
    .align 7
    b exception_handler_sync      // Should be el0_sync_handler
    .align 7
    b exception_handler_irq       // Should be el0_irq_handler
```

**Problem**:
- EL0→EL1 transitions need different handling:
  - Must save SP_EL0 (EL1→EL1 doesn't use SP_EL0)
  - May need to switch page tables (TTBR0_EL1)
  - Different security checks (validate user pointers)

**Required Fix**: Create separate handlers:
```assembly
    // Lower EL using AArch64 (offset 0x400)
    .align 7
    b el0_sync_handler      // NEW: EL0→EL1 sync
    .align 7
    b el0_irq_handler       // NEW: EL0→EL1 IRQ
```

---

### 3.6 BUG: No Syscall Dispatcher

**File**: `interrupts.c:49-73`
**Severity**: HIGH
**Impact**: SVC instruction causes system halt instead of syscall

**Current Code**: Just prints and hangs:
```c
void *handle_sync_exception_with_context(void *stack_ptr) {
    unsigned long esr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    unsigned int ec = (esr >> 26) & 0x3F;

    printf("[EXCEPTION] Synchronous exception\n");
    printf("  EC=0x%x\n", ec);
    // ... more printing ...

    while (1);  // HALT!
}
```

**Problem**:
- EC=0x15 indicates SVC instruction (syscall)
- Should dispatch to syscall handler, not halt

**Required Fix**: Add syscall dispatcher:
```c
void *handle_sync_exception_with_context(void *stack_ptr) {
    unsigned long esr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    unsigned int ec = (esr >> 26) & 0x3F;

    if (ec == 0x15) {  // SVC instruction
        return handle_syscall(stack_ptr);
    }

    // Other synchronous exceptions (page fault, etc.)
    printf("[EXCEPTION] Unhandled sync exception EC=0x%x\n", ec);
    while (1);
}
```

---

## Part 4: Detailed Implementation Roadmap

### Overall Strategy

**Philosophy**: Each step must be:
1. **Minimal**: Changes only what's necessary
2. **Testable**: Has clear pass/fail criteria
3. **Non-breaking**: Doesn't break existing EL1 processes
4. **Incremental**: Builds on previous steps

**Testing Approach**:
- Keep existing EL1 test processes working throughout
- Add new EL0 test processes incrementally
- Each step has dedicated test

---

### Phase 1: Fix Critical Bugs (Foundation Hardening)

**Goal**: Fix security and correctness bugs in existing code before adding EL0 support

---

#### Step 1.1: Fix PTE_USER_CODE Security Bug

**Estimated Effort**: 5 minutes
**Risk**: LOW (simple constant change)

**Changes**:
```c
// File: memory.h

// BEFORE:
#define PTE_USER_CODE    (PTE_USER)

// AFTER:
#define PTE_USER_CODE    (PTE_USER | PTE_PXN)  // User RW+X, kernel cannot execute
```

**Update Test**:
```c
// File: tests/test_pte_bits.c:53-54

// BEFORE:
// PXN = 0 (but wait, should be 1 for security!)
ASSERT_EQ((pte >> 53) & 1, 0);

// AFTER:
// PXN = 1 (security: kernel cannot execute user code)
ASSERT_EQ((pte >> 53) & 1, 1);
```

**Test**:
```bash
make test
make run-test
# Verify: "PTE_USER_CODE has correct attributes: PASS"
```

**Success Criteria**:
- ✅ Test passes
- ✅ PTE_USER_CODE has PXN bit set
- ✅ Existing EL1 processes still work

---

#### Step 1.2: Extend Exception Handlers to Save/Restore SP_EL0 and TTBR0_EL1

**Estimated Effort**: 30 minutes
**Risk**: MEDIUM (changes exception path - critical code)

**Changes**:

**File: exceptions.S**

Add SP_EL0/TTBR0_EL1 save/restore to both sync and IRQ handlers:

```assembly
exception_handler_sync:
    // CRITICAL: Save x0, x1 FIRST before using them as scratch registers
    stp x0, x1, [sp, #-16]!

    // Save ELR_EL1 and SPSR_EL1
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #-16]!

    // Save remaining general purpose registers (x2-x30)
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
    stp x2, xzr, [sp, #-16]!

    // NEW: Save SP_EL0 and TTBR0_EL1
    mrs x0, sp_el0
    mrs x1, ttbr0_el1
    stp x0, x1, [sp, #-16]!

    // Context frame now: 36 * 8 = 288 bytes (16-byte aligned)
    // Layout: [sp_el0, ttbr0_el1, x2, xzr, x3, x4, ..., x30, ELR, SPSR, x0, x1]

    mov x0, sp
    bl handle_sync_exception_with_context
    mov sp, x0

    // NEW: Restore SP_EL0 and TTBR0_EL1
    ldp x0, x1, [sp], #16
    msr sp_el0, x0
    msr ttbr0_el1, x1
    // Note: Don't need TLB invalidation here - context may not have changed

    // Restore all registers
    ldp x2, xzr, [sp], #16
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

    // Finally restore original x0, x1
    ldp x0, x1, [sp], #16

    eret
```

**Repeat for exception_handler_irq** (same pattern)

**File: scheduler.c**

Update context save/restore to use 36 instead of 34:

```c
// Line 53-54: Update frame allocation
unsigned long *next_stack = (unsigned long *)next->context.sp_el1;
next_stack -= 36;  // Changed from 34 to 36

// Line 56-89: Update register layout
next_stack[0] = 0;  // sp_el0 (not used for EL1 processes)
next_stack[1] = 0;  // ttbr0_el1 (not used for EL1 processes)
next_stack[2] = next->context.x2;
next_stack[3] = 0;  // xzr
next_stack[4] = next->context.x3;
// ... continue with +2 offset for all registers ...
next_stack[34] = next->context.x0;
next_stack[35] = next->context.x1;

// Line 98-132: Update context save
unsigned long *ctx_ptr = (unsigned long *)stack_ptr;
// Skip sp_el0 and ttbr0_el1 (will be loaded from cpu_context_t)
ctx_ptr += 2;
current->context.x2 = *ctx_ptr++;
// ... rest of save ...

// Line 164-200: Update next process restore (same as 53-89)
```

**File: process.c**

Update SP calculation for new frame size:

```c
// Line 75: Update comment and calculation
/* ARM64 AAPCS64 requires SP to be 16-byte aligned.
 * Context frame: 36 * 8 = 288 bytes (already 16-byte aligned).
 * Initialize SP at top of stack, aligned.
 */
proc->context.sp_el1 = ((unsigned long)stack + stack_size) & ~0xFUL;
```

**Test**:

Create test to verify context frame alignment:
```c
// File: tests/test_context_frame_el0.c

void test_context_frame_size_36(void) {
    TEST("Context frame is 36 quad-words (288 bytes)");
    ASSERT_EQ(CONTEXT_FRAME_SIZE, 36);
    ASSERT_EQ(CONTEXT_FRAME_BYTES, 288);
    ASSERT_EQ(CONTEXT_FRAME_BYTES % 16, 0);
}

void test_sp_el0_ttbr0_saved(void) {
    TEST("Exception handlers save/restore SP_EL0 and TTBR0_EL1");
    // Create process with non-zero sp_el0 and ttbr0_el1
    process_t *proc = process_create(dummy_fn, 4096);
    proc->context.sp_el0 = 0xDEADBEEF;
    proc->context.ttbr0_el1 = 0xCAFEBABE;

    // Trigger timer interrupt (will save/restore context)
    // ... verify sp_el0 and ttbr0_el1 unchanged after context switch ...
}
```

**Success Criteria**:
- ✅ Context frame is 288 bytes (36 quad-words)
- ✅ Exception handlers save/restore 36 registers
- ✅ Scheduler uses 36 for frame size
- ✅ Existing EL1 processes still work (sp_el0=0, ttbr0_el1=0 saved/restored)
- ✅ Test confirms frame size and alignment

---

### Phase 2: Add Syscall Infrastructure

**Goal**: Implement syscall mechanism (detection, dispatch, basic handlers)

---

#### Step 2.1: Detect SVC Instruction in Exception Handler

**Estimated Effort**: 15 minutes
**Risk**: LOW (read-only check)

**Changes**:

**File: interrupts.c**

Update `handle_sync_exception_with_context` to detect SVC:

```c
void *handle_sync_exception_with_context(void *stack_ptr) {
    unsigned long esr, elr, far, spsr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    __asm__ volatile("mrs %0, spsr_el1" : "=r"(spsr));

    unsigned int ec = (esr >> 26) & 0x3F;

    // Check for SVC instruction (EC = 0x15)
    if (ec == 0x15) {
        printf("[SYSCALL] SVC detected - syscall handler not yet implemented\n");
        printf("  ESR_EL1: 0x%x\n", (unsigned int)esr);
        printf("  ELR_EL1: 0x%x (return address)\n", (unsigned int)elr);

        // For now, just return (will implement dispatcher in next step)
        return stack_ptr;
    }

    // Other synchronous exceptions (page fault, etc.)
    printf("[EXCEPTION] Synchronous exception\n");
    printf("  ESR_EL1:  0x%x\n", (unsigned int)esr);
    printf("  EC=0x%x\n", ec);
    printf("  ELR_EL1:  0x%x\n", (unsigned int)elr);
    printf("  FAR_EL1:  0x%x\n", (unsigned int)far);
    printf("  SPSR_EL1: 0x%x\n", (unsigned int)spsr);

    while (1);
}
```

**Test**:

Create test that executes SVC:
```c
// File: tests/test_svc_detection.c

void test_svc_instruction(void) {
    TEST("SVC instruction is detected (EC=0x15)");

    // Execute SVC #0
    __asm__ volatile("svc #0");

    // If we get here, SVC was handled and returned
    printf("  " COLOR_GREEN "[SVC] Returned from SVC successfully" COLOR_RESET "\n");
}
```

**Success Criteria**:
- ✅ SVC instruction detected (prints "SVC detected")
- ✅ System doesn't halt (returns from handler)
- ✅ Test passes

---

#### Step 2.2: Implement Syscall Dispatcher and Basic Handlers

**Estimated Effort**: 45 minutes
**Risk**: MEDIUM (new subsystem)

**Changes**:

**File: syscall.h** (NEW)

```c
#ifndef SYSCALL_H
#define SYSCALL_H

// Syscall numbers (Linux ARM64 compatible where possible)
#define SYS_exit      93
#define SYS_write     64
#define SYS_read      63
#define SYS_getpid    172

// Syscall dispatcher
void *syscall_handler(void *stack_ptr);

// Syscall implementations
long sys_exit(int status);
long sys_write(int fd, const void *buf, unsigned long count);
long sys_read(int fd, void *buf, unsigned long count);
long sys_getpid(void);

#endif
```

**File: syscall.c** (NEW)

```c
#include "syscall.h"
#include "printf.h"
#include "process.h"
#include "uart.h"

// Syscall table (sparse array)
typedef long (*syscall_fn_t)(unsigned long, unsigned long, unsigned long,
                              unsigned long, unsigned long, unsigned long);

static syscall_fn_t syscall_table[256] = {0};

void syscall_init(void) {
    syscall_table[SYS_exit] = (syscall_fn_t)sys_exit;
    syscall_table[SYS_write] = (syscall_fn_t)sys_write;
    syscall_table[SYS_read] = (syscall_fn_t)sys_read;
    syscall_table[SYS_getpid] = (syscall_fn_t)sys_getpid;

    printf("[SYSCALL] Syscall interface initialized\n");
}

// Syscall dispatcher
// stack_ptr points to saved context: [sp_el0, ttbr0_el1, x2, xzr, ..., x0, x1]
void *syscall_handler(void *stack_ptr) {
    unsigned long *ctx = (unsigned long *)stack_ptr;

    // Context layout (36 quad-words from lowest to highest address):
    // [0]=sp_el0, [1]=ttbr0_el1, [2]=x2, [3]=xzr, [4]=x3, ...
    // [32]=ELR, [33]=SPSR, [34]=x0, [35]=x1

    // For syscalls, we need to extract from the SAVED register values
    // However, x8 is saved at position [2+7] = [9]
    // Let me recalculate the layout:
    // [sp_el0, ttbr0_el1, x2, xzr, x3, x4, x5, x6, x7, x8, x9, ...]
    // Index: 0,  1,        2,  3,   4,  5,  6,  7,  8,  9,  10, ...

    unsigned long syscall_nr = ctx[9];  // x8 is at index 9
    unsigned long arg0 = ctx[34];       // x0 at index 34
    unsigned long arg1 = ctx[35];       // x1 at index 35
    unsigned long arg2 = ctx[2];        // x2 at index 2
    unsigned long arg3 = ctx[4];        // x3 at index 4
    unsigned long arg4 = ctx[5];        // x4 at index 5
    unsigned long arg5 = ctx[6];        // x5 at index 6

    // Dispatch syscall
    long result = -1;  // Default: not implemented

    if (syscall_nr < 256 && syscall_table[syscall_nr]) {
        syscall_fn_t handler = syscall_table[syscall_nr];
        result = handler(arg0, arg1, arg2, arg3, arg4, arg5);
    } else {
        printf("[SYSCALL] Unknown syscall: %d\n", (int)syscall_nr);
    }

    // Store result in x0 (index 34)
    ctx[34] = result;

    return stack_ptr;
}

// Syscall implementations

long sys_exit(int status) {
    printf("[SYSCALL] exit(%d)\n", status);
    process_exit();
    return 0;  // Never reached
}

long sys_write(int fd, const void *buf, unsigned long count) {
    // For now, only support stdout (fd=1) and stderr (fd=2)
    if (fd != 1 && fd != 2) {
        return -1;  // Bad file descriptor
    }

    // TODO: Validate buf is in user space

    // Write to UART
    const char *str = (const char *)buf;
    for (unsigned long i = 0; i < count; i++) {
        uart_putc(str[i]);
    }

    return count;
}

long sys_read(int fd, void *buf, unsigned long count) {
    // Not implemented yet
    printf("[SYSCALL] read() not implemented\n");
    return -1;
}

long sys_getpid(void) {
    process_t *current = process_get_current();
    return current ? current->pid : 0;
}
```

**File: interrupts.c**

Update to call syscall_handler:

```c
#include "syscall.h"  // Add include

void *handle_sync_exception_with_context(void *stack_ptr) {
    unsigned long esr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    unsigned int ec = (esr >> 26) & 0x3F;

    if (ec == 0x15) {  // SVC instruction
        return syscall_handler(stack_ptr);
    }

    // Other exceptions...
    unsigned long elr, far, spsr;
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    __asm__ volatile("mrs %0, spsr_el1" : "=r"(spsr));

    printf("[EXCEPTION] Synchronous exception\n");
    printf("  ESR_EL1:  0x%x (EC=0x%x)\n", (unsigned int)esr, ec);
    printf("  ELR_EL1:  0x%x\n", (unsigned int)elr);
    printf("  FAR_EL1:  0x%x\n", (unsigned int)far);
    printf("  SPSR_EL1: 0x%x\n", (unsigned int)spsr);

    while (1);
}
```

**File: main.c**

Add syscall_init() call:

```c
#include "syscall.h"  // Add include

void kernel_main(void) {
    // ... existing init ...
    interrupts_init();
    syscall_init();  // NEW
    // ... rest of init ...
}
```

**Test**:

```c
// File: tests/test_syscall_dispatch.c

void test_syscall_getpid(void) {
    TEST("Syscall: getpid()");

    long pid;
    __asm__ volatile(
        "mov x8, %1\n"      // Syscall number in x8
        "svc #0\n"          // Invoke syscall
        "mov %0, x0\n"      // Get result from x0
        : "=r"(pid)
        : "i"(SYS_getpid)
        : "x8", "x0"
    );

    process_t *current = process_get_current();
    ASSERT_EQ(pid, current->pid);
}

void test_syscall_write(void) {
    TEST("Syscall: write()");

    const char *msg = "Hello from syscall!\n";
    long result;

    __asm__ volatile(
        "mov x8, %1\n"      // SYS_write
        "mov x0, #1\n"      // fd = stdout
        "mov x1, %2\n"      // buf
        "mov x2, #20\n"     // count
        "svc #0\n"
        "mov %0, x0\n"      // result
        : "=r"(result)
        : "i"(SYS_write), "r"(msg)
        : "x8", "x0", "x1", "x2"
    );

    ASSERT_EQ(result, 20);
}
```

**Success Criteria**:
- ✅ SVC instruction invokes syscall_handler
- ✅ Syscall number extracted from x8
- ✅ Arguments extracted from x0-x5
- ✅ Result returned in x0
- ✅ getpid() returns correct PID
- ✅ write() outputs to UART
- ✅ Tests pass

---

### Phase 3: Create First EL0 Process

**Goal**: Create and run first userspace process at EL0

---

#### Step 3.1: Create User Process Creation Function

**Estimated Effort**: 45 minutes
**Risk**: MEDIUM (new process type)

**Changes**:

**File: process.h**

Add new function:
```c
// Create EL0 (userspace) process with separate user and kernel stacks
process_t *process_create_user(void (*entry)(void), unsigned long user_stack_size);
```

**File: process.c**

```c
process_t *process_create_user(void (*entry)(void), unsigned long user_stack_size) {
    // Allocate process structure
    process_t *proc = (process_t *)pmm_alloc_page();
    if (!proc) {
        printf("[PROCESS] Failed to allocate process structure\n");
        return 0;
    }

    // Allocate kernel stack (for syscall handling) - fixed size 8KB
    void *kernel_stack = pmm_alloc_page();
    if (!kernel_stack) {
        printf("[PROCESS] Failed to allocate kernel stack\n");
        pmm_free_page(proc);
        return 0;
    }
    void *kernel_stack2 = pmm_alloc_page();
    if (!kernel_stack2) {
        pmm_free_page(kernel_stack);
        pmm_free_page(proc);
        return 0;
    }

    // Allocate user stack
    unsigned long num_pages = (user_stack_size + PAGE_SIZE - 1) / PAGE_SIZE;
    void *user_stack = 0;
    void **allocated_pages = (void **)pmm_alloc_page();
    unsigned long allocated_count = 0;

    if (!allocated_pages) {
        printf("[PROCESS] Failed to allocate tracking page\n");
        pmm_free_page(kernel_stack2);
        pmm_free_page(kernel_stack);
        pmm_free_page(proc);
        return 0;
    }

    for (unsigned long i = 0; i < num_pages; i++) {
        void *page = pmm_alloc_page();
        if (!page) {
            printf("[PROCESS] Failed to allocate user stack\n");
            for (unsigned long j = 0; j < allocated_count; j++) {
                pmm_free_page(allocated_pages[j]);
            }
            pmm_free_page(allocated_pages);
            pmm_free_page(kernel_stack2);
            pmm_free_page(kernel_stack);
            pmm_free_page(proc);
            return 0;
        }
        allocated_pages[allocated_count++] = page;
        if (i == 0) {
            user_stack = page;
        }
    }

    pmm_free_page(allocated_pages);

    // Initialize process structure
    proc->pid = next_pid++;
    proc->state = PROCESS_READY;
    proc->stack = kernel_stack;  // PCB points to kernel stack
    proc->stack_size = PAGE_SIZE * 2;  // 8KB kernel stack
    proc->next = 0;

    // Initialize context - clear all registers
    for (int i = 0; i < 31; i++) {
        ((unsigned long *)&proc->context)[i] = 0;
    }

    // Set up kernel stack (SP_EL1) - used during syscalls/exceptions
    proc->context.sp_el1 = ((unsigned long)kernel_stack + PAGE_SIZE * 2) & ~0xFUL;

    // Set up user stack (SP_EL0) - used when running at EL0
    proc->context.sp_el0 = ((unsigned long)user_stack + user_stack_size) & ~0xFUL;

    // Set up initial execution state
    proc->context.x30 = (unsigned long)process_exit;
    proc->context.pc = (unsigned long)entry;
    proc->context.pstate = PSTATE_EL0T_IRQ_ENABLED;  // EL0 mode!

    // Mark as EL0 process
    proc->context.exception_level = 0;

    // TODO: Set up per-process page table (TTBR0_EL1)
    // For now, use identity mapping (same as kernel)
    proc->context.ttbr0_el1 = 0;  // Will set up in later step

    printf("[PROCESS] Created EL0 process PID=%d, user_sp=0x%lx, kernel_sp=0x%lx\n",
           proc->pid, proc->context.sp_el0, proc->context.sp_el1);

    return proc;
}
```

**Test**:

```c
// File: tests/test_el0_process_create.c

void dummy_user_fn(void) {
    // User code - do nothing
}

void test_el0_process_creation(void) {
    TEST("Create EL0 process");

    process_t *proc = process_create_user(dummy_user_fn, 4096);
    ASSERT(proc != 0, "Process created");

    if (proc) {
        ASSERT_EQ(proc->context.exception_level, 0);
        ASSERT_EQ(proc->context.pstate, PSTATE_EL0T_IRQ_ENABLED);
        ASSERT(proc->context.sp_el0 != 0, "User stack set");
        ASSERT(proc->context.sp_el1 != 0, "Kernel stack set");
        ASSERT(proc->context.sp_el0 != proc->context.sp_el1, "Stacks are different");
        ASSERT(process_is_el0(proc), "Marked as EL0 process");
    }
}
```

**Success Criteria**:
- ✅ EL0 process created successfully
- ✅ Has separate user and kernel stacks
- ✅ PSTATE set to EL0 mode
- ✅ exception_level = 0
- ✅ Test passes

---

#### Step 3.2: Create Separate EL0 Exception Handlers

**Estimated Effort**: 1 hour
**Risk**: HIGH (complex assembly, critical path)

**Changes**:

**File: exceptions.S**

Add new handlers for Lower EL (offset 0x400+):

```assembly
    // Lower EL using AArch64 (offset 0x400)
    .align 7
    b el0_sync_handler          // NEW: EL0→EL1 synchronous
    .align 7
    b el0_irq_handler           // NEW: EL0→EL1 IRQ
    .align 7
    b el0_fiq_handler           // NEW: EL0→EL1 FIQ
    .align 7
    b el0_serror_handler        // NEW: EL0→EL1 SError

// NEW: EL0→EL1 synchronous exception handler
el0_sync_handler:
    // When entering from EL0:
    // - SP_EL1 is already active (SPSel=1)
    // - SP_EL0 contains user stack (must be saved manually)
    // - TTBR0_EL1 contains user page table (must be saved manually)

    // Save x0, x1 first
    stp x0, x1, [sp, #-16]!

    // Save ELR_EL1 and SPSR_EL1
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #-16]!

    // Save all general purpose registers
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
    stp x2, xzr, [sp, #-16]!

    // CRITICAL: Save SP_EL0 and TTBR0_EL1 (required for EL0→EL1)
    mrs x0, sp_el0
    mrs x1, ttbr0_el1
    stp x0, x1, [sp, #-16]!

    // Call C handler
    mov x0, sp
    bl handle_sync_exception_with_context
    mov sp, x0

    // Restore SP_EL0 and TTBR0_EL1
    ldp x0, x1, [sp], #16
    msr sp_el0, x0
    msr ttbr0_el1, x1

    // TLB invalidation (in case page table changed)
    tlbi vmalle1
    dsb sy
    isb

    // Restore all registers
    ldp x2, xzr, [sp], #16
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

    // Restore x0, x1
    ldp x0, x1, [sp], #16

    // Return to EL0
    eret

// NEW: EL0→EL1 IRQ handler
el0_irq_handler:
    // Same structure as el0_sync_handler
    stp x0, x1, [sp, #-16]!
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #-16]!

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
    stp x2, xzr, [sp, #-16]!

    mrs x0, sp_el0
    mrs x1, ttbr0_el1
    stp x0, x1, [sp, #-16]!

    mov x0, sp
    bl handle_irq_with_context
    mov sp, x0

    ldp x0, x1, [sp], #16
    msr sp_el0, x0
    msr ttbr0_el1, x1
    tlbi vmalle1
    dsb sy
    isb

    ldp x2, xzr, [sp], #16
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

    ldp x0, x1, [sp], #16
    msr elr_el1, x0
    msr spsr_el1, x1

    ldp x0, x1, [sp], #16
    eret

el0_fiq_handler:
    stp x29, x30, [sp, #-16]!
    bl handle_fiq
    ldp x29, x30, [sp], #16
    eret

el0_serror_handler:
    stp x29, x30, [sp, #-16]!
    bl handle_serror
    ldp x29, x30, [sp], #16
    eret
```

**Update EL1 handlers**: Keep existing `exception_handler_sync` and `exception_handler_irq` for EL1→EL1 transitions (offsets 0x200-0x380). These don't need SP_EL0 save/restore since EL1 code doesn't use SP_EL0.

**Success Criteria**:
- ✅ Separate handlers at offset 0x400+
- ✅ EL0 handlers save/restore SP_EL0
- ✅ EL0 handlers save/restore TTBR0_EL1
- ✅ Code compiles and links
- ✅ Existing EL1 processes still work

---

#### Step 3.3: Create Simple User Program and Test

**Estimated Effort**: 30 minutes
**Risk**: MEDIUM (first EL0 execution)

**Test**:

```c
// File: tests/test_el0_hello.c

// User program - runs at EL0
void user_hello(void) {
    // Use syscall to print message
    const char *msg = "Hello from EL0!\n";

    __asm__ volatile(
        "mov x8, %0\n"      // SYS_write
        "mov x0, #1\n"      // stdout
        "mov x1, %1\n"      // buffer
        "mov x2, #16\n"     // count
        "svc #0\n"          // syscall
        :
        : "i"(SYS_write), "r"(msg)
        : "x0", "x1", "x2", "x8"
    );

    // Exit
    __asm__ volatile(
        "mov x8, %0\n"      // SYS_exit
        "mov x0, #0\n"      // status
        "svc #0\n"
        :
        : "i"(SYS_exit)
        : "x0", "x8"
    );
}

void test_el0_execution(void) {
    TEST("First EL0 process execution");

    // Create EL0 process
    process_t *proc = process_create_user(user_hello, 4096);
    ASSERT(proc != 0, "EL0 process created");

    // Add to scheduler
    scheduler_add(proc);

    // Process will run when timer fires
    // Expected output: "Hello from EL0!"
}
```

**Success Criteria**:
- ✅ EL0 process created
- ✅ Starts execution at EL0 (PSTATE mode = 0x0)
- ✅ SVC instruction triggers syscall
- ✅ Syscall handler executes at EL1
- ✅ Returns to EL0 correctly
- ✅ Prints "Hello from EL0!"
- ✅ Process exits cleanly

---

### Phase 4: Per-Process Page Tables (Memory Isolation)

**Goal**: Each EL0 process gets its own TTBR0_EL1 page table for memory isolation

---

#### Step 4.1: Implement Page Table Creation for User Process

**Estimated Effort**: 1.5 hours
**Risk**: HIGH (MMU programming, complex)

**Changes**:

**File: mmu.h** (NEW)

```c
#ifndef MMU_H
#define MMU_H

#include <stdint.h>

// Create a new page table for user process
// Returns physical address of L1 table
unsigned long mmu_create_user_page_table(void);

// Map a page in user page table
// virt_addr: virtual address (0x0 - 0x7FFF_FFFF range)
// phys_addr: physical address
// permissions: PTE flags (PTE_USER_CODE, PTE_USER_DATA, etc.)
int mmu_map_user_page(unsigned long ttbr0, unsigned long virt_addr,
                      unsigned long phys_addr, unsigned long permissions);

// Free user page table
void mmu_free_user_page_table(unsigned long ttbr0);

#endif
```

**File: mmu.c**

Add functions:

```c
#include "mmu.h"
#include "pmm.h"
#include "memory.h"
#include "printf.h"

// Create empty user page table (identity mapped for now)
unsigned long mmu_create_user_page_table(void) {
    // Allocate L1 page table
    void *l1_table = pmm_alloc_page();
    if (!l1_table) {
        return 0;
    }

    // Clear table
    unsigned long *l1 = (unsigned long *)l1_table;
    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
        l1[i] = 0;
    }

    // For now, create identity mapping for lower 1GB
    // This allows simple testing without ELF loader

    // Allocate L2 table for first 1GB (VA 0x00000000 - 0x3FFFFFFF)
    void *l2_table = pmm_alloc_page();
    if (!l2_table) {
        pmm_free_page(l1_table);
        return 0;
    }

    // Clear L2 table
    unsigned long *l2 = (unsigned long *)l2_table;
    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
        l2[i] = 0;
    }

    // Point L1[0] to L2 table
    unsigned long l2_phys = (unsigned long)l2_table - KERNEL_VIRT_OFFSET;
    l1[0] = l2_phys | PTE_TABLE | PTE_VALID;

    // Map first 128MB as identity (covers DRAM at 0x40000000)
    // Use 2MB blocks for simplicity
    for (int i = 0; i < 64; i++) {  // 64 * 2MB = 128MB
        unsigned long pa = i * L2_BLOCK_SIZE;
        unsigned long pte = pa | PTE_VALID | PTE_BLOCK | PTE_AF;

        // Determine memory type and permissions
        if (pa >= PHYS_MEMORY_START && pa < PHYS_MEMORY_END) {
            // DRAM: user accessible, normal memory
            pte |= (MT_NORMAL << 2) | PTE_USER | PTE_UXN | PTE_PXN;
        } else if (pa >= DEVICE_REGION_START && pa < DEVICE_REGION_END) {
            // Devices: not accessible from user mode
            pte |= (MT_DEVICE_nGnRnE << 2) | PTE_KERNEL | PTE_UXN | PTE_PXN;
        } else {
            // Other: not accessible
            continue;
        }

        l2[i] = pte;
    }

    // Return physical address of L1 table
    unsigned long l1_phys = (unsigned long)l1_table - KERNEL_VIRT_OFFSET;
    return l1_phys;
}

// Map single 4KB page in user page table
int mmu_map_user_page(unsigned long ttbr0, unsigned long virt_addr,
                      unsigned long phys_addr, unsigned long permissions) {
    // TODO: Implement fine-grained 4KB page mapping
    // For M6, coarse 2MB blocks are sufficient
    return 0;
}

// Free user page table
void mmu_free_user_page_table(unsigned long ttbr0) {
    // TODO: Walk page table and free all allocated pages
    // For M6, skip (leaks memory but acceptable for testing)
}
```

**File: process.c**

Update `process_create_user` to create page table:

```c
#include "mmu.h"  // Add include

process_t *process_create_user(void (*entry)(void), unsigned long user_stack_size) {
    // ... existing allocation code ...

    // NEW: Create per-process page table
    unsigned long page_table = mmu_create_user_page_table();
    if (!page_table) {
        printf("[PROCESS] Failed to create user page table\n");
        // ... cleanup code ...
        return 0;
    }

    // ... existing context init ...

    proc->context.ttbr0_el1 = page_table;  // Set page table base

    printf("[PROCESS] Created EL0 process PID=%d, ttbr0=0x%lx\n",
           proc->pid, page_table);

    return proc;
}
```

**Success Criteria**:
- ✅ Each EL0 process gets unique TTBR0_EL1
- ✅ Page table identity-maps lower 128MB
- ✅ User code can access DRAM
- ✅ EL0 process runs successfully with own page table
- ✅ Context switch updates TTBR0_EL1 correctly

---

### Phase 5: Polish and Hardening

**Goal**: Add robustness, validation, cleanup

---

#### Step 5.1: Add User Pointer Validation

**Estimated Effort**: 30 minutes
**Risk**: LOW

**Changes**:

**File: syscall.c**

Add validation:

```c
// Validate user pointer is in user address space
static int is_user_pointer_valid(const void *ptr, unsigned long size) {
    unsigned long addr = (unsigned long)ptr;

    // User space is 0x0000_0000_0000_0000 to 0x0000_FFFF_FFFF_FFFF (TTBR0 range)
    // But we only mapped first 128MB (0x0000_0000 - 0x07FF_FFFF)
    if (addr < 0x08000000 && (addr + size) <= 0x08000000) {
        return 1;  // Valid
    }

    return 0;  // Invalid
}

long sys_write(int fd, const void *buf, unsigned long count) {
    if (fd != 1 && fd != 2) {
        return -1;
    }

    // NEW: Validate buffer is in user space
    if (!is_user_pointer_valid(buf, count)) {
        printf("[SYSCALL] write: invalid user pointer 0x%lx\n", (unsigned long)buf);
        return -1;
    }

    const char *str = (const char *)buf;
    for (unsigned long i = 0; i < count; i++) {
        uart_putc(str[i]);
    }

    return count;
}
```

**Success Criteria**:
- ✅ Syscalls validate user pointers
- ✅ Invalid pointers rejected with error
- ✅ Prevents kernel from accessing invalid addresses

---

#### Step 5.2: Add Integration Test

**Estimated Effort**: 45 minutes
**Risk**: LOW

**Test**:

```c
// File: tests/test_el0_integration.c

void user_integration_test(void) {
    // Test multiple syscalls
    const char *msg1 = "Testing syscalls from EL0\n";
    __asm__ volatile(
        "mov x8, %0\n mov x0, #1\n mov x1, %1\n mov x2, #27\n svc #0\n"
        :: "i"(SYS_write), "r"(msg1) : "x0","x1","x2","x8");

    // Test getpid
    long pid;
    __asm__ volatile(
        "mov x8, %1\n svc #0\n mov %0, x0\n"
        : "=r"(pid) : "i"(SYS_getpid) : "x0", "x8");

    // Print PID
    char buf[32];
    int len = 0;
    buf[len++] = 'P';
    buf[len++] = 'I';
    buf[len++] = 'D';
    buf[len++] = '=';
    buf[len++] = '0' + (pid % 10);
    buf[len++] = '\n';

    __asm__ volatile(
        "mov x8, %0\n mov x0, #1\n mov x1, %1\n mov x2, %2\n svc #0\n"
        :: "i"(SYS_write), "r"(buf), "r"((unsigned long)len)
        : "x0","x1","x2","x8");

    // Exit
    __asm__ volatile(
        "mov x8, %0\n mov x0, #0\n svc #0\n"
        :: "i"(SYS_exit) : "x0", "x8");
}

void test_el0_full_integration(void) {
    TEST("EL0 integration test - syscalls, context switch, exit");

    // Create two EL0 processes
    process_t *proc1 = process_create_user(user_integration_test, 8192);
    process_t *proc2 = process_create_user(user_integration_test, 8192);

    ASSERT(proc1 != 0 && proc2 != 0, "Both processes created");
    ASSERT(proc1->context.ttbr0_el1 != proc2->context.ttbr0_el1,
           "Processes have different page tables");

    scheduler_add(proc1);
    scheduler_add(proc2);

    // Let them run
    // Expected: Both print messages, PIDs, and exit cleanly
}
```

**Success Criteria**:
- ✅ Multiple EL0 processes run concurrently
- ✅ Each has own page table
- ✅ Syscalls work correctly
- ✅ Context switching works
- ✅ Processes exit cleanly

---

## Part 5: Final Roadmap Summary

### Minimal Testable Steps (15 steps total)

| Step | Phase | Task | Effort | Risk | Test |
|------|-------|------|--------|------|------|
| 1.1 | Foundation | Fix PTE_USER_CODE PXN bug | 5 min | LOW | test_pte_bits |
| 1.2 | Foundation | Add SP_EL0/TTBR0 save/restore | 30 min | MED | test_context_frame |
| 2.1 | Syscall | Detect SVC instruction | 15 min | LOW | test_svc_detection |
| 2.2 | Syscall | Implement syscall dispatcher | 45 min | MED | test_syscall_dispatch |
| 3.1 | EL0 | Create user process function | 45 min | MED | test_el0_process_create |
| 3.2 | EL0 | Add EL0 exception handlers | 1 hr | HIGH | Compile test |
| 3.3 | EL0 | First EL0 execution test | 30 min | MED | test_el0_hello |
| 4.1 | MMU | Per-process page tables | 1.5 hr | HIGH | test_page_table |
| 5.1 | Polish | User pointer validation | 30 min | LOW | test_validation |
| 5.2 | Polish | Integration test | 45 min | LOW | test_el0_integration |

**Total Estimated Effort**: ~6.5 hours of focused development

---

## Part 6: Testing Strategy

### Test Pyramid

**Unit Tests** (tests/ directory):
- test_pte_bits.c - Page table permission bits
- test_context_frame_el0.c - Context frame structure
- test_svc_detection.c - SVC instruction detection
- test_syscall_dispatch.c - Syscall argument passing
- test_el0_process_create.c - EL0 process creation
- test_validation.c - User pointer validation

**Integration Tests**:
- test_el0_hello.c - First EL0 program
- test_el0_integration.c - Multiple processes, syscalls

**Regression Tests**:
- Existing M1-M5 tests must continue passing
- EL1 processes must continue working

### Test Execution

```bash
# After each step:
make clean
make test
make run-test

# Verify expected output
# Check for no crashes or hangs
# Confirm all tests pass
```

---

## Part 7: Risk Mitigation

### High-Risk Areas

1. **Exception Handler Assembly** (Step 3.2)
   - **Risk**: Incorrect register save/restore causes corruption
   - **Mitigation**:
     - Test with existing EL1 processes first
     - Add logging to verify SP_EL0 values
     - Single-step in debugger if needed

2. **Page Table Setup** (Step 4.1)
   - **Risk**: Incorrect mappings cause data aborts
   - **Mitigation**:
     - Start with identity mapping
     - Test read/write to each region
     - Add page fault handler logging

3. **Context Switching** (Step 1.2)
   - **Risk**: Frame size mismatch corrupts stack
   - **Mitigation**:
     - Use CONTEXT_FRAME_SIZE constant everywhere
     - Add assertions for SP alignment
     - Test with known patterns (0xDEADBEEF)

### Debugging Tools

```c
// Add debug prints:
#define DEBUG_SYSCALL 1
#define DEBUG_CONTEXT_SWITCH 1
#define DEBUG_PAGE_TABLE 1

// Example:
#if DEBUG_SYSCALL
printf("[SYSCALL] nr=%ld, x0=%lx, x1=%lx\n", nr, arg0, arg1);
#endif
```

---

## Part 8: Success Criteria for M6 Completion

M6 is **COMPLETE** when:

- ✅ At least one EL0 process runs successfully
- ✅ SVC instruction triggers syscall handler
- ✅ Syscalls work: exit, write, getpid
- ✅ EL0 process has separate user and kernel stacks
- ✅ EL0 process has own page table (TTBR0_EL1)
- ✅ Context switching preserves SP_EL0 and TTBR0_EL1
- ✅ User pointers validated before kernel access
- ✅ All M1-M5 features still work (no regressions)
- ✅ Integration test passes with multiple EL0 processes

---

## Part 9: Next Steps (Beyond M6)

### M7: Fork & Exec
- Copy-on-write page tables
- ELF loader
- Process creation from userspace
- exec() syscall

### M8: Filesystem
- VFS layer
- initramfs support
- open, read, write, close syscalls

### M9: Shell
- Terminal driver
- Line editing
- Command execution
- Simple shell program

---

## References

### ARM Architecture
- **ARM DDI 0487**: ARM Architecture Reference Manual ARMv8-A
- **ARM DEN 0024**: ARM Cortex-A Series Programmer's Guide
- **ARM IHI 0055**: Procedure Call Standard for ARM64

### Open Source
- **Linux**: arch/arm64/kernel/entry.S
- **seL4**: src/arch/arm/64/
- **Zephyr**: arch/arm64/core/

### SLOPIX Documentation
- docs/arm64-userspace-el0.md - EL0 implementation guide
- docs/dual-stack-architecture.md - SP_EL0/SP_EL1 guide
- docs/arm64-page-tables.md - MMU reference

---

**Document Version**: 1.0
**Last Updated**: 2026-01-11
**Status**: READY FOR IMPLEMENTATION
