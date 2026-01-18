# Userspace Roadmap

A detailed plan to move kernel processes to userspace (Milestone 8).

## Goal

Run user processes at EL0 (unprivileged mode) with:
- Per-process address spaces (TTBR0)
- User/kernel memory isolation
- Preemptive scheduling from userspace
- Exception handling for user faults

## Current Infrastructure

### Already Implemented

| Component | Status | Location |
|-----------|--------|----------|
| Trap frame saves SP_EL0/ELR_EL1/SPSR_EL1 | Ready | `vectors.S:32-39` |
| TCR_EL1 configured for dual TTBR | Ready | `mmu.h:33-36` |
| Physical page allocator | Ready | `pmem.c` |
| Process structure with context | Ready | `proc.h` |
| Context switch (callee-saved regs) | Ready | `switch.S` |
| Preemptive scheduling via timer | Ready | `timer.c`, `proc.c` |
| Kernel high-address mapping (TTBR1) | Ready | `mmu.c` |
| ESR_EL1 parsing (EC, ISS) | Ready | `exception.h` |
| SVC exception class detection | Ready | `exception.c:88` |

### Architecture Decisions (Already Made)

- **Address split**: User `0x0000...`, kernel `0xFFFF...` (TCR bits 63:48)
- **Granule**: 4KB pages
- **VA size**: 48-bit (T0SZ=16, T1SZ=16)
- **Kernel mapping**: TTBR1 unchanged on context switch

## Gaps to Fill

| Component | Priority | Effort |
|-----------|----------|--------|
| AP (access permission) bits | High | Small |
| Extended proc structure | High | Small |
| 4KB page table functions | High | Medium |
| Per-process page tables | High | Medium |
| TTBR0 switch on context switch | High | Small |
| Lower EL exception handlers | High | Medium |
| User mode entry/exit | High | Medium |
| TLB invalidation | Medium | Small |
| User stack mapping | Medium | Small |
| Simple user program | Low | Small |

---

## Step 1: Add AP Permission Bits

### Goal

Define page table entry bits that control EL0 access permissions.

### Background

ARM page table entries use AP[2:1] bits to control access:

| AP[2:1] | EL1 | EL0 | Use Case |
|---------|-----|-----|----------|
| 00 | R/W | None | Kernel-only pages |
| 01 | R/W | R/W | User read-write data |
| 10 | R/O | None | Kernel read-only |
| 11 | R/O | R/O | User read-only, shared libs |

Current code sets no AP bits, defaulting to kernel-only access.

### Implementation

Add to `mmu.h`:

```c
// Access Permission bits [7:6]
#define PTE_AP_RW_EL1   (0UL << 6)  // EL1 R/W, EL0 none (kernel only)
#define PTE_AP_RW_ALL   (1UL << 6)  // EL1 R/W, EL0 R/W (user data)
#define PTE_AP_RO_EL1   (2UL << 6)  // EL1 R/O, EL0 none
#define PTE_AP_RO_ALL   (3UL << 6)  // EL1 R/O, EL0 R/O (user code)
```

### Testing

No runtime test needed - these are just constant definitions. Verify compilation succeeds.

### Deliverables

- [x] AP bit macros added to `mmu.h`
- [x] `make test` passes

---

## Step 2: Extend Process Structure

### Goal

Add fields to track user address space and trapframe location.

### Background

Current `struct proc` only has kernel context. For user mode we need:
- `pagetable`: Physical address of user L0 table (loaded into TTBR0)
- `sz`: Size of user memory (for sbrk, memory limits)
- `tf`: Pointer to trapframe on kernel stack (for modifying user registers)

### Implementation

Update `proc.h`:

```c
struct proc {
    enum proc_state state;
    int pid;
    char *kstack;
    struct context ctx;

    // User mode support
    pte_t *pagetable;       // User page table (TTBR0 value)
    unsigned long sz;       // User memory size
    struct trap_frame *tf;  // Trapframe pointer on kstack
};
```

