#include "scheduler.h"
#include "process.h"
#include "printf.h"

extern void switch_context(cpu_context_t *old_ctx, cpu_context_t *new_ctx);

static process_t *run_queue_head = 0;
static process_t *run_queue_tail = 0;

void scheduler_init(void) {
    run_queue_head = 0;
    run_queue_tail = 0;
    printf("[SCHEDULER] Scheduler initialized\n");
}

void scheduler_add(process_t *proc) {
    if (!proc) return;

    proc->state = PROCESS_READY;

    if (!run_queue_head) {
        run_queue_head = proc;
        run_queue_tail = proc;
        proc->next = proc;  // Single process circular list
    } else {
        proc->next = run_queue_head;  // Make circular
        run_queue_tail->next = proc;
        run_queue_tail = proc;
    }

    printf("[SCHEDULER] Added process PID=%d to run queue\n", proc->pid);
}

void scheduler_schedule(void) {
    // Legacy function - not used with new context switching
    // Kept for compatibility
}

// New scheduler that works with stack-based context switching
// stack_ptr points to saved context on current process's stack
// Returns pointer to stack to restore (may be different process)
void *scheduler_schedule_with_context(void *stack_ptr) {
    process_t *current = process_get_current();

    if (!current) {
        // Bootstrap: no current process yet, pick first ready one
        process_t *next = run_queue_head;
        if (next && next->state == PROCESS_READY) {
            next->state = PROCESS_RUNNING;
            process_set_current(next);

            // Build initial context on next process's stack
            // Stack layout must match exception handler: x0, xzr, x1...x30, ELR, SPSR
            unsigned long *next_stack = (unsigned long *)next->context.sp;

            // Push context in reverse order (stack grows down)
            *(--next_stack) = next->context.pstate;  // SPSR_EL1
            *(--next_stack) = next->context.pc;      // ELR_EL1
            *(--next_stack) = next->context.x30;
            *(--next_stack) = next->context.x29;
            *(--next_stack) = next->context.x28;
            *(--next_stack) = next->context.x27;
            *(--next_stack) = next->context.x26;
            *(--next_stack) = next->context.x25;
            *(--next_stack) = next->context.x24;
            *(--next_stack) = next->context.x23;
            *(--next_stack) = next->context.x22;
            *(--next_stack) = next->context.x21;
            *(--next_stack) = next->context.x20;
            *(--next_stack) = next->context.x19;
            *(--next_stack) = next->context.x18;
            *(--next_stack) = next->context.x17;
            *(--next_stack) = next->context.x16;
            *(--next_stack) = next->context.x15;
            *(--next_stack) = next->context.x14;
            *(--next_stack) = next->context.x13;
            *(--next_stack) = next->context.x12;
            *(--next_stack) = next->context.x11;
            *(--next_stack) = next->context.x10;
            *(--next_stack) = next->context.x9;
            *(--next_stack) = next->context.x8;
            *(--next_stack) = next->context.x7;
            *(--next_stack) = next->context.x6;
            *(--next_stack) = next->context.x5;
            *(--next_stack) = next->context.x4;
            *(--next_stack) = next->context.x3;
            *(--next_stack) = next->context.x2;
            *(--next_stack) = next->context.x1;
            *(--next_stack) = 0;  // xzr
            *(--next_stack) = next->context.x0;

            return next_stack;
        }
        return stack_ptr;  // No process to run
    }

    // Save current process context from stack
    // Stack layout: x0, xzr, x1, x2, ..., x30, ELR, SPSR
    unsigned long *ctx_ptr = (unsigned long *)stack_ptr;
    current->context.x0 = *ctx_ptr++;
    ctx_ptr++;  // skip xzr
    current->context.x1 = *ctx_ptr++;
    current->context.x2 = *ctx_ptr++;
    current->context.x3 = *ctx_ptr++;
    current->context.x4 = *ctx_ptr++;
    current->context.x5 = *ctx_ptr++;
    current->context.x6 = *ctx_ptr++;
    current->context.x7 = *ctx_ptr++;
    current->context.x8 = *ctx_ptr++;
    current->context.x9 = *ctx_ptr++;
    current->context.x10 = *ctx_ptr++;
    current->context.x11 = *ctx_ptr++;
    current->context.x12 = *ctx_ptr++;
    current->context.x13 = *ctx_ptr++;
    current->context.x14 = *ctx_ptr++;
    current->context.x15 = *ctx_ptr++;
    current->context.x16 = *ctx_ptr++;
    current->context.x17 = *ctx_ptr++;
    current->context.x18 = *ctx_ptr++;
    current->context.x19 = *ctx_ptr++;
    current->context.x20 = *ctx_ptr++;
    current->context.x21 = *ctx_ptr++;
    current->context.x22 = *ctx_ptr++;
    current->context.x23 = *ctx_ptr++;
    current->context.x24 = *ctx_ptr++;
    current->context.x25 = *ctx_ptr++;
    current->context.x26 = *ctx_ptr++;
    current->context.x27 = *ctx_ptr++;
    current->context.x28 = *ctx_ptr++;
    current->context.x29 = *ctx_ptr++;
    current->context.x30 = *ctx_ptr++;
    current->context.pc = *ctx_ptr++;
    current->context.pstate = *ctx_ptr++;

    // Find next process in circular list
    process_t *next = current->next;

    // If only one process or next is same, don't switch
    if (next == current) {
        return stack_ptr;  // Return same stack
    }

    // Skip terminated processes
    int count = 0;
    while (next->state == PROCESS_TERMINATED && count < 10) {
        next = next->next;
        count++;
    }

    // If no valid next process, stay with current
    if (!next || next == current || next->state == PROCESS_TERMINATED) {
        return stack_ptr;
    }

    // Switch to next process
    current->state = PROCESS_READY;
    next->state = PROCESS_RUNNING;
    process_set_current(next);

    // Build context on next process's stack
    // Stack layout: x0, xzr, x1...x30, ELR, SPSR
    unsigned long *next_stack = (unsigned long *)next->context.sp;

    *(--next_stack) = next->context.pstate;  // SPSR
    *(--next_stack) = next->context.pc;      // ELR
    *(--next_stack) = next->context.x30;
    *(--next_stack) = next->context.x29;
    *(--next_stack) = next->context.x28;
    *(--next_stack) = next->context.x27;
    *(--next_stack) = next->context.x26;
    *(--next_stack) = next->context.x25;
    *(--next_stack) = next->context.x24;
    *(--next_stack) = next->context.x23;
    *(--next_stack) = next->context.x22;
    *(--next_stack) = next->context.x21;
    *(--next_stack) = next->context.x20;
    *(--next_stack) = next->context.x19;
    *(--next_stack) = next->context.x18;
    *(--next_stack) = next->context.x17;
    *(--next_stack) = next->context.x16;
    *(--next_stack) = next->context.x15;
    *(--next_stack) = next->context.x14;
    *(--next_stack) = next->context.x13;
    *(--next_stack) = next->context.x12;
    *(--next_stack) = next->context.x11;
    *(--next_stack) = next->context.x10;
    *(--next_stack) = next->context.x9;
    *(--next_stack) = next->context.x8;
    *(--next_stack) = next->context.x7;
    *(--next_stack) = next->context.x6;
    *(--next_stack) = next->context.x5;
    *(--next_stack) = next->context.x4;
    *(--next_stack) = next->context.x3;
    *(--next_stack) = next->context.x2;
    *(--next_stack) = next->context.x1;
    *(--next_stack) = 0;  // xzr
    *(--next_stack) = next->context.x0;

    return next_stack;
}
