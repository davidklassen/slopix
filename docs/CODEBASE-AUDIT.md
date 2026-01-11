# M6 CODEBASE AUDIT - CRITICAL BLOCKERS

**Auditor**: ARM Expert / Tech Lead
**Date**: 2026-01-11
**Scope**: Identify bugs and issues blocking M6 (userspace) implementation
**Status**: 10 CRITICAL blockers identified

---

## EXECUTIVE SUMMARY

A comprehensive audit of the SLOPIX codebase has identified **10 critical blockers** that prevent M6 (userspace/EL0) implementation. All issues are **CRITICAL** severity and must be resolved before EL0 processes can execute.

**Key Findings**:
- Exception handler cannot detect EL0 exceptions
- Process structure lacks EL0-specific fields
- All memory pages are kernel-only (no user access)
- Single global TTBR0 prevents process isolation
- No system call mechanism exists

---

## BLOCKER #1: EL0 EXCEPTION ROUTING MISSING

**Severity**: CRITICAL
**Component**: `exceptions.S`
**Location**: Lines 28-46

### Problem

The exception vector table and handlers cannot differentiate between exceptions originating from EL0 vs EL1:

1. **Vector table incomplete**: Only 8 handlers defined (Current EL), missing Lower EL handlers
2. **No EL detection**: Handler doesn't check SPSR_EL1.M field to determine source privilege level
3. **Cannot route syscalls**: SVC exceptions from EL0 go to same handler as EL1 exceptions
4. **Cannot return to EL0**: No mechanism to restore EL0 context and ERET back

### Current Code

```asm
// exceptions.S - Only handles Current EL
.align 11
exception_vectors:
    // Current EL with SP0
    exception_entry exception_handler_sync
    exception_entry exception_handler_irq
    // ... 6 more Current EL handlers

    // MISSING: Lower EL AArch64 (offset 0x400)
    // MISSING: EL0 sync handler for syscalls
```

### Impact

- **Blocks**: System calls, EL0 exception handling
- **Consequence**: Cannot execute any EL0 process
- **Fix Effort**: High (4-5 steps in roadmap)

### Solution

1. Expand vector table to 16 entries (add offset 0x400 handlers)
2. Create `el0_exception_handler_sync` for syscalls
3. Detect exception source via `SPSR_EL1.M` bits
4. Route to separate EL0 handler path
5. Implement ERET path that restores EL0 context

**Roadmap Reference**: Phase 5, Steps 5.1-5.3

---

## BLOCKER #2: PROCESS STRUCTURE MISSING EL0 FIELDS

**Severity**: CRITICAL
**Component**: `process.h`
**Location**: Lines 15-24

### Problem

The `cpu_context_t` structure lacks fields needed for EL0 execution:

1. **No SP_EL0 field**: Only stores single `sp` register
   - ARM64 has **two separate stack pointers**: SP_EL0 (user) and SP_EL1 (kernel)
   - EL0 processes need both: user stack for EL0, kernel stack for exception handling
   - Cannot save/restore user stack pointer across context switches

2. **No TTBR0 field**: Missing per-process page table base
   - Required for memory isolation between processes
   - Each process needs its own TTBR0_EL1 value
   - Context switch must reload TTBR0

3. **No exception level indicator**: Cannot track if process is EL0 or EL1
   - Needed to determine which context to restore
   - Required for exception handler routing

4. **PSTATE hardcoded to EL1h**: All processes get `0x5` (EL1h mode)
   - EL0 processes need PSTATE `0x0` (EL0t mode)
   - No way to distinguish user vs kernel processes

### Current Code

```c
typedef struct {
    unsigned long x2, xzr, x3, ..., x30;
    unsigned long sp;        // ONLY ONE SP - INSUFFICIENT
    unsigned long pc;
    unsigned long pstate;
    // MISSING: sp_el0, ttbr0_el1, exception_level
} cpu_context_t;
```

### Impact

- **Blocks**: EL0 process creation, context switching, memory isolation
- **Consequence**: Cannot create or run EL0 processes
- **Fix Effort**: High (affects process.c, scheduler.c, exceptions.S)

### Solution

Add three new fields:

```c
typedef struct {
    // ... existing fields ...
    unsigned long sp_el0;              // User stack pointer
    unsigned long ttbr0_el1;           // Process page table base
    unsigned char exception_level;     // 0=EL0, 1=EL1
    unsigned char _padding[7];
} cpu_context_t;
```