Add include for trap_frame:

```c
#include "exception.h"  // for struct trap_frame
```

### Testing

Add test in `tests/test_proc.c`:

```c
TEST(proc_has_usermode_fields) {
    struct proc *p = proc_alloc();
    ASSERT_NOT_NULL(p, "alloc should succeed");

    // New fields should be zero-initialized
    ASSERT_EQ((unsigned long)p->pagetable, 0, "pagetable should be null");
    ASSERT_EQ(p->sz, 0, "sz should be 0");
    ASSERT_EQ((unsigned long)p->tf, 0, "tf should be null");

    pmem_free(VA_TO_PA(p->kstack));
    p->state = UNUSED;
    return 0;
}
```

### Deliverables

- [x] `struct proc` extended with `pagetable`, `sz`, `tf`
- [x] Fields initialized to 0 in `proc_alloc()`
- [x] Test verifies new fields exist
- [x] `make test` passes

---

## Step 3: Implement 4KB Page Mapping

### Goal

Create functions to map individual 4KB pages (current code only maps 2MB blocks).

### Background

User memory needs fine-grained 4KB mapping for:
- Stack guard pages
- Copy-on-write
- Demand paging
- Proper permission boundaries

4-level page table walk for 4KB granule:
```
VA bits [47:39] -> L0 index (512GB per entry)
VA bits [38:30] -> L1 index (1GB per entry)
VA bits [29:21] -> L2 index (2MB per entry)
VA bits [20:12] -> L3 index (4KB per entry)
VA bits [11:0]  -> page offset
```

### Implementation

Add to `mmu.h`:

```c
// Page sizes
#define PAGE_SIZE_4KB  0x1000UL

// Index extraction for all levels
#define L0_INDEX(va) (((va) >> 39) & 0x1FF)
#define L1_INDEX(va) (((va) >> 30) & 0x1FF)
#define L3_INDEX(va) (((va) >> 12) & 0x1FF)

// Page descriptor (L3 entry) - bit 1 must be 1 for page (not block)
#define PTE_PAGE (1UL << 1)
```

Add to `mmu.c`:

```c
// Create a 4KB page descriptor for user memory
static pte_t make_page_desc_user(paddr_t pa, int write, int exec) {
    pte_t entry = (pa & PTE_ADDR_MASK) | PTE_AF | PTE_SH_INNER |
                  PTE_ATTR_NORMAL | PTE_PAGE | PTE_VALID;

    if (write) {
        entry |= PTE_AP_RW_ALL;  // User R/W
    } else {
        entry |= PTE_AP_RO_ALL;  // User R/O
    }

    if (!exec) {
        entry |= PTE_UXN;  // No user execute
    }
    entry |= PTE_PXN;  // Never kernel execute user pages

    return entry;
}

// Walk page table, allocating intermediate tables as needed
// Returns pointer to L3 entry for given VA, or NULL on failure
static pte_t *walk(pte_t *pagetable, unsigned long va, int alloc) {
    for (int level = 0; level < 3; level++) {
        int idx;
        switch (level) {
        case 0: idx = L0_INDEX(va); break;
        case 1: idx = L1_INDEX(va); break;
        case 2: idx = L2_INDEX(va); break;
        }

        pte_t *pte = &pagetable[idx];
        if (*pte & PTE_VALID) {
            pagetable = (pte_t *)PA_TO_VA(*pte & PTE_ADDR_MASK);
        } else {
            if (!alloc) {
                return 0;
            }
            paddr_t pa = pmem_alloc();
            if (pa == 0) {
                return 0;
            }
            *pte = make_table_desc(pa);
            pagetable = (pte_t *)PA_TO_VA(pa);
        }
    }
    return &pagetable[L3_INDEX(va)];
}

// Map a single 4KB page in user page table
int uvm_map_page(pte_t *pagetable, unsigned long va, paddr_t pa,
                 int write, int exec) {
    pte_t *pte = walk(pagetable, va, 1);
    if (pte == 0) {
        return -1;
    }
    if (*pte & PTE_VALID) {
        return -1;  // Already mapped
    }
    *pte = make_page_desc_user(pa, write, exec);
    return 0;
}
```

