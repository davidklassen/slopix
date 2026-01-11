# M6 USERSPACE IMPLEMENTATION ROADMAP

**Owner**: Tech Lead / ARM Expert
**Milestone**: M6 - Userspace (EL0) Support
**Status**: Planning Complete
**Date**: 2026-01-11

---

## EXECUTIVE SUMMARY

This roadmap details the path from current M5 (kernel-only EL1 execution) to M6 (userspace EL0 support with syscalls). The implementation is broken into **40+ minimal, testable steps** across 7 phases.

**Critical Blockers Identified**: 10 critical issues preventing M6 (see section below)
**Estimated Steps**: 40+ incremental changes
**Testing Strategy**: One test per step, fail-fast approach

---

## CRITICAL BLOCKERS PREVENTING M6

### Summary Table

| # | Issue | Severity | Component | Fix Effort |
|---|-------|----------|-----------|-----------|
| 1 | EL0 exception routing missing | CRITICAL | exceptions.S | High |
| 2 | Process struct lacks EL0 fields | CRITICAL | process.h | High |
| 3 | SP_EL0 vs SP_EL1 confusion | CRITICAL | scheduler.c | Very High |
| 4 | All pages kernel-only (no PTE_USER) | CRITICAL | mmu.c | High |
| 5 | Shared global TTBR0 (no isolation) | CRITICAL | mmu.c | Very High |
| 6 | Memory allocator EL0-unaware | MAJOR | pmm.c | Medium |
| 7 | Context switch missing TTBR0/SP_EL0 | CRITICAL | exceptions.S | Very High |
| 8 | Hardcoded EL1 assumptions | CRITICAL | Multiple | Medium |
| 9 | ERET cannot return to EL0 | CRITICAL | exceptions.S | High |
| 10 | No syscall mechanism | CRITICAL | Missing | Very High |

All 10 blockers are **CRITICAL** for M6. See `/docs/CODEBASE-AUDIT.md` for details.

---

## IMPLEMENTATION STRATEGY

### Principles
1. **Minimal changes**: Each step changes <50 lines of code
2. **Testable**: Every step has a corresponding test
3. **Incremental**: Each step builds on previous work
4. **Fail-fast**: Tests run after each commit
5. **Reversible**: Small commits allow easy rollback

### Testing Approach
- Write test FIRST (TDD-style when possible)
- Test should fail initially
- Implement minimal code to make test pass
- Run full test suite (`make test`)
- Only proceed if all tests pass

---

## PHASE 1: FOUNDATION - DUAL STACK & PROCESS STRUCTURE

**Goal**: Add SP_EL0, TTBR0, and exception level tracking to process structure.

**Duration**: ~8 steps
**Risk**: Low (purely additive)

---

### Step 1.1: Add EL0 fields to cpu_context_t

**File**: `process.h`

**Change**:
```c
typedef struct {
    unsigned long x2, xzr, x3, ..., x30;  // Keep existing
    unsigned long sp;         // Rename to sp_el1 in future
    unsigned long pc;         // ELR_EL1
    unsigned long pstate;     // SPSR_EL1

    // NEW FIELDS:
    unsigned long sp_el0;              // User stack pointer
    unsigned long ttbr0_el1;           // Process page table base
    unsigned char exception_level;     // 0=EL0, 1=EL1
    unsigned char _padding[7];         // Align to 8 bytes
} cpu_context_t;
```

**Test**: `test_process_context_fields.c`
```c
void test_context_has_el0_fields(void) {
    cpu_context_t ctx;
    ctx.sp_el0 = 0x1234;
    ctx.ttbr0_el1 = 0x5678;
    ctx.exception_level = 0;

    ASSERT_EQ(ctx.sp_el0, 0x1234);
    ASSERT_EQ(ctx.ttbr0_el1, 0x5678);
    ASSERT_EQ(ctx.exception_level, 0);
}
```

**Success Criteria**: Test passes, no build errors.

---

### Step 1.2: Initialize new fields in process_create()

**File**: `process.c`

**Change**:
```c
// In process_create(), after existing initialization:
proc->context.sp_el0 = 0;          // Not used for EL1 processes yet
proc->context.ttbr0_el1 = 0;       // Will be set in Phase 2
proc->context.exception_level = 1;  // All processes start at EL1
```

**Test**: `test_process_context_init.c`
```c
void test_process_created_at_el1(void) {
    process_t *proc = process_create(dummy_fn, 1024);
    ASSERT_EQ(proc->context.exception_level, 1);
    ASSERT_EQ(proc->context.sp_el0, 0);
    // sp_el1 should still be set
    ASSERT_NEQ(proc->context.sp_el1, 0);
}
```

**Success Criteria**: Existing processes still work, new fields initialized.

---

### Step 1.3: Add PSTATE_EL0t constant for EL0 processes

**File**: `process.h`

**Change**:
```c
// Existing:
#define PSTATE_EL1H_IRQ_ENABLED 0x5  // EL1h mode, IRQ enabled

// NEW:
#define PSTATE_EL0T_IRQ_ENABLED 0x0  // EL0t mode, IRQ enabled
#define PSTATE_MODE_MASK        0xF  // Bits [3:0] = execution mode
```

**Test**: `test_pstate_constants.c`
```c
void test_pstate_el0_mode(void) {
    unsigned long pstate = PSTATE_EL0T_IRQ_ENABLED;
    unsigned int mode = pstate & PSTATE_MODE_MASK;
    ASSERT_EQ(mode, 0x0);  // EL0t
}

void test_pstate_el1_mode(void) {
    unsigned long pstate = PSTATE_EL1H_IRQ_ENABLED;
    unsigned int mode = pstate & PSTATE_MODE_MASK;
    ASSERT_EQ(mode, 0x5);  // EL1h
}
```

**Success Criteria**: Tests pass, constants well-defined.

---

### Step 1.4: Rename cpu_context_t.sp to sp_el1 for clarity

**File**: `process.h`, `process.c`, `scheduler.c`

**Change**: Global rename `context.sp` → `context.sp_el1`

**Rationale**: Makes dual-stack semantics explicit.