**Roadmap Reference**: Phase 1, Steps 1.1-1.8

---

## BLOCKER #3: SP_EL0 vs SP_EL1 CONFUSION

**Severity**: CRITICAL
**Component**: `process.c`, `scheduler.c`
**Location**: process.c:70-75, scheduler.c:55-56, scheduler.c:166-167

### Problem

Code confuses ARM64's dual stack pointer system:

1. **Single SP assumption**: Process creation only sets one `sp` field
2. **Scheduler unaware**: Only manages `next->context.sp`, doesn't know about SP_EL0
3. **ERET won't work**: When returning to EL0, ERET doesn't restore SP automatically
   - SP_EL0 is a **separate register** from SP_EL1
   - Must manually write SP_EL0 before ERET
4. **Exception handler doesn't save SP_EL0**: Current handler saves SP_EL1 only

### Current Code

```c
// process.c - Only sets one SP
proc->context.sp = (((unsigned long)stack + stack_size - 8) & ~0xFUL) + 8;

// scheduler.c - Only manages one SP
unsigned long *next_stack = (unsigned long *)next->context.sp;
```

### ARM64 Reality

```
EL1h mode: Uses SP_EL1 (implicit)
EL0t mode: Uses SP_EL0 (implicit, separate register)

When EL0 → EL1 (exception):
  - Hardware switches to SP_EL1 automatically
  - SP_EL0 unchanged (must save manually)

When EL1 → EL0 (ERET):
  - Hardware switches to SP_EL0 automatically
  - Must have loaded SP_EL0 register beforehand
```

### Impact

- **Blocks**: EL0 process execution, exception returns
- **Consequence**: User stack corruption, crashes
- **Fix Effort**: Very High (touches scheduler, exception handler, process creation)

### Solution

1. Add `sp_el0` field to `cpu_context_t`
2. Exception handler saves/restores SP_EL0 via MRS/MSR
3. Before ERET to EL0: `msr sp_el0, <saved_value>`
4. Scheduler manages both SP_EL0 and SP_EL1

**Roadmap Reference**: Phase 1 (foundation), Phase 6 (refinement)

---

## BLOCKER #4: ALL PAGES KERNEL-ONLY (NO PTE_USER)

**Severity**: CRITICAL
**Component**: `mmu.c`, `memory.h`
**Location**: memory.h:46-48, mmu.c:78, 91, 119, 132

### Problem

All page table entries use kernel-only permissions:

1. **All PTEs have AP[2:1] = 00**: Kernel RW, User NO ACCESS
2. **PTE_USER defined but never used**: Code always uses `PTE_KERNEL`
3. **Missing UXN/PXN bits**: No execute-never protection
4. **Cannot allocate user pages**: PMM has no concept of user-accessible pages

### Current Code

```c
// memory.h - Defines exist but not used
#define PTE_KERNEL  (0UL << 6)   // AP=00 (kernel only)
#define PTE_USER    (1UL << 6)   // AP=01 (NEVER USED!)

// mmu.c line 78 - Always PTE_KERNEL
entry = phys | ... | PTE_KERNEL | PTE_VALID;
```

### Page Table Reality

```
Current state (all pages):
  AP[2:1] = 00 → Kernel RW, User NO ACCESS

Required for EL0:
  AP[2:1] = 01 → Kernel RW, User RW (user-accessible)
  UXN = 1 → User cannot execute (data pages)
  PXN = 1 → Kernel cannot execute (user code pages, ret2usr mitigation)
```

### Impact

- **Blocks**: EL0 memory access
- **Consequence**: EL0 process gets data abort on ANY memory access
- **Fix Effort**: High (modify all page table setup)

### Solution

1. Define UXN/PXN bits: `#define PTE_UXN (1UL << 54)`
2. Create combined attributes:
   - `PTE_KERNEL_DATA = PTE_KERNEL | PTE_UXN`
   - `PTE_USER_DATA = PTE_USER | PTE_UXN | PTE_PXN`
3. Update page mappings to use appropriate attributes
4. Implement per-process page tables with user mappings

**Roadmap Reference**: Phase 2, Steps 2.1-2.8

---

## BLOCKER #5: SHARED GLOBAL TTBR0 (NO ISOLATION)