### Testing

Add `tests/test_uvm.c`:

```c
TEST(walk_allocates_tables) {
    paddr_t l0_pa = pmem_alloc();
    pte_t *l0 = (pte_t *)PA_TO_VA(l0_pa);

    // Walk should allocate L1, L2, L3 tables
    pte_t *pte = walk(l0, 0x1000, 1);
    ASSERT_NOT_NULL(pte, "walk should return L3 entry");

    // L0[0] should now point to L1
    ASSERT(l0[0] & PTE_VALID, "L0[0] should be valid");

    // Clean up (would need to free intermediate tables)
    return 0;
}

TEST(uvm_map_page_creates_mapping) {
    paddr_t l0_pa = pmem_alloc();
    pte_t *l0 = (pte_t *)PA_TO_VA(l0_pa);

    paddr_t page_pa = pmem_alloc();
    int ret = uvm_map_page(l0, 0x1000, page_pa, 1, 0);
    ASSERT_EQ(ret, 0, "map should succeed");

    pte_t *pte = walk(l0, 0x1000, 0);
    ASSERT_NOT_NULL(pte, "should find mapping");
    ASSERT(*pte & PTE_VALID, "entry should be valid");
    ASSERT((*pte & PTE_ADDR_MASK) == page_pa, "PA should match");

    return 0;
}
```

### Deliverables

- [x] `L0_INDEX`, `L1_INDEX`, `L3_INDEX` macros
- [x] `make_page_desc_user()` function
- [x] `walk()` function
- [x] `uvm_map_page()` function
- [x] Tests for page table walking
- [x] `make test` passes

---

## Step 4: Per-Process Page Table Allocation

### Goal

Create and destroy user address spaces.

### Background

Each process needs its own L0 table for TTBR0. The kernel mappings (TTBR1) remain shared. We need:
- `uvm_create()`: Allocate empty user page table
- `uvm_free()`: Free all user pages and page tables

### Implementation

Add to `mmu.c`:

```c
// Allocate an empty user page table (just L0)
pte_t *uvm_create(void) {
    paddr_t pa = pmem_alloc();
    if (pa == 0) {
        return 0;
    }
    return (pte_t *)PA_TO_VA(pa);
}

// Recursively free page table entries
static void freewalk(pte_t *pagetable, int level) {
    for (int i = 0; i < PTE_PER_TABLE; i++) {
        pte_t entry = pagetable[i];
        if ((entry & PTE_VALID) == 0) {
            continue;
        }

        paddr_t pa = entry & PTE_ADDR_MASK;

        if (level < 3 && (entry & PTE_TABLE)) {
            // This is a table descriptor, recurse
            pte_t *child = (pte_t *)PA_TO_VA(pa);
            freewalk(child, level + 1);
            pmem_free(pa);
        } else if (level == 3) {
            // L3 entry points to actual page
            pmem_free(pa);
        }
        // 2MB block entries at L1/L2 would need different handling
    }
}

// Free a user page table and all its pages
void uvm_free(pte_t *pagetable) {
    if (pagetable == 0) {
        return;
    }
    freewalk(pagetable, 0);
    pmem_free(VA_TO_PA(pagetable));
}
```

Add declarations to `mmu.h`:

```c
pte_t *uvm_create(void);
void uvm_free(pte_t *pagetable);
int uvm_map_page(pte_t *pagetable, unsigned long va, paddr_t pa,
                 int write, int exec);
```

### Testing

