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
    // Get current process
    process_t *current = process_get_current();

    if (!current) {
        // Bootstrap: no current process, pick first ready one
        process_t *next = run_queue_head;
        if (next && next->state == PROCESS_READY) {
            next->state = PROCESS_RUNNING;
            process_set_current(next);
            switch_context(0, &next->context);
        }
        return;
    }

    // Find next process in circular list (simple round-robin)
    process_t *next = current->next;

    // If only one process in queue, don't switch to self
    if (next == current) {
        return;
    }

    // Skip terminated processes
    int count = 0;
    while (next->state == PROCESS_TERMINATED && count < 10) {
        next = next->next;
        count++;
    }

    // Switch to next process if it's different and not terminated
    if (next != current && next->state != PROCESS_TERMINATED) {
        current->state = PROCESS_READY;
        next->state = PROCESS_RUNNING;
        process_set_current(next);
        switch_context(&current->context, &next->context);
    }
}
