#include "process.h"
#include "pmm.h"
#include "printf.h"
#include "memory.h"

static process_t *current_process = 0;
static int next_pid = 1;

void process_init(void) {
    current_process = 0;
    printf("[PROCESS] Process management initialized\n");
}

process_t *process_create(void (*entry)(void), unsigned long stack_size) {
    // Allocate process structure
    process_t *proc = (process_t *)pmm_alloc_page();
    if (!proc) {
        printf("[PROCESS] Failed to allocate process structure\n");
        return 0;
    }

    // Allocate stack (multiple pages if needed)
    unsigned long num_pages = (stack_size + PAGE_SIZE - 1) / PAGE_SIZE;
    void *stack = 0;
    void **allocated_pages = (void **)pmm_alloc_page();  // Track allocated pages
    unsigned long allocated_count = 0;

    if (!allocated_pages) {
        printf("[PROCESS] Failed to allocate tracking page\n");
        pmm_free_page(proc);
        return 0;
    }

    for (unsigned long i = 0; i < num_pages; i++) {
        void *page = pmm_alloc_page();
        if (!page) {
            printf("[PROCESS] Failed to allocate stack\n");
            // Free all previously allocated stack pages
            for (unsigned long j = 0; j < allocated_count; j++) {
                pmm_free_page(allocated_pages[j]);
            }
            pmm_free_page(allocated_pages);
            pmm_free_page(proc);
            return 0;
        }
        allocated_pages[allocated_count++] = page;
        if (i == 0) {
            stack = page;
        }
    }

    // Free tracking page - no longer needed
    pmm_free_page(allocated_pages);

    // Initialize process structure
    proc->pid = next_pid++;
    proc->state = PROCESS_READY;
    proc->stack = stack;
    proc->stack_size = stack_size;
    proc->next = 0;

    // Initialize context - clear all general purpose registers
    for (int i = 0; i < 31; i++) {
        ((unsigned long *)&proc->context)[i] = 0;
    }

    // Set up initial execution state
    proc->context.x30 = (unsigned long)process_exit;  // Link register (return address)

    /* ARM64 AAPCS64 requires SP to be 16-byte aligned.
     * Context frame: 36 * 8 = 288 bytes (16-byte aligned).
     * Initialize SP at top of stack, 16-byte aligned.
     */
    proc->context.sp_el1 = ((unsigned long)stack + stack_size) & ~0xFUL;

    proc->context.pc = (unsigned long)entry;  // Entry point (will be loaded into ELR_EL1)
    proc->context.pstate = PSTATE_EL1H_IRQ_ENABLED;

    // EL0 fields (unused until M6 userspace)
    proc->context.sp_el0 = 0;
    proc->context.ttbr0_el1 = 0;
    proc->context.exception_level = 1;

    return proc;
}

void process_exit(void) {
    if (current_process) {
        current_process->state = PROCESS_TERMINATED;
        printf("[PROCESS] Process PID=%d exited\n", current_process->pid);
    }
    // Scheduler will pick next process
    while (1) {
        __asm__ volatile("wfe");
    }
}

process_t *process_get_current(void) {
    return current_process;
}

void process_set_current(process_t *proc) {
    current_process = proc;
}

process_t *process_create_user(void (*entry)(void), unsigned long user_stack_size) {
    // Allocate process structure
    process_t *proc = (process_t *)pmm_alloc_page();
    if (!proc) {
        printf("[PROCESS] Failed to allocate process structure\n");
        return 0;
    }

    // Allocate kernel stack (for syscall handling) - 8KB (2 pages)
    void *kernel_stack = pmm_alloc_page();
    if (!kernel_stack) {
        printf("[PROCESS] Failed to allocate kernel stack\n");
        pmm_free_page(proc);
        return 0;
    }
    void *kernel_stack2 = pmm_alloc_page();
    if (!kernel_stack2) {
        printf("[PROCESS] Failed to allocate kernel stack page 2\n");
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

    // Page table not set up yet (will do in later step)
    proc->context.ttbr0_el1 = 0;

    printf("[PROCESS] Created EL0 process PID=%d, user_sp=0x%lx, kernel_sp=0x%lx\n",
           proc->pid, proc->context.sp_el0, proc->context.sp_el1);

    return proc;
}