```c
TEST(uvm_create_returns_zeroed_table) {
    pte_t *pt = uvm_create();
    ASSERT_NOT_NULL(pt, "should allocate");

    for (int i = 0; i < 512; i++) {
        ASSERT_EQ(pt[i], 0, "entries should be zero");
    }

    uvm_free(pt);
    return 0;
}

TEST(uvm_free_releases_pages) {
    unsigned long before = pmem_free_count();

    pte_t *pt = uvm_create();
    paddr_t page = pmem_alloc();
    uvm_map_page(pt, 0x1000, page, 1, 0);

    // Used: L0 + L1 + L2 + L3 + data page = 5 pages

    uvm_free(pt);

    unsigned long after = pmem_free_count();
    ASSERT_EQ(before, after, "all pages should be freed");

    return 0;
}
```

### Deliverables

- [x] `uvm_create()` function
- [x] `uvm_free()` function with recursive cleanup
- [x] Tests verify allocation and deallocation
- [x] `make test` passes

---

## Step 5: TTBR0 Switch on Context Switch

### Goal

Load the correct user page table when switching to a process.

### Background

When switching from process A to process B:
1. Save A's context (already done)
2. Load B's TTBR0 value
3. Invalidate TLB (stale A mappings)
4. Restore B's context (already done)

### Implementation

Add to `arch.h`:

```c
static inline unsigned long read_ttbr0_el1(void) {
    unsigned long v;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(v));
    return v;
}

static inline void tlbi_vmalle1(void) {
    __asm__ volatile("tlbi vmalle1");
    __asm__ volatile("dsb sy");
    __asm__ volatile("isb");
}
```

Modify scheduler in `proc.c`:

```c
void scheduler(void) {
    for (;;) {
        for (int i = 0; i < NPROC; i++) {
            struct proc *p = &procs[i];
            if (p->state != RUNNABLE) {
                continue;
            }

            current = p;
            p->state = RUNNING;

            // Switch to process page table (if user process)
            if (p->pagetable) {
                write_ttbr0_el1(VA_TO_PA(p->pagetable));
                tlbi_vmalle1();
            }

            context_switch(&sched_ctx, &p->ctx);
            current = 0;
        }
    }
}
```

### Testing

Test requires a user process. For now, verify kernel processes still work:

```c
TEST(scheduler_handles_null_pagetable) {
    // Existing kernel processes have pagetable = NULL
    // Scheduler should not crash
    // This is implicitly tested by existing tests running
    return 0;
}
```

Full test comes after user mode entry works.

### Deliverables

- [x] `read_ttbr0_el1()` accessor
- [x] `tlbi_vmalle1()` TLB invalidation
- [x] Scheduler switches TTBR0 for user processes
- [x] Kernel processes (pagetable=NULL) still work
- [x] `make test` passes

---

## Step 6: Lower EL Exception Handlers

### Goal

Handle exceptions from EL0 (user mode) instead of panicking.

### Background

Current vectors.S has `panic_vector` for Lower EL exceptions. We need to:
1. Handle sync exceptions (syscalls, page faults) from EL0
2. Handle IRQs from EL0 (timer preemption)

Key difference from current EL (SPx) handlers:
- On entry from EL0, hardware automatically selects SP_EL1
- We're already on kernel stack, just need to save trapframe

### Implementation

Modify `vectors.S`:

```asm
// Lower EL using AArch64
vector_entry vec_sync_lower64
    b       sync_lower_entry

vector_entry vec_irq_lower64
    b       irq_lower_entry

// ... keep FIQ and SError as panic ...

// Synchronous exception from EL0
sync_lower_entry:
    save_trap_frame
    mov     x0, sp
    bl      sync_exception_handler_user
    restore_trap_frame
    eret

// IRQ from EL0
irq_lower_entry:
    save_trap_frame
    mov     x0, sp
    bl      irq_handler
    restore_trap_frame
    eret
```

Add user exception handler to `exception.c`:

```c
// Exception class values for lower EL
#define EC_SVC_AARCH64_LOWER  0x15  // Same as current EL
#define EC_IABT_LOWER         0x20  // Instruction abort from lower EL
#define EC_DABT_LOWER         0x24  // Data abort from lower EL

void sync_exception_handler_user(struct trap_frame *tf) {
    unsigned long esr = read_esr_el1();
    unsigned int ec = ESR_EC(esr);
    unsigned int iss = ESR_ISS(esr);

    switch (ec) {
    case EC_SVC_AARCH64:
        // System call
        kprintf("SYSCALL #%d from user at 0x%lx\n", iss & 0xFFFF, tf->elr);
        // TODO: dispatch to syscall handler
        break;

    case EC_IABT_LOWER: {
        unsigned int fsc = iss & FSC_MASK;
        kprintf("USER INSTRUCTION ABORT at 0x%lx\n", tf->elr);
        kprintf("  FAR: 0x%lx, Fault: %s\n",
                read_far_el1(), fault_status_string(fsc));
        // TODO: kill process or handle page fault
        // For now, just halt
        for (;;) wfi();
        break;
    }

    case EC_DABT_LOWER: {
        unsigned int fsc = iss & FSC_MASK;
        const char *op = (iss & ISS_WNR) ? "write" : "read";
        kprintf("USER DATA ABORT (%s) at 0x%lx\n", op, tf->elr);
        kprintf("  FAR: 0x%lx, Fault: %s\n",
                read_far_el1(), fault_status_string(fsc));
        // TODO: kill process
        for (;;) wfi();
        break;
    }

    default:
        kprintf("UNHANDLED USER EXCEPTION: EC=0x%x at 0x%lx\n", ec, tf->elr);
        for (;;) wfi();
    }
}
```

### Testing

Test that kernel mode exceptions still work (existing tests). User exception handling tested in Step 8.

```c
TEST(lower_el_vectors_defined) {
    // Just verify we can reference the vector table
    // Actual test comes when we run user code
    extern char vectors[];
    ASSERT_NOT_NULL(vectors, "vectors should exist");
    return 0;
}
```

### Deliverables

- [ ] `vec_sync_lower64` routes to `sync_lower_entry`
- [ ] `vec_irq_lower64` routes to `irq_lower_entry`
- [ ] `sync_exception_handler_user()` handles syscalls and faults
- [ ] IRQs from user mode handled by existing `irq_handler()`
- [ ] `make test` passes

---

## Step 7: User Mode Entry (First Return to EL0)

### Goal

Create a function to enter user mode for the first time.

### Background

To start a user process:
1. Set up trapframe with user PC (ELR) and SP (SP_EL0)
2. Set SPSR for EL0 return (M=0)
3. Switch to user page table
4. Execute ERET

The trapframe is pre-built on the kernel stack. On "return", we restore it and ERET to user mode.

### Implementation

Add assembly in `vectors.S` or new `usertrap.S`:

```asm
// Return to user mode
// x0 = process pagetable physical address
// sp = points to trapframe
.global usertrap_return
usertrap_return:
    // Disable interrupts during switch
    msr     daifset, #2

    // Switch to user page table
    msr     ttbr0_el1, x0
    isb
    tlbi    vmalle1
    dsb     sy
    isb

    // Restore trapframe and return to EL0
    restore_trap_frame
    eret
```

Add user process creation to `proc.c`:

```c
// Create a user process
int proc_create_user(pte_t *pagetable, unsigned long entry, unsigned long ustack) {
    struct proc *p = proc_alloc();
    if (!p) {
        return -1;
    }

    p->pagetable = pagetable;

    // Set up trapframe at top of kernel stack
    char *sp = p->kstack + PAGE_SIZE;
    sp -= sizeof(struct trap_frame);
    sp = (char *)((unsigned long)sp & ~0xFUL);

    struct trap_frame *tf = (struct trap_frame *)sp;
    p->tf = tf;

    // Clear trapframe
    for (int i = 0; i < 31; i++) {
        tf->regs[i] = 0;
    }
    tf->sp_el0 = ustack;
    tf->elr = entry;
    tf->spsr = 0;  // EL0, AArch64, interrupts enabled

    // Context will "return" to usertrap_first
    p->ctx.x30 = (unsigned long)usertrap_first;
    p->ctx.sp = (unsigned long)tf;

    return p->pid;
}

// First entry to user mode (called via context switch)
static void usertrap_first(void) {
    // Enable interrupts (were disabled by scheduler)
    enable_irq();

    // Return to user mode
    usertrap_return(VA_TO_PA(current->pagetable));
    // Never returns
}
```