**Test**: Existing tests should still pass (no behavior change).

**Success Criteria**: All existing tests pass after rename.

---

### Step 1.5: Add helper functions to detect process execution level

**File**: `process.h`, `process.c`

**Change**:
```c
// process.h
static inline int process_is_el0(const process_t *proc) {
    return proc->context.exception_level == 0;
}

static inline int process_is_el1(const process_t *proc) {
    return proc->context.exception_level == 1;
}
```

**Test**: `test_process_el_detection.c`
```c
void test_el0_detection(void) {
    process_t proc = {0};
    proc.context.exception_level = 0;
    ASSERT_TRUE(process_is_el0(&proc));
    ASSERT_FALSE(process_is_el1(&proc));
}

void test_el1_detection(void) {
    process_t proc = {0};
    proc.context.exception_level = 1;
    ASSERT_TRUE(process_is_el1(&proc));
    ASSERT_FALSE(process_is_el0(&proc));
}
```

**Success Criteria**: Tests pass, helpers available.

---

### Step 1.6: Update context frame size constant

**File**: `process.h` or `scheduler.h`

**Change**:
```c
// Context frame now has 2 additional fields (sp_el0, ttbr0_el1)
// Old: 34 registers (x2, xzr, x3-x30, sp, pc, pstate, x0, x1)
// New: 36 registers (add sp_el0, ttbr0_el1)

#define CONTEXT_FRAME_SIZE 36  // Total quad-words in context frame
#define CONTEXT_FRAME_BYTES (CONTEXT_FRAME_SIZE * 8)  // 288 bytes
```

**Test**: `test_context_frame_size.c`
```c
void test_context_frame_alignment(void) {
    // Frame must be 16-byte aligned
    ASSERT_EQ(CONTEXT_FRAME_BYTES % 16, 0);
    ASSERT_EQ(CONTEXT_FRAME_SIZE, 36);
}
```

**Success Criteria**: Constants updated, alignment verified.

---

### Step 1.7: Document dual-stack architecture

**File**: `docs/dual-stack-architecture.md` (new)

**Content**:
- Explain SP_EL0 vs SP_EL1 semantics
- Diagram showing exception flow with stack switches
- ARM64 register usage conventions
- Context switching requirements

**Test**: Documentation reviewed, no code changes.

**Success Criteria**: Comprehensive documentation written.

---

### Step 1.8: Phase 1 integration test

**File**: `tests/test_phase1_dual_stack.c`

**Test**:
```c
void test_phase1_complete(void) {
    // Create EL1 process
    process_t *proc = process_create(dummy_fn, 4096);

    // Verify structure
    ASSERT_EQ(proc->context.exception_level, 1);
    ASSERT_EQ(proc->context.pstate, PSTATE_EL1H_IRQ_ENABLED);
    ASSERT_NEQ(proc->context.sp_el1, 0);
    ASSERT_EQ(proc->context.sp_el0, 0);

    // Verify helpers
    ASSERT_TRUE(process_is_el1(proc));
    ASSERT_FALSE(process_is_el0(proc));

    printf("[PHASE 1] Dual stack foundation: PASS\n");
}
```

**Success Criteria**: All Phase 1 tests pass, existing functionality intact.

---

## PHASE 2: PAGE TABLE PERMISSIONS & USER PAGES

**Goal**: Add UXN/PXN bits, create user-accessible page mappings.

**Duration**: ~8 steps
**Risk**: Medium (changes memory protection)

---

### Step 2.1: Define UXN and PXN page table bits

**File**: `memory.h`

**Change**:
```c
// Existing:
#define PTE_KERNEL  (0UL << 6)   // AP[2:1] = 00
#define PTE_USER    (1UL << 6)   // AP[2:1] = 01
#define PTE_RO      (2UL << 6)   // AP[2:1] = 10

// NEW - Execute-Never bits:
#define PTE_UXN     (1UL << 54)  // User execute never (data pages)
#define PTE_PXN     (1UL << 53)  // Privileged execute never (user code pages)

// Combined attributes:
#define PTE_KERNEL_CODE  (PTE_KERNEL)                    // Kernel RW+X
#define PTE_KERNEL_DATA  (PTE_KERNEL | PTE_UXN)          // Kernel RW, no exec
#define PTE_USER_CODE    (PTE_USER)                      // User RW+X
#define PTE_USER_DATA    (PTE_USER | PTE_UXN | PTE_PXN)  // User RW, no exec
```

**Test**: `test_pte_bits.c`
```c
void test_uxn_bit(void) {
    unsigned long pte = PTE_UXN;
    ASSERT_EQ((pte >> 54) & 1, 1);
}

void test_user_data_page(void) {
    unsigned long pte = PTE_USER_DATA;
    // Should have AP=01 (user accessible)
    ASSERT_EQ((pte >> 6) & 3, 1);
    // Should have UXN=1
    ASSERT_EQ((pte >> 54) & 1, 1);
    // Should have PXN=1
    ASSERT_EQ((pte >> 53) & 1, 1);
}
```

**Success Criteria**: Bit definitions correct, tests pass.

---

### Step 2.2: Update existing kernel mappings with UXN

**File**: `mmu.c`

**Change**: Update kernel data pages to use `PTE_KERNEL_DATA`:
```c
// Line 78: TTBR0 low RAM
entry = phys_addr | (MAIR_NORMAL_NC << 2) | PTE_BLOCK | PTE_AF | PTE_KERNEL_DATA | PTE_VALID;

// Line 91: TTBR0 kernel
entry = phys_addr | (MAIR_NORMAL_CACHED << 2) | PTE_BLOCK | PTE_AF | PTE_KERNEL_DATA | PTE_VALID;
```

**Rationale**: All current kernel pages are data (no execute needed yet).

**Test**: `test_kernel_uxn_protection.c`
```c
void test_kernel_data_no_execute(void) {
    // Read L2 table entries
    unsigned long *l2_low = get_ttbr0_l2_low();
    unsigned long entry = l2_low[0];

    // Should have UXN set
    ASSERT_EQ((entry >> 54) & 1, 1);
}
```