**Severity**: CRITICAL
**Component**: `mmu.c`, `scheduler.c`, `boot.S`
**Location**: mmu.c:14-16, boot.S:59, main.c:32

### Problem

TTBR0 is global and shared across all processes:

1. **Single TTBR0 allocated at boot**: Never changes after initialization
2. **Processes share same address space**: No memory isolation
3. **Context switch doesn't update TTBR0**: Scheduler saves/restores all registers EXCEPT TTBR0
4. **Security vulnerability**: Process A can access Process B's memory

### Current Code

```c
// mmu.c - Single global table
static unsigned long *ttbr0_l1_table;

// scheduler.c - Never touches TTBR0
void schedule(void) {
    // Saves x2-x30, sp, pc, pstate
    // MISSING: ttbr0_el1 save/restore
}
```

### Documentation Acknowledges This

From `slopix-memory-layout.md`:
> "Single TTBR0 page table shared globally (no per-process isolation yet)"

### Impact

- **Blocks**: Process memory isolation
- **Consequence**: All processes see same memory, no protection
- **Fix Effort**: Very High (major architectural change)

### Solution

1. Create `mmu_create_process_page_table()` function
2. Add `ttbr0_el1` field to process structure
3. Allocate separate L1/L2 tables per process
4. Context switch must:
   ```asm
   msr ttbr0_el1, <next_process_ttbr0>
   dsb sy
   isb
   ```
5. Clone kernel mappings to each process table

**Roadmap Reference**: Phase 3, Steps 3.1-3.6

---

## BLOCKER #6: MEMORY ALLOCATOR EL0-UNAWARE

**Severity**: MAJOR
**Component**: `pmm.c`
**Location**: Lines 69-96

### Problem

PMM only returns kernel-accessible addresses:

1. **Returns TTBR1 addresses**: After MMU, `pmm_alloc_page()` returns `0xFFFFFF80_XXXX_XXXX`
2. **User processes cannot use these**: EL0 can only access TTBR0 range (`0x0000_XXXX_XXXX`)
3. **No per-process allocation**: Not aware of which process is allocating
4. **No permission tracking**: Cannot mark pages as user vs kernel

### Current Code

```c
void *pmm_alloc_page(void) {
    // ...
    if (kernel_in_higher_half()) {
        result = PHYS_TO_VIRT((void *)phys_addr);  // TTBR1 address
    }
    return result;
}
```

### Impact

- **Blocks**: User heap allocation, user stack allocation
- **Consequence**: Cannot allocate memory for EL0 processes
- **Fix Effort**: Medium (wrapper functions needed)

### Solution

1. Add allocation flags:
   ```c
   #define PMM_KERNEL  0x00
   #define PMM_USER    0x01
   void *pmm_alloc_page_flags(unsigned int flags);
   ```
2. For user allocations:
   - Allocate physical page
   - Map into process TTBR0 at user virtual address
   - Return user virtual address
3. Track page ownership per-process

**Roadmap Reference**: Phase 2, Steps 2.5-2.7

---

## BLOCKER #7: CONTEXT SWITCHING MISSING TTBR0 & SP_EL0

**Severity**: CRITICAL
**Component**: `scheduler.c`, `exceptions.S`
**Location**: scheduler.c:40-204, exceptions.S:28-114

### Problem

Context switch doesn't save/restore critical registers:

1. **TTBR0 not saved**: Process address space not preserved
2. **SP_EL0 not saved**: User stack pointer lost
3. **No EL detection**: Cannot determine if process is EL0 or EL1
4. **Stack frame too small**: Only 34 entries, needs 36 (add TTBR0, SP_EL0)

### Current Context Frame

```c
// 34 registers saved:
[x2, xzr, x3, ..., x30, sp, pc, pstate, x0, x1]

// MISSING:
[ttbr0_el1, sp_el0]
```

### Impact

- **Blocks**: EL0 context switching
- **Consequence**: Wrong address space, corrupted user stack
- **Fix Effort**: Very High (modify exception handler, scheduler)

### Solution

1. Expand context frame to 36 entries
2. Exception handler saves TTBR0:
   ```asm
   mrs x0, ttbr0_el1
   str x0, [sp, #TTBR0_OFFSET]
   ```
3. Exception handler saves SP_EL0:
   ```asm
   mrs x0, sp_el0
   str x0, [sp, #SP_EL0_OFFSET]
   ```