### Testing

Requires a working user program. See Step 8.

### Deliverables

- [ ] `usertrap_return` assembly routine
- [ ] `proc_create_user()` function
- [ ] `usertrap_first()` entry point
- [ ] Code compiles without error

---

## Step 8: Simple User Program

### Goal

Run a minimal user program at EL0.

### Background

The simplest user program is an infinite loop. We'll:
1. Compile user code to raw binary
2. Copy it to a user page
3. Map the page at user address 0x0
4. Allocate and map user stack
5. Create user process and run it

### Implementation

Create `user/init.S`:

```asm
// Minimal user program - just loop
.global _user_start
_user_start:
    mov     x0, #0
loop:
    add     x0, x0, #1
    b       loop
```

Or as embedded bytes in kernel (simpler for now):

```c
// user_init_code: infinite loop
// mov x0, #0; add x0, x0, #1; b -4
static const unsigned int user_init_code[] = {
    0xd2800000,  // mov x0, #0
    0x91000400,  // add x0, x0, #1
    0x17ffffff,  // b -4
};
```

Add user process setup to `kernel.c`:

```c
#define USER_BASE  0x0000000000000000UL
#define USER_STACK 0x0000000080000000UL  // 2GB, stack grows down

static void start_user_init(void) {
    // Create user page table
    pte_t *pt = uvm_create();
    if (!pt) {
        kpanic("failed to create user page table");
    }

    // Allocate and map code page
    paddr_t code_pa = pmem_alloc();
    if (code_pa == 0) {
        kpanic("failed to allocate code page");
    }

    // Copy user code
    unsigned int *code = (unsigned int *)PA_TO_VA(code_pa);
    code[0] = 0xd2800000;  // mov x0, #0
    code[1] = 0x91000400;  // add x0, x0, #1
    code[2] = 0x17ffffff;  // b -4

    // Map code page (read-only, executable)
    if (uvm_map_page(pt, USER_BASE, code_pa, 0, 1) < 0) {
        kpanic("failed to map code page");
    }

    // Allocate and map stack page
    paddr_t stack_pa = pmem_alloc();
    if (stack_pa == 0) {
        kpanic("failed to allocate stack page");
    }

    // Map stack page (read-write, not executable)
    unsigned long stack_va = USER_STACK - PAGE_SIZE;
    if (uvm_map_page(pt, stack_va, stack_pa, 1, 0) < 0) {
        kpanic("failed to map stack page");
    }

    // Create user process
    int pid = proc_create_user(pt, USER_BASE, USER_STACK);
    if (pid < 0) {
        kpanic("failed to create user process");
    }

    kprintf("Created user process %d\n", pid);
}
```

Call from `kernel_main()`:

```c
void kernel_main(void) {
    // ... existing init ...

    TEST_REPORT();
    TEST_EXIT();

    start_user_init();  // Create user process

    proc_create(cursor_blink);  // Keep kernel processes for now
    scheduler();
}
```

### Testing

1. **Run and observe**: User process should run without crashing
2. **Timer preemption**: Cursor should still blink (timer IRQ works)
3. **Exception test**: Modify user code to trigger fault:

```c
// This should cause data abort
code[0] = 0xf9400000;  // ldr x0, [x0] - load from address 0
```

Expected output:
```
USER DATA ABORT (read) at 0x0
  FAR: 0x0, Fault: Translation fault, L3
```

### Deliverables

- [ ] User code (loop) embedded or compiled
- [ ] `start_user_init()` creates user process
- [ ] User process runs at EL0
- [ ] Timer interrupts work from EL0
- [ ] Page faults from user mode are caught
- [ ] Kernel processes still work alongside user process