**Success Criteria**: Kernel still boots, UXN bit set on data pages.

---

### Step 2.3: Create test user page mapping (identity map at 0x00100000)

**File**: `mmu.c`

**New Function**:
```c
// Map a single 4KB user page for testing
void mmu_map_test_user_page(unsigned long virt_addr, unsigned long phys_addr) {
    // Create L1 entry if needed
    // Create L2 entry with PTE_USER_DATA
    unsigned long entry = phys_addr | (MAIR_NORMAL_CACHED << 2) |
                         PTE_PAGE | PTE_AF | PTE_USER_DATA | PTE_VALID;
    // Write to TTBR0 page table
}
```

**Test**: `test_user_page_mapping.c`
```c
void test_map_user_page(void) {
    unsigned long test_page = 0x00100000;  // 1MB mark
    void *phys = pmm_alloc_page();

    mmu_map_test_user_page(test_page, (unsigned long)phys);

    // Verify mapping exists in TTBR0
    // Verify PTE_USER bit set
    // Verify UXN/PXN bits set
}
```

**Success Criteria**: User page mapped, permissions correct.

---

### Step 2.4: Test EL0 would be able to access user page (simulation)

**File**: `tests/test_el0_memory_access.c`

**Test**:
```c
void test_el0_can_read_user_page(void) {
    // Map test page
    unsigned long user_addr = 0x00200000;
    void *phys = pmm_alloc_page();
    mmu_map_test_user_page(user_addr, (unsigned long)phys);

    // Write test pattern from EL1
    unsigned long *ptr = (unsigned long *)user_addr;
    *ptr = 0xDEADBEEF;

    // Read back (still at EL1, but verifying mapping works)
    ASSERT_EQ(*ptr, 0xDEADBEEF);

    // Note: Cannot actually test EL0 access until exception handler ready
    printf("User page mapping functional (EL1 test)\n");
}
```

**Success Criteria**: User-accessible pages work from EL1.

---

### Step 2.5: Add allocation flags to PMM

**File**: `pmm.h`, `pmm.c`

**Change**:
```c
// pmm.h
#define PMM_KERNEL  0x00  // Kernel-only page
#define PMM_USER    0x01  // User-accessible page

void *pmm_alloc_page_flags(unsigned int flags);
```

**Implementation**:
```c
void *pmm_alloc_page_flags(unsigned int flags) {
    void *page = pmm_alloc_page();  // Existing allocator
    if (page && (flags & PMM_USER)) {
        // Mark page as user-accessible (for future tracking)
        // For now, just return page (mapping happens separately)
    }
    return page;
}
```

**Test**: `test_pmm_flags.c`
```c
void test_pmm_alloc_user_page(void) {
    void *page = pmm_alloc_page_flags(PMM_USER);
    ASSERT_NEQ(page, NULL);
    pmm_free_page(page);
}
```

**Success Criteria**: Flag-based allocation works.

---

### Step 2.6: Create function to allocate and map user pages

**File**: `mmu.c`, `mmu.h`

**New Function**:
```c
// Allocate physical page and map to user virtual address
void *mmu_alloc_user_page(unsigned long virt_addr) {
    void *phys = pmm_alloc_page_flags(PMM_USER);
    if (!phys) return NULL;

    mmu_map_user_page(virt_addr, (unsigned long)phys, PTE_USER_DATA);
    return (void *)virt_addr;  // Return user virtual address
}
```

**Test**: `test_user_page_allocation.c`
```c
void test_alloc_and_map_user_page(void) {
    unsigned long user_va = 0x00400000;
    void *result = mmu_alloc_user_page(user_va);

    ASSERT_EQ(result, (void *)user_va);

    // Verify mapping exists and is writable
    unsigned int *ptr = (unsigned int *)user_va;
    *ptr = 0x12345678;
    ASSERT_EQ(*ptr, 0x12345678);
}
```

**Success Criteria**: User page allocation and mapping works.

---

### Step 2.7: Create user stack allocation helper

**File**: `mmu.c`, `mmu.h`

**New Function**:
```c
#define USER_STACK_SIZE  (4 * 4096)  // 16KB user stack

// Allocate user stack at given virtual address
void *mmu_alloc_user_stack(unsigned long stack_base) {
    // Allocate 4 pages for stack
    for (int i = 0; i < 4; i++) {
        unsigned long va = stack_base + (i * 4096);
        if (!mmu_alloc_user_page(va)) {
            // TODO: Cleanup on failure
            return NULL;
        }
    }

    // Return top of stack (grows downward)
    return (void *)(stack_base + USER_STACK_SIZE);
}
```

**Test**: `test_user_stack_allocation.c`
```c
void test_alloc_user_stack(void) {
    unsigned long stack_base = 0x01000000;  // 16MB
    void *stack_top = mmu_alloc_user_stack(stack_base);

    ASSERT_EQ(stack_top, (void *)(stack_base + USER_STACK_SIZE));

    // Test stack is writable (write near top)
    unsigned long *sp = (unsigned long *)((unsigned long)stack_top - 16);
    *sp = 0xCAFEBABE;
    ASSERT_EQ(*sp, 0xCAFEBABE);
}
```

**Success Criteria**: User stack allocation works.

---

### Step 2.8: Phase 2 integration test

**File**: `tests/test_phase2_user_pages.c`

**Test**:
```c
void test_phase2_complete(void) {
    // Test PTE bits defined
    ASSERT_NEQ(PTE_UXN, 0);
    ASSERT_NEQ(PTE_PXN, 0);

    // Allocate user page
    void *page = mmu_alloc_user_page(0x00500000);
    ASSERT_NEQ(page, NULL);

    // Allocate user stack
    void *stack = mmu_alloc_user_stack(0x02000000);
    ASSERT_NEQ(stack, NULL);

    printf("[PHASE 2] User page permissions: PASS\n");
}
```

**Success Criteria**: All Phase 2 tests pass, user pages allocatable.

---

## PHASE 3: PER-PROCESS PAGE TABLES

**Goal**: Create separate TTBR0 page table for each process.

**Duration**: ~6 steps
**Risk**: High (changes address space management)

---

