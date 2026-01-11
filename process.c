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
    proc->context.sp = (unsigned long)stack + stack_size;  // Stack grows down
    proc->context.pc = (unsigned long)entry;  // Entry point (will be loaded into ELR_EL1)
    proc->context.pstate = PSTATE_EL1H_IRQ_ENABLED;

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