---

## Step 9: Add Syscall for Testing

### Goal

Implement a simple syscall so user can communicate with kernel.

### Background

Once the user process runs, we need syscalls for it to do anything useful. Start with `write()` to print output.

Syscall convention (Linux AArch64):
- x8 = syscall number
- x0-x5 = arguments
- x0 = return value

### Implementation

Define syscall numbers in `syscall.h`:

```c
#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_write  0
#define SYS_exit   1

#endif
```

Add syscall handler in `syscall.c`:

```c
#include "syscall.h"
#include "exception.h"
#include "proc.h"
#include "uart.h"

static long sys_write(int fd, const char *buf, unsigned long len) {
    if (fd != 1) {
        return -1;  // Only stdout for now
    }

    // TODO: validate user pointer
    for (unsigned long i = 0; i < len; i++) {
        uart_putc(buf[i]);
    }
    return len;
}

static long sys_exit(int status) {
    kprintf("Process %d exited with status %d\n", current->pid, status);
    current->state = UNUSED;
    // TODO: free resources
    sched();
    return 0;  // Never reached
}

void syscall(struct trap_frame *tf) {
    long ret = -1;
    unsigned long num = tf->regs[8];  // x8

    switch (num) {
    case SYS_write:
        ret = sys_write(tf->regs[0], (const char *)tf->regs[1], tf->regs[2]);
        break;
    case SYS_exit:
        ret = sys_exit(tf->regs[0]);
        break;
    default:
        kprintf("Unknown syscall %lu\n", num);
    }

    tf->regs[0] = ret;  // Return value in x0
}
```

Modify `sync_exception_handler_user()`:

```c
case EC_SVC_AARCH64:
    syscall(tf);
    break;
```

User code with syscall:

```c
// write(1, "Hi\n", 3); exit(0);
static const unsigned int user_init_code[] = {
    // write(1, msg, 3)
    0xd2800020,  // mov x0, #1        (fd = stdout)
    0x10000061,  // adr x1, msg       (buf = msg, PC-relative)
    0xd2800062,  // mov x2, #3        (len = 3)
    0xd2800008,  // mov x8, #0        (SYS_write)
    0xd4000001,  // svc #0

    // exit(0)
    0xd2800000,  // mov x0, #0        (status)
    0xd2800028,  // mov x8, #1        (SYS_exit)
    0xd4000001,  // svc #0

    // msg: "Hi\n"
    0x000a6948,  // "Hi\n\0"
};
```

### Testing

Run kernel and verify:
1. "Hi\n" appears on console (from user process)
2. "Process 1 exited with status 0" appears
3. Kernel continues running (cursor blinks)

### Deliverables

- [ ] `syscall.h` with syscall numbers
- [ ] `syscall.c` with `sys_write()` and `sys_exit()`
- [ ] User code calls syscalls
- [ ] Output appears on console
- [ ] Process exits cleanly

---

## Verification Checklist

After completing all steps:

- [ ] User process runs at EL0 (verified by syscall working)
- [ ] Timer interrupts preempt user process
- [ ] Multiple user processes can run (optional)
- [ ] Page faults from user mode are caught
- [ ] Syscalls work (write, exit)
- [ ] Kernel processes still function
- [ ] All existing tests pass
- [ ] No memory leaks (free count stable after process exit)

## References

- [ARM Exception Model](docs/ARM-Exception-Model/ARM-Exception-Model.md)
- [ARMv8-A Programmer's Guide](docs/ARMv8-A-Programmer-Guide/ARMv8-A-Programmer-Guide.md) Ch 10-12
- [xv6-riscv proc.c](https://github.com/mit-pdos/xv6-riscv/blob/riscv/kernel/proc.c)
- [raspberry-pi-os lesson06](https://s-matyukevich.github.io/raspberry-pi-os/docs/lesson06/rpi-os.html)
- [ARM Learn the Architecture - AArch64 Exception Model](https://developer.arm.com/documentation/102412/latest)