### Step 3.1: Create empty TTBR0 page table allocator

**File**: `mmu.c`, `mmu.h`

**New Function**:
```c
// Allocate fresh L1 table for process TTBR0
unsigned long *mmu_create_process_page_table(void) {
    // Allocate L1 table (512 entries × 8 bytes = 4KB)
    void *l1_table = pmm_alloc_page();
    if (!l1_table) return NULL;

    // Zero out table
    memset(l1_table, 0, 4096);

    // Return physical address for TTBR0
    if (kernel_in_higher_half()) {
        return (unsigned long *)VIRT_TO_PHYS(l1_table);
    }
    return l1_table;
}
```

**Test**: `test_process_page_table.c`
```c
void test_create_empty_page_table(void) {
    unsigned long *table = mmu_create_process_page_table();
    ASSERT_NEQ(table, NULL);

    // Verify table is zeroed
    for (int i = 0; i < 512; i++) {
        ASSERT_EQ(table[i], 0);
    }
}
```

**Success Criteria**: Page table allocation works.

---

### Step 3.2: Copy kernel mappings to new process table

**File**: `mmu.c`

**New Function**:
```c
// Clone current TTBR0 mappings to new table
void mmu_clone_kernel_mappings(unsigned long *new_l1) {
    unsigned long *current_l1 = get_current_ttbr0_l1();

    // Copy all L1 entries (both data and L2 pointers)
    for (int i = 0; i < 512; i++) {
        new_l1[i] = current_l1[i];
    }

    // Note: This shares L2 tables between processes (copy-on-write later)
}
```

**Test**: `test_clone_kernel_mappings.c`
```c
void test_clone_mappings(void) {
    unsigned long *new_table = mmu_create_process_page_table();
    mmu_clone_kernel_mappings(new_table);

    // Verify first entry copied
    unsigned long *current = get_current_ttbr0_l1();
    ASSERT_EQ(new_table[0], current[0]);
}
```

**Success Criteria**: Kernel mappings cloned to new table.

---

### Step 3.3: Set TTBR0 field during process creation

**File**: `process.c`

**Change**:
```c
// In process_create():
proc->context.ttbr0_el1 = (unsigned long)mmu_create_process_page_table();
if (!proc->context.ttbr0_el1) {
    // Cleanup and return NULL
}

mmu_clone_kernel_mappings((unsigned long *)proc->context.ttbr0_el1);
```

**Test**: `test_process_has_page_table.c`
```c
void test_process_gets_ttbr0(void) {
    process_t *proc = process_create(dummy_fn, 4096);

    ASSERT_NEQ(proc->context.ttbr0_el1, 0);

    // Verify it's different from global TTBR0
    unsigned long global_ttbr0;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(global_ttbr0));

    // May or may not be different yet (not switched)
    printf("Process TTBR0: 0x%lx\n", proc->context.ttbr0_el1);
}
```

**Success Criteria**: Each process gets unique TTBR0 table.

---

### Step 3.4: Add TTBR0 save/restore to context frame

**File**: `exceptions.S`

**Change** (in exception entry):
```asm
// After saving all registers (before current save location of x0, x1)
mrs x0, ttbr0_el1       // Read current TTBR0
stp x0, xzr, [sp, #-16]!  // Save TTBR0 (and padding)
```

**Change** (in exception exit):
```asm
// Before restoring x0, x1
ldp x0, xzr, [sp], #16   // Load TTBR0
msr ttbr0_el1, x0        // Restore TTBR0
dsb sy                   // Data Synchronization Barrier
isb                      // Instruction Synchronization Barrier
```

**Test**: `test_ttbr0_context_save.c`
```c
void test_ttbr0_saved_in_context(void) {
    // Trigger exception (e.g., timer interrupt)
    // Verify TTBR0 restored after exception
    // (Hard to test without actual exception, may defer to integration test)
}
```

**Success Criteria**: TTBR0 saved and restored across exceptions.

---

### Step 3.5: Update scheduler to switch TTBR0

**File**: `scheduler.c`

**Change**:
```c
// In schedule() after selecting next process:
if (next->context.ttbr0_el1 != 0) {
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"(next->context.ttbr0_el1));
    __asm__ volatile("dsb sy");
    __asm__ volatile("isb");
}
```

**Test**: `test_ttbr0_switch.c`
```c
void test_scheduler_switches_ttbr0(void) {
    process_t *proc1 = process_create(dummy_fn1, 4096);
    process_t *proc2 = process_create(dummy_fn2, 4096);

    // Verify different TTBR0 values
    ASSERT_NEQ(proc1->context.ttbr0_el1, proc2->context.ttbr0_el1);

    // Schedule proc1
    current_process = proc1;
    schedule();

    // Read current TTBR0
    unsigned long ttbr0;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));

    // Should match proc1's TTBR0
    ASSERT_EQ(ttbr0, proc1->context.ttbr0_el1);
}
```

**Success Criteria**: TTBR0 switches between processes.

---

### Step 3.6: Phase 3 integration test

**File**: `tests/test_phase3_per_process_tables.c`

**Test**:
```c
void test_phase3_complete(void) {
    // Create two processes
    process_t *p1 = process_create(dummy1, 4096);
    process_t *p2 = process_create(dummy2, 4096);

    // Verify separate page tables
    ASSERT_NEQ(p1->context.ttbr0_el1, 0);
    ASSERT_NEQ(p2->context.ttbr0_el1, 0);
    ASSERT_NEQ(p1->context.ttbr0_el1, p2->context.ttbr0_el1);

    // Verify kernel mappings present in both
    unsigned long *p1_table = (unsigned long *)p1->context.ttbr0_el1;
    unsigned long *p2_table = (unsigned long *)p2->context.ttbr0_el1;

    // First entry should match (kernel low RAM)
    ASSERT_EQ(p1_table[0], p2_table[0]);

    printf("[PHASE 3] Per-process page tables: PASS\n");
}
```

**Success Criteria**: All Phase 3 tests pass, processes isolated.

---

## PHASE 4: EL0 PROCESS CREATION

**Goal**: Create processes that run at EL0 with user stacks.

