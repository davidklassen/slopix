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

    proc->next = 0;
    proc->state = PROCESS_READY;

    if (!run_queue_head) {
        run_queue_head = proc;
        run_queue_tail = proc;
    } else {
        run_queue_tail->next = proc;
        run_queue_tail = proc;
    }

    printf("[SCHEDULER] Added process PID=%d to run queue\n", proc->pid);
}

void scheduler_schedule(void) {
    // Get current process
    process_t *current = process_get_current();

    // Find next ready process (round-robin)
    process_t *next = 0;

    if (current && current->state == PROCESS_RUNNING) {
        // Current process is still running, try to find next in queue
        next = current->next;
        if (!next) {
            next = run_queue_head;
        }

        // Skip terminated processes
        while (next && next->state == PROCESS_TERMINATED) {
            next = next->next;
            if (!next) {
                next = run_queue_head;
            }
            if (next == current) {
                break;  // Wrapped around
            }
        }

        // If we found a different ready process, switch to it
        if (next && next != current && next->state == PROCESS_READY) {
            current->state = PROCESS_READY;
            next->state = PROCESS_RUNNING;
            process_set_current(next);
            switch_context(&current->context, &next->context);
        }
    } else if (!current) {
        // No current process, pick first ready one
        next = run_queue_head;
        while (next && next->state != PROCESS_READY) {
            next = next->next;
        }

        if (next) {
            next->state = PROCESS_RUNNING;
            process_set_current(next);
            // Initial switch - no old context to save
            switch_context(0, &next->context);
        }
    }
}