4. Before ERET, restore both:
   ```asm
   ldr x0, [sp, #TTBR0_OFFSET]
   msr ttbr0_el1, x0
   ldr x1, [sp, #SP_EL0_OFFSET]
   msr sp_el0, x1
   dsb sy
   isb
   ```

**Roadmap Reference**: Phase 3 (TTBR0), Phase 6 (SP_EL0)

---

## BLOCKER #8: HARDCODED EL1 ASSUMPTIONS

**Severity**: CRITICAL
**Component**: Multiple files
**Location**: process.h:5, process.c:78, timer.c:27, interrupts.c:39

### Problem

Code assumes all processes run at EL1:

1. **PSTATE hardcoded**: `PSTATE_EL1H_IRQ_ENABLED = 0x5` for all processes
2. **No EL0 process creation**: No `process_create_el0()` function
3. **Timer uses EL0 registers**: Accesses `cntp_cval_el0` which may be restricted
4. **Exception handler EL1-only**: Cannot handle EL0 exceptions

### Specific Issues

```c
// process.h - All processes get EL1h mode
#define PSTATE_EL1H_IRQ_ENABLED 0x5
// MISSING: PSTATE_EL0T_IRQ_ENABLED = 0x0

// process.c - Forces EL1
proc->context.pstate = PSTATE_EL1H_IRQ_ENABLED;
// MISSING: EL0 initialization path
```

### Impact

- **Blocks**: EL0 process creation
- **Consequence**: Cannot create user processes
- **Fix Effort**: Medium (requires new code paths)

### Solution

1. Add EL0 constants:
   ```c
   #define PSTATE_EL0T_IRQ_ENABLED 0x0
   ```
2. Create `process_create_el0()`:
   ```c
   process_t *process_create_el0(...) {
       proc->context.pstate = PSTATE_EL0T_IRQ_ENABLED;
       proc->context.exception_level = 0;
       // Allocate user stack
       // Allocate kernel stack
   }
   ```
3. Update exception handler to detect EL

**Roadmap Reference**: Phase 1 (constants), Phase 4 (EL0 creation)

---

## BLOCKER #9: ERET CANNOT RETURN TO EL0

**Severity**: CRITICAL
**Component**: `exceptions.S`, `interrupts.c`
**Location**: exceptions.S:107-114, interrupts.c:68-69

### Problem

Exception return path doesn't properly restore EL0 context:

1. **SP_EL0 not restored**: ERET doesn't restore SP when returning to EL0
2. **TTBR0 not switched**: May be wrong address space
3. **No syscall handling**: SVC from EL0 just prints and halts

### Current Code

```asm
// exceptions.S - Minimal ERET
ldp x0, x1, [sp], #16
msr elr_el1, x0
msr spsr_el1, x1
eret

// MISSING:
// - Restore SP_EL0
// - Switch TTBR0
// - Synchronization barriers
```

```c
// interrupts.c - SVC just halts
if (ec == 0x15) {  // SVC
    printf("Unexpected SVC\n");
    while (1);  // HANGS!
}
```

### Impact

- **Blocks**: EL0 execution, system calls
- **Consequence**: Cannot return to EL0 after exception
- **Fix Effort**: High (major exception handler rewrite)

### Solution

1. Before ERET to EL0:
   ```asm
   // Restore SP_EL0
   ldr x0, [sp, #SP_EL0_OFFSET]
   msr sp_el0, x0

   // Switch TTBR0
   ldr x1, [sp, #TTBR0_OFFSET]
   msr ttbr0_el1, x1
   dsb sy
   isb

   // Restore SPSR/ELR
   // ERET
   ```
2. Create syscall handler instead of halting

**Roadmap Reference**: Phase 5, Steps 5.2-5.8, Phase 6

---

## BLOCKER #10: NO SYSTEM CALL MECHANISM

**Severity**: CRITICAL
**Component**: Missing (syscall.c not implemented)
**Location**: N/A

### Problem

Complete absence of syscall infrastructure:

1. **No SVC handler**: SVC exceptions cause kernel panic
2. **No syscall table**: No way to define system calls
3. **No dispatcher**: No way to route syscall number to implementation
4. **No argument passing**: No mechanism to read x0-x7 arguments
5. **No return mechanism**: No way to return results in x0

### What's Needed