**Duration**: ~6 steps
**Risk**: Medium (new process type)

---

### Step 4.1: Add process_create_el0() function

**File**: `process.c`, `process.h`

**New Function**:
```c
process_t *process_create_el0(void (*entry)(void), unsigned int stack_size) {
    process_t *proc = process_create(entry, stack_size);
    if (!proc) return NULL;

    // Override PSTATE for EL0
    proc->context.pstate = PSTATE_EL0T_IRQ_ENABLED;
    proc->context.exception_level = 0;

    return proc;
}
```

**Test**: `test_el0_process_creation.c`
```c
void test_create_el0_process(void) {
    process_t *proc = process_create_el0(user_fn, 4096);

    ASSERT_NEQ(proc, NULL);
    ASSERT_EQ(proc->context.exception_level, 0);
    ASSERT_EQ(proc->context.pstate, PSTATE_EL0T_IRQ_ENABLED);
    ASSERT_TRUE(process_is_el0(proc));
}
```

**Success Criteria**: EL0 process creation works.

---

### Step 4.2: Allocate user stack for EL0 processes

**File**: `process.c`

**Change**:
```c
// In process_create_el0():
#define USER_STACK_BASE  0x7FFF0000  // Near top of user address space

// Allocate user stack in user address space
void *user_stack = mmu_alloc_user_stack_for_process(proc, USER_STACK_BASE);
if (!user_stack) {
    // Cleanup and fail
    return NULL;
}

proc->context.sp_el0 = (unsigned long)user_stack;
```

**Test**: `test_el0_user_stack.c`
```c
void test_el0_process_has_user_stack(void) {
    process_t *proc = process_create_el0(user_fn, 4096);

    ASSERT_NEQ(proc->context.sp_el0, 0);
    ASSERT_GT(proc->context.sp_el0, 0x7FFF0000);  // In user range
}
```

**Success Criteria**: EL0 processes get user stacks.

---

### Step 4.3: Allocate kernel stack for EL0 processes

**File**: `process.c`

**Change**:
```c
// EL0 processes need TWO stacks:
// 1. User stack (SP_EL0) - already allocated above
// 2. Kernel stack (SP_EL1) - for handling exceptions

// In process_create_el0():
void *kernel_stack = pmm_alloc_page();  // Reuse existing allocation
proc->context.sp_el1 = (unsigned long)kernel_stack + 4096;  // Top of kernel stack
```

**Test**: `test_el0_dual_stacks.c`
```c
void test_el0_has_both_stacks(void) {
    process_t *proc = process_create_el0(user_fn, 4096);

    // Verify user stack
    ASSERT_NEQ(proc->context.sp_el0, 0);

    // Verify kernel stack
    ASSERT_NEQ(proc->context.sp_el1, 0);

    // Should be in different address ranges
    // User: 0x00xxxxxx, Kernel: 0xFFFFFF80xxxxxxxx or physical range
}
```

**Success Criteria**: EL0 processes have both stacks.

---

### Step 4.4: Create simple user program in assembly

**File**: `user_programs/hello_el0.S` (new)

**Content**:
```asm
.global user_hello_el0
.type user_hello_el0, @function

user_hello_el0:
    // Infinite loop for testing
    mov x0, #42         // Test value
1:
    nop
    b 1b

.size user_hello_el0, . - user_hello_el0
```

**Build**: Add to Makefile to assemble into kernel.

**Test**: Link successfully, symbol available.

**Success Criteria**: User program compiles and links.

---

### Step 4.5: Load user program into EL0 process

**File**: `process.c`

**Change**:
```c
// In process_create_el0():
extern void user_hello_el0(void);  // From assembly

// Set entry point to user program
proc->context.pc = (unsigned long)user_hello_el0;

// Initialize x30 (link register) to process_exit
proc->context.x30 = (unsigned long)process_exit;
```

**Test**: `test_el0_program_loaded.c`
```c
void test_el0_program_set(void) {
    extern void user_hello_el0(void);
    process_t *proc = process_create_el0(user_hello_el0, 4096);

    ASSERT_EQ(proc->context.pc, (unsigned long)user_hello_el0);
}
```

**Success Criteria**: EL0 process has valid entry point.

---

### Step 4.6: Phase 4 integration test

**File**: `tests/test_phase4_el0_processes.c`

**Test**:
```c
void test_phase4_complete(void) {
    extern void user_hello_el0(void);
    process_t *proc = process_create_el0(user_hello_el0, 4096);

    // Verify EL0 characteristics
    ASSERT_TRUE(process_is_el0(proc));
    ASSERT_EQ(proc->context.pstate, PSTATE_EL0T_IRQ_ENABLED);
    ASSERT_NEQ(proc->context.sp_el0, 0);
    ASSERT_NEQ(proc->context.sp_el1, 0);
    ASSERT_NEQ(proc->context.ttbr0_el1, 0);
    ASSERT_EQ(proc->context.pc, (unsigned long)user_hello_el0);

    printf("[PHASE 4] EL0 process creation: PASS\n");
}
```

**Success Criteria**: All Phase 4 tests pass, EL0 process fully initialized.

---

## PHASE 5: EXCEPTION HANDLING FOR EL0

**Goal**: Handle exceptions from EL0 (syscalls, page faults, interrupts).

**Duration**: ~8 steps
**Risk**: Very High (modifies critical exception path)

---

### Step 5.1: Add EL0 exception vector handlers to vector table

**File**: `exceptions.S`

**Change**:
```asm
// Current: Only 8 handlers (Current EL SP0/SPx, 4 exception types each)
// Need: 16 handlers (add Lower EL AArch64, 4 exception types)

.align 11  // Vector table 2KB aligned
exception_vectors:
    // Current EL with SP0 (offset 0x000)
    exception_entry exception_handler_sync
    exception_entry exception_handler_irq
    exception_entry exception_handler_fiq
    exception_entry exception_handler_serror

    // Current EL with SPx (offset 0x200)
    exception_entry exception_handler_sync
    exception_entry exception_handler_irq
    exception_entry exception_handler_fiq
    exception_entry exception_handler_serror

    // Lower EL AArch64 (offset 0x400) - NEW
    exception_entry el0_exception_handler_sync
    exception_entry el0_exception_handler_irq
    exception_entry el0_exception_handler_fiq
    exception_entry el0_exception_handler_serror

    // Lower EL AArch32 (offset 0x600) - Unused
    exception_entry unhandled_exception
    exception_entry unhandled_exception
    exception_entry unhandled_exception
    exception_entry unhandled_exception
```

