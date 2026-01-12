#include "scheduler.h"
#include "process.h"
#include "printf.h"

static process_t *run_queue_head;
static process_t *run_queue_tail;

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

            /* ARM64 AAPCS64 requires SP to be 16-byte aligned.
             * Context frame: 36 * 8 bytes = 288 bytes (16-byte aligned size)
             * Layout matches exceptions.S save order (SP points to lowest address):
             * [sp_el0, ttbr0_el1, x2, xzr, x3, x4, x5, x6, ..., x29, x30, ELR, SPSR, x0, x1]
             */
            unsigned long *next_stack = (unsigned long *)next->context.sp_el1;
            next_stack -= 36;  // Allocate entire frame at once, maintaining alignment

            next_stack[0] = next->context.sp_el0;
            next_stack[1] = next->context.ttbr0_el1;
            next_stack[2] = next->context.x2;
            next_stack[3] = 0;  // xzr
            next_stack[4] = next->context.x3;
            next_stack[5] = next->context.x4;
            next_stack[6] = next->context.x5;
            next_stack[7] = next->context.x6;
            next_stack[8] = next->context.x7;
            next_stack[9] = next->context.x8;
            next_stack[10] = next->context.x9;
            next_stack[11] = next->context.x10;
            next_stack[12] = next->context.x11;
            next_stack[13] = next->context.x12;
            next_stack[14] = next->context.x13;
            next_stack[15] = next->context.x14;
            next_stack[16] = next->context.x15;
            next_stack[17] = next->context.x16;
            next_stack[18] = next->context.x17;
            next_stack[19] = next->context.x18;
            next_stack[20] = next->context.x19;
            next_stack[21] = next->context.x20;
            next_stack[22] = next->context.x21;
            next_stack[23] = next->context.x22;
            next_stack[24] = next->context.x23;
            next_stack[25] = next->context.x24;
            next_stack[26] = next->context.x25;
            next_stack[27] = next->context.x26;
            next_stack[28] = next->context.x27;
            next_stack[29] = next->context.x28;
            next_stack[30] = next->context.x29;
            next_stack[31] = next->context.x30;
            next_stack[32] = next->context.pc;      // ELR_EL1
            next_stack[33] = next->context.pstate;  // SPSR_EL1
            next_stack[34] = next->context.x0;
            next_stack[35] = next->context.x1;

            return next_stack;
        }
        return stack_ptr;  // No process to run
    }

    // Save current process context from stack
    // Stack layout (matches exceptions.S): [sp_el0, ttbr0_el1, x2, xzr, x3, x4, ..., x29, x30, ELR, SPSR, x0, x1]
    unsigned long *ctx_ptr = (unsigned long *)stack_ptr;
    ctx_ptr += 2;  // Skip sp_el0 and ttbr0_el1 (loaded directly from system registers)
    current->context.x2 = *ctx_ptr++;
    ctx_ptr++;  // skip xzr
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
    current->context.pc = *ctx_ptr++;      // ELR_EL1
    current->context.pstate = *ctx_ptr++;  // SPSR_EL1
    current->context.x0 = *ctx_ptr++;
    current->context.x1 = *ctx_ptr++;

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

    /* ARM64 AAPCS64 requires SP to be 16-byte aligned.
     * Context frame: 36 * 8 bytes = 288 bytes (16-byte aligned size)
     * Layout matches exceptions.S save order (SP points to lowest address):
     * [sp_el0, ttbr0_el1, x2, xzr, x3, x4, x5, x6, ..., x29, x30, ELR, SPSR, x0, x1]
     */
    unsigned long *next_stack = (unsigned long *)next->context.sp_el1;
    next_stack -= 36;  // Allocate entire frame at once, maintaining alignment

    next_stack[0] = next->context.sp_el0;
    next_stack[1] = next->context.ttbr0_el1;
    next_stack[2] = next->context.x2;
    next_stack[3] = 0;  // xzr
    next_stack[4] = next->context.x3;
    next_stack[5] = next->context.x4;
    next_stack[6] = next->context.x5;
    next_stack[7] = next->context.x6;
    next_stack[8] = next->context.x7;
    next_stack[9] = next->context.x8;
    next_stack[10] = next->context.x9;
    next_stack[11] = next->context.x10;
    next_stack[12] = next->context.x11;
    next_stack[13] = next->context.x12;
    next_stack[14] = next->context.x13;
    next_stack[15] = next->context.x14;
    next_stack[16] = next->context.x15;
    next_stack[17] = next->context.x16;
    next_stack[18] = next->context.x17;
    next_stack[19] = next->context.x18;
    next_stack[20] = next->context.x19;
    next_stack[21] = next->context.x20;
    next_stack[22] = next->context.x21;
    next_stack[23] = next->context.x22;
    next_stack[24] = next->context.x23;
    next_stack[25] = next->context.x24;
    next_stack[26] = next->context.x25;
    next_stack[27] = next->context.x26;
    next_stack[28] = next->context.x27;
    next_stack[29] = next->context.x28;
    next_stack[30] = next->context.x29;
    next_stack[31] = next->context.x30;
    next_stack[32] = next->context.pc;      // ELR_EL1
    next_stack[33] = next->context.pstate;  // SPSR_EL1
    next_stack[34] = next->context.x0;
    next_stack[35] = next->context.x1;

    return next_stack;
}