```c
// syscall.h
#define SYSCALL_WRITE  0
#define SYSCALL_EXIT   1
#define SYSCALL_GETPID 2

// syscall.c
void handle_el0_svc(unsigned long *context, unsigned long esr) {
    unsigned long syscall_num = context[8];  // x8

    switch (syscall_num) {
        case SYSCALL_WRITE:
            sys_write(context);
            break;
        // ...
    }
}

void sys_write(unsigned long *context) {
    unsigned long fd = context[0];     // x0
    char *buf = (char *)context[1];    // x1
    unsigned long count = context[2];  // x2

    // Validate user pointer
    // Perform write

    context[0] = bytes_written;  // Return in x0
}
```

### Impact

- **Blocks**: EL0 <-> EL1 communication
- **Consequence**: User programs cannot interact with kernel
- **Fix Effort**: Very High (requires syscall framework)

### Solution

1. Create `syscall.c` and `syscall.h`
2. Implement SVC exception handler
3. Create syscall dispatcher
4. Implement basic syscalls: write, exit, getpid
5. Add user pointer validation
6. Return results via x0 register

**Roadmap Reference**: Phase 5, Steps 5.4-5.6

---

## SUMMARY TABLE

| # | Blocker | Component | Severity | Fix Effort | Phase |
|---|---------|-----------|----------|------------|-------|
| 1 | EL0 exception routing | exceptions.S | CRITICAL | High | 5 |
| 2 | Process structure missing EL0 fields | process.h | CRITICAL | High | 1 |
| 3 | SP_EL0 vs SP_EL1 confusion | scheduler.c | CRITICAL | Very High | 1, 6 |
| 4 | All pages kernel-only | mmu.c | CRITICAL | High | 2 |
| 5 | Shared global TTBR0 | mmu.c | CRITICAL | Very High | 3 |
| 6 | Memory allocator EL0-unaware | pmm.c | MAJOR | Medium | 2 |
| 7 | Context switching incomplete | exceptions.S | CRITICAL | Very High | 3, 6 |
| 8 | Hardcoded EL1 assumptions | Multiple | CRITICAL | Medium | 1, 4 |
| 9 | ERET cannot return to EL0 | exceptions.S | CRITICAL | High | 5, 6 |
| 10 | No syscall mechanism | Missing | CRITICAL | Very High | 5 |

**Total**: 10 blockers (9 CRITICAL, 1 MAJOR)

---

## DEPENDENCIES BETWEEN BLOCKERS

```
Blocker #2 (Process struct)
  └─> Enables #3 (Dual stacks)
  └─> Enables #8 (EL0 creation)

Blocker #4 (User pages)
  └─> Enables #6 (User allocation)
  └─> Enables #8 (EL0 creation)

Blocker #5 (Per-process TTBR0)
  └─> Depends on #2 (Process struct has ttbr0 field)
  └─> Enables #7 (TTBR0 context switch)

Blocker #8 (EL0 creation)
  └─> Depends on #2 (Process struct)
  └─> Depends on #4 (User pages)
  └─> Depends on #3 (Dual stacks)

Blocker #1, #9, #10 (Exception handling)
  └─> Depends on #2 (Process struct)
  └─> Depends on #7 (Context switching)
  └─> Must be implemented together (Phase 5)
```

---

## RECOMMENDED FIX ORDER

1. **Phase 1**: Fix #2, #3 (Process structure, dual stacks)
2. **Phase 2**: Fix #4, #6 (User pages, allocator)
3. **Phase 3**: Fix #5, #7 (Per-process TTBR0, context switching)
4. **Phase 4**: Fix #8 (EL0 process creation)
5. **Phase 5**: Fix #1, #9, #10 (Exception handling, syscalls)

See `/docs/M6-ROADMAP.md` for detailed implementation plan.

---

## TESTING REQUIREMENTS

Each blocker must have:
- **Unit test**: Verify fix in isolation
- **Integration test**: Verify fix with dependent components
- **Regression test**: Ensure existing functionality intact

**Test Coverage Target**: 100% of new code paths

---

## REFERENCES

1. `/docs/M6-ROADMAP.md` - Detailed fix roadmap
2. `/docs/arm64-userspace-el0.md` - ARM64 EL0 specification
3. ARM Architecture Reference Manual - Exception model
4. Raspberry Pi OS Lesson 5 - User processes

---

**END OF AUDIT**