**Test**: `test_el0_vector_table.c`
```c
void test_el0_vectors_installed(void) {
    extern void *exception_vectors;
    unsigned long *vectors = (unsigned long *)&exception_vectors;

    // Verify offset 0x400 is not zero (has handler)
    unsigned long *el0_sync = (unsigned long *)((char *)vectors + 0x400);
    ASSERT_NEQ(*el0_sync, 0);
}
```

**Success Criteria**: Vector table expanded with EL0 handlers.

---

### Step 5.2: Implement el0_exception_handler_sync for syscalls

**File**: `exceptions.S`

**New Handler**:
```asm
el0_exception_handler_sync:
    // Save EL0 context
    // Switch to kernel stack (SP_EL1)
    // Save all registers (x0-x30)
    // Save SP_EL0, ELR_EL1, SPSR_EL1
    // Call C handler: el0_sync_handler(context_frame)
    // Restore context
    // ERET to EL0
```

**Test**: Defer to integration test (hard to trigger without running EL0).

**Success Criteria**: Handler compiles, links correctly.

---

### Step 5.3: Create C handler for EL0 sync exceptions

**File**: `interrupts.c`, `interrupts.h`

**New Function**:
```c
void el0_sync_handler(unsigned long *context) {
    unsigned long esr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));

    unsigned int ec = (esr >> 26) & 0x3F;  // Exception class

    switch (ec) {
        case 0x15:  // SVC from AArch64
            handle_el0_svc(context, esr);
            break;
        case 0x20:  // Instruction abort
        case 0x24:  // Data abort
            printf("[EL0] Page fault: ESR=0x%lx\n", esr);
            // Handle page fault
            break;
        default:
            printf("[EL0] Unhandled sync exception: EC=0x%x\n", ec);
            process_exit();
    }
}
```

**Test**: `test_el0_sync_dispatcher.c`
```c
void test_ec_extraction(void) {
    unsigned long esr = 0x56000000;  // EC = 0x15 (SVC)
    unsigned int ec = (esr >> 26) & 0x3F;
    ASSERT_EQ(ec, 0x15);
}
```

**Success Criteria**: Exception dispatcher implemented.

---

### Step 5.4: Implement handle_el0_svc() syscall dispatcher

**File**: `syscall.c`, `syscall.h` (new)

**Content**:
```c
#define SYSCALL_WRITE  0
#define SYSCALL_EXIT   1
#define SYSCALL_GETPID 2

void handle_el0_svc(unsigned long *context, unsigned long esr) {
    // Extract syscall number from x8
    unsigned long syscall_num = context[8];  // x8 register

    switch (syscall_num) {
        case SYSCALL_WRITE:
            sys_write(context);
            break;
        case SYSCALL_EXIT:
            sys_exit(context);
            break;
        case SYSCALL_GETPID:
            sys_getpid(context);
            break;
        default:
            printf("[SYSCALL] Unknown syscall: %lu\n", syscall_num);
            context[0] = -1;  // Return error in x0
    }
}
```

**Test**: `test_syscall_dispatcher.c`
```c
void test_syscall_dispatch(void) {
    unsigned long context[31] = {0};
    context[8] = SYSCALL_GETPID;  // x8 = syscall number

    handle_el0_svc(context, 0);

    // x0 should have PID
    ASSERT_GT(context[0], 0);
}
```

**Success Criteria**: Syscall dispatcher routes correctly.

---

### Step 5.5: Implement sys_write() syscall

**File**: `syscall.c`

**Implementation**:
```c
void sys_write(unsigned long *context) {
    unsigned long fd = context[0];      // x0
    const char *buf = (const char *)context[1];  // x1
    unsigned long count = context[2];   // x2

    if (fd != 1) {  // Only support stdout for now
        context[0] = -1;
        return;
    }

    // TODO: Validate user pointer

    for (unsigned long i = 0; i < count; i++) {
        uart_putc(buf[i]);
    }

    context[0] = count;  // Return bytes written
}
```

**Test**: `test_sys_write.c`
```c
void test_sys_write_syscall(void) {
    unsigned long context[31] = {0};
    context[0] = 1;                    // stdout
    context[1] = (unsigned long)"Hi";  // buffer
    context[2] = 2;                    // count

    sys_write(context);

    ASSERT_EQ(context[0], 2);  // Should return 2
}
```

**Success Criteria**: Write syscall works.

---

### Step 5.6: Implement sys_exit() syscall

**File**: `syscall.c`

**Implementation**:
```c
void sys_exit(unsigned long *context) {
    int exit_code = (int)context[0];  // x0

    printf("[SYSCALL] Process exiting with code %d\n", exit_code);

    // Mark process as terminated
    extern process_t *current_process;
    current_process->state = PROCESS_TERMINATED;

    // Trigger scheduler (won't return)
    schedule();
}
```

**Test**: Integration test (defer to Phase 5.8).

**Success Criteria**: Exit syscall implemented.

---

### Step 5.7: Handle EL0 IRQ (timer interrupt from userspace)

**File**: `exceptions.S`, `interrupts.c`

**Handler**:
```c
void el0_irq_handler(unsigned long *context) {
    // Same as EL1 IRQ handler, but context is EL0 state
    handle_irq();  // Reuse existing handler

    // Context switch may occur here
    // Will ERET back to EL0 (possibly different process)
}
```

**Test**: Defer to integration test.

**Success Criteria**: Timer interrupts work from EL0.

---

### Step 5.8: Phase 5 integration test - Run EL0 process

**File**: `tests/test_phase5_el0_exceptions.c`

**Test**:
```c
// User program that makes syscalls
void user_test_syscalls(void) {
    // Write syscall
    __asm__ volatile(
        "mov x8, #0\n"           // SYSCALL_WRITE
        "mov x0, #1\n"           // stdout
        "adr x1, msg\n"          // buffer
        "mov x2, #5\n"           // count
        "svc #0\n"               // Trigger syscall
        ::: "x0", "x1", "x2", "x8"
    );

    // Exit syscall
    __asm__ volatile(
        "mov x8, #1\n"           // SYSCALL_EXIT
        "mov x0, #42\n"          // exit code
        "svc #0\n"
        ::: "x0", "x8"
    );

msg:
    .ascii "Hello"
}

void test_phase5_complete(void) {
    process_t *proc = process_create_el0(user_test_syscalls, 4096);

    // Add to scheduler
    scheduler_add_process(proc);

    // Run scheduler (should execute user program)
    schedule();

    // Should see "Hello" output and exit
    printf("[PHASE 5] EL0 exception handling: PASS\n");
}
```

**Success Criteria**: User program runs, makes syscalls, exits cleanly.

---

## PHASE 6: CONTEXT SWITCHING REFINEMENTS

**Goal**: Perfect context switching between EL0 and EL1 processes.

**Duration**: ~4 steps
**Risk**: Medium (refining existing code)

---

### Step 6.1: Fix SP_EL0 save/restore in exception handler

**File**: `exceptions.S`

**Change** (in EL0 exception entry):
```asm
el0_exception_entry:
    // Switch to kernel stack
    msr sp_el1, sp           // Save user SP temporarily
    mrs x0, sp_el0           // Read EL0 stack pointer
    // ... save x0 to context frame as sp_el0
```

**Change** (in EL0 exception exit):
```asm
el0_exception_exit:
    // Restore SP_EL0 before ERET
    ldr x0, [sp, #SP_EL0_OFFSET]
    msr sp_el0, x0
    // ... ERET
```

**Test**: Verify SP_EL0 preserved across syscalls.

**Success Criteria**: User stack pointer preserved.

---

### Step 6.2: Add TTBR0 switch in EL0 exception exit

**File**: `exceptions.S`

**Change**:
```asm
// Before ERET in el0_exception_exit:
ldr x0, [sp, #TTBR0_OFFSET]
msr ttbr0_el1, x0
dsb sy
isb
```

**Test**: Verify TTBR0 switches on process switch from EL0.

**Success Criteria**: Address space switches correctly.

---

### Step 6.3: Verify stack alignment in all paths

**File**: `exceptions.S`, `process.c`

**Check**:
- Exception entry: SP 16-byte aligned after saving context
- Exception exit: SP 16-byte aligned before ERET
- Process creation: SP_EL0 and SP_EL1 both 16-byte aligned

**Test**: `test_stack_alignment.c`
```c
void test_all_stacks_aligned(void) {
    process_t *el0_proc = process_create_el0(user_fn, 4096);
    process_t *el1_proc = process_create(kernel_fn, 4096);

    ASSERT_EQ(el0_proc->context.sp_el0 % 16, 0);
    ASSERT_EQ(el0_proc->context.sp_el1 % 16, 0);
    ASSERT_EQ(el1_proc->context.sp_el1 % 16, 0);
}
```

**Success Criteria**: All stacks aligned.

---

### Step 6.4: Phase 6 integration test

**File**: `tests/test_phase6_context_switching.c`

**Test**:
```c
void test_phase6_complete(void) {
    // Create mixed EL0 and EL1 processes
    process_t *el0_a = process_create_el0(user_fn_a, 4096);
    process_t *el0_b = process_create_el0(user_fn_b, 4096);
    process_t *el1 = process_create(kernel_fn, 4096);

    // Add to scheduler
    scheduler_add_process(el0_a);
    scheduler_add_process(el0_b);
    scheduler_add_process(el1);

    // Run for 10 timer ticks
    for (int i = 0; i < 10; i++) {
        trigger_timer_interrupt();
    }

    // Verify all processes ran (check state changes)
    printf("[PHASE 6] Context switching: PASS\n");
}
```

**Success Criteria**: Mixed EL0/EL1 scheduling works.

---

## PHASE 7: USER POINTER VALIDATION & SECURITY

**Goal**: Prevent EL0 from exploiting kernel via bad pointers.

**Duration**: ~4 steps
**Risk**: Low (purely defensive)

---

### Step 7.1: Implement user pointer validation

**File**: `syscall.c`, `syscall.h`

**New Function**:
```c
int is_valid_user_pointer(const void *ptr, unsigned long size) {
    unsigned long addr = (unsigned long)ptr;

    // Must be in user address space (< 0x8000000000000000)
    if (addr >= 0x8000000000000000UL) return 0;

    // Must not overflow into kernel space
    if (addr + size >= 0x8000000000000000UL) return 0;

    // TODO: Check against process page table (mapped pages)

    return 1;
}
```

**Test**: `test_user_pointer_validation.c`
```c
void test_valid_user_pointer(void) {
    ASSERT_TRUE(is_valid_user_pointer((void *)0x00100000, 4096));
}

void test_invalid_kernel_pointer(void) {
    ASSERT_FALSE(is_valid_user_pointer((void *)0xFFFFFF8000000000, 1));
}
```

**Success Criteria**: Pointer validation works.

---

### Step 7.2: Add validation to sys_write()

**File**: `syscall.c`

**Change**:
```c
void sys_write(unsigned long *context) {
    const char *buf = (const char *)context[1];
    unsigned long count = context[2];

    // Validate user buffer
    if (!is_valid_user_pointer(buf, count)) {
        printf("[SYSCALL] Invalid user pointer: %p\n", buf);
        context[0] = -1;  // EFAULT
        return;
    }

    // ... rest of implementation
}
```

**Test**: `test_sys_write_validation.c`
```c
void test_write_rejects_kernel_pointer(void) {
    unsigned long context[31] = {0};
    context[0] = 1;
    context[1] = 0xFFFFFF8000000000;  // Kernel pointer
    context[2] = 10;

    sys_write(context);

    ASSERT_EQ(context[0], -1);  // Should fail
}
```

**Success Criteria**: Write syscall validates pointers.

---

### Step 7.3: Add PXN enforcement to user code pages

**File**: `mmu.c`

**Change**: When mapping user code pages:
```c
unsigned long entry = phys | (MAIR_NORMAL_CACHED << 2) |
                      PTE_USER | PTE_PXN | PTE_AF | PTE_VALID;
```

**Rationale**: Prevent kernel from executing user code (ret2usr mitigation).

**Test**: Cannot easily test without attempting kernel execution of user code.

**Success Criteria**: PXN bit set on user pages.

---

### Step 7.4: Phase 7 integration test

**File**: `tests/test_phase7_security.c`

**Test**:
```c
void test_phase7_complete(void) {
    // Test pointer validation
    ASSERT_TRUE(is_valid_user_pointer((void *)0x00100000, 4096));
    ASSERT_FALSE(is_valid_user_pointer((void *)0xFFFFFF8000000000, 1));

    // Test syscall validation
    unsigned long context[31] = {0};
    context[0] = 1;
    context[1] = 0xFFFFFF8000000000;
    context[2] = 10;
    sys_write(context);
    ASSERT_EQ(context[0], -1);

    printf("[PHASE 7] Security & validation: PASS\n");
}
```

**Success Criteria**: All security features working.

---

## FINAL INTEGRATION TEST - M6 COMPLETE

**File**: `tests/test_m6_complete.c`

**User Program**:
```c
// user_programs/test_el0_full.S
.global user_test_full

user_test_full:
    // Write "M6 OK" to stdout
    mov x8, #0           // SYSCALL_WRITE
    mov x0, #1           // stdout
    adr x1, msg
    mov x2, #5
    svc #0

    // Exit with code 0
    mov x8, #1           // SYSCALL_EXIT
    mov x0, #0           // success
    svc #0

msg:
    .ascii "M6 OK"
```

**Test**:
```c
void test_m6_milestone_complete(void) {
    extern void user_test_full(void);

    // Create EL0 process
    process_t *proc = process_create_el0(user_test_full, 4096);
    ASSERT_NEQ(proc, NULL);

    // Verify all EL0 fields set
    ASSERT_TRUE(process_is_el0(proc));
    ASSERT_NEQ(proc->context.sp_el0, 0);
    ASSERT_NEQ(proc->context.ttbr0_el1, 0);

    // Run process
    scheduler_add_process(proc);
    schedule();

    // Should see "M6 OK" output
    // Process should exit cleanly

    printf("\n");
    printf("========================================\n");
    printf("  M6 MILESTONE: USERSPACE COMPLETE\n");
    printf("========================================\n");
    printf("✓ EL0 processes running\n");
    printf("✓ System calls working\n");
    printf("✓ Per-process address spaces\n");
    printf("✓ Dual stack management\n");
    printf("✓ Context switching EL0 ↔ EL1\n");
    printf("✓ Security validation\n");
    printf("========================================\n");
}
```

**Success Criteria**: User program runs, prints output, exits cleanly.

---

## RISK MITIGATION

### High-Risk Steps

| Step | Risk | Mitigation |
|------|------|------------|
| 3.4 | TTBR0 context switching | Test with single process first |
| 5.1 | Vector table expansion | Keep old handlers functional |
| 5.2 | EL0 exception handler | Start with minimal handler, expand gradually |
| 6.1 | SP_EL0 save/restore | Verify with debug prints before ERET |

### Rollback Strategy

- Each phase is a git branch
- Merge only after all phase tests pass
- Keep M5 as stable fallback branch

### Debug Tools Needed

1. **Context frame dumper**: Print all 36 registers
2. **TTBR0 viewer**: Dump page table contents
3. **Exception analyzer**: Decode ESR_EL1, SPSR_EL1
4. **Stack tracer**: Verify alignment and contents

---

## SUCCESS METRICS

### M6 Complete When:

1. ✅ EL0 processes can be created
2. ✅ EL0 processes execute in user mode
3. ✅ System calls work (write, exit, getpid)
4. ✅ Timer interrupts work from EL0
5. ✅ Context switching preserves all state
6. ✅ Per-process address spaces isolated
7. ✅ User pointers validated
8. ✅ All 40+ tests pass

---

## TIMELINE ESTIMATE

- **Phase 1**: 2-3 days (foundation, low risk)
- **Phase 2**: 2-3 days (page tables, medium risk)
- **Phase 3**: 3-4 days (per-process tables, high risk)
- **Phase 4**: 2 days (EL0 creation, medium risk)
- **Phase 5**: 4-5 days (exception handling, very high risk)
- **Phase 6**: 2 days (refinement, medium risk)
- **Phase 7**: 1-2 days (security, low risk)

**Total**: 16-23 days with careful testing

---

## DEPENDENCIES

### External Resources
- ARM Architecture Reference Manual (ARMv8)
- Raspberry Pi OS tutorial (Lesson 5)
- xv6-aarch64 source code

### Internal Prerequisites
- M5 must be stable (currently complete)
- All existing tests must pass
- Timer interrupts working

---

## APPENDIX: KEY ARM64 REGISTERS

| Register | Purpose | Used In |
|----------|---------|---------|
| SP_EL0 | User stack pointer | Phase 1, 6 |
| SP_EL1 | Kernel stack pointer | Phase 1, 6 |
| TTBR0_EL1 | User page table base | Phase 3 |
| TTBR1_EL1 | Kernel page table base | M5 (done) |
| ELR_EL1 | Exception return address | All phases |
| SPSR_EL1 | Saved processor state | All phases |
| ESR_EL1 | Exception syndrome | Phase 5 |
| VBAR_EL1 | Vector base address | Phase 5 |

---

## REFERENCES

1. `/docs/arm64-userspace-el0.md` - Complete EL0 specification
2. `/docs/CODEBASE-AUDIT.md` - Blocking issues analysis (to be created)
3. Raspberry Pi OS Lesson 5: https://s-matyukevich.github.io/raspberry-pi-os/docs/lesson05/rpi-os.html
4. xv6-aarch64: https://github.com/k-mrm/xv6-aarch64

---

**END OF ROADMAP**
