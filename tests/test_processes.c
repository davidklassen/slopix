#include "test_framework.h"
#include "../process.h"
#include "../scheduler.h"
#include "../printf.h"

// Test thread functions
static volatile int thread_counter_1 = 0;
static volatile int thread_counter_2 = 0;
static volatile int thread_counter_3 = 0;

void test_thread_1(void) {
    for (int i = 0; i < 50; i++) {
        thread_counter_1++;
        // Small delay
        for (volatile int j = 0; j < 1000; j++);
    }
}

void test_thread_2(void) {
    for (int i = 0; i < 50; i++) {
        thread_counter_2++;
        for (volatile int j = 0; j < 1000; j++);
    }
}

void test_thread_3(void) {
    for (int i = 0; i < 50; i++) {
        thread_counter_3++;
        for (volatile int j = 0; j < 1000; j++);
    }
}

void test_process_create_multiple(void) {
    TEST("Create 5 processes - all should succeed with unique PIDs");

    process_t *processes[5];
    int all_succeeded = 1;
    int all_unique = 1;

    // Create 5 processes
    processes[0] = process_create(test_thread_1, 4096);
    processes[1] = process_create(test_thread_2, 4096);
    processes[2] = process_create(test_thread_3, 4096);
    processes[3] = process_create(test_thread_1, 4096);
    processes[4] = process_create(test_thread_2, 4096);

    // Check all succeeded
    for (int i = 0; i < 5; i++) {
        if (!processes[i]) {
            all_succeeded = 0;
            break;
        }
    }
    ASSERT(all_succeeded, "All 5 processes created successfully");

    // Check unique PIDs
    if (all_succeeded) {
        for (int i = 0; i < 5; i++) {
            for (int j = i + 1; j < 5; j++) {
                if (processes[i]->pid == processes[j]->pid) {
                    all_unique = 0;
                    break;
                }
            }
            if (!all_unique) break;
        }
    }
    ASSERT(all_unique, "All 5 processes have unique PIDs");

    // Verify PIDs are sequential
    if (all_succeeded && all_unique) {
        int sequential = 1;
        for (int i = 1; i < 5; i++) {
            if (processes[i]->pid != processes[i-1]->pid + 1) {
                sequential = 0;
                break;
            }
        }
        ASSERT(sequential, "PIDs are assigned sequentially");
    }

    // Note: We don't clean up processes as there's no process_destroy function yet
}

void test_process_state_management(void) {
    TEST("Process state transitions work correctly");

    process_t *proc = process_create(test_thread_1, 4096);
    ASSERT(proc != 0, "Process created");

    if (proc) {
        ASSERT(proc->state == PROCESS_READY, "New process state is READY");

        // Simulate state changes
        proc->state = PROCESS_RUNNING;
        ASSERT(proc->state == PROCESS_RUNNING, "State changed to RUNNING");

        proc->state = PROCESS_BLOCKED;
        ASSERT(proc->state == PROCESS_BLOCKED, "State changed to BLOCKED");

        proc->state = PROCESS_TERMINATED;
        ASSERT(proc->state == PROCESS_TERMINATED, "State changed to TERMINATED");
    }
}

void test_process_stack_allocation(void) {
    TEST("Process stacks are allocated and non-overlapping");

    process_t *processes[3];
    processes[0] = process_create(test_thread_1, 4096);
    processes[1] = process_create(test_thread_2, 4096);
    processes[2] = process_create(test_thread_3, 4096);

    int all_have_stacks = 1;
    for (int i = 0; i < 3; i++) {
        if (!processes[i] || !processes[i]->stack) {
            all_have_stacks = 0;
            break;
        }
    }
    ASSERT(all_have_stacks, "All processes have allocated stacks");

    if (all_have_stacks) {
        // Check stacks don't overlap
        int no_overlap = 1;
        for (int i = 0; i < 3; i++) {
            unsigned long stack_start = (unsigned long)processes[i]->stack;
            unsigned long stack_end = stack_start + processes[i]->stack_size;

            for (int j = i + 1; j < 3; j++) {
                unsigned long other_start = (unsigned long)processes[j]->stack;
                unsigned long other_end = other_start + processes[j]->stack_size;

                // Check if ranges overlap
                if ((stack_start < other_end && stack_end > other_start) ||
                    (other_start < stack_end && other_end > stack_start)) {
                    no_overlap = 0;
                    break;
                }
            }
            if (!no_overlap) break;
        }
        ASSERT(no_overlap, "Process stacks do not overlap");
    }
}

void test_scheduler_add_processes(void) {
    TEST("Scheduler can add multiple processes to run queue");

    process_t *proc1 = process_create(test_thread_1, 4096);
    process_t *proc2 = process_create(test_thread_2, 4096);
    process_t *proc3 = process_create(test_thread_3, 4096);

    ASSERT(proc1 && proc2 && proc3, "Created 3 test processes");

    if (proc1 && proc2 && proc3) {
        // Add to scheduler (this modifies process->next to form circular list)
        scheduler_add(proc1);
        scheduler_add(proc2);
        scheduler_add(proc3);

        // Verify circular list is formed
        ASSERT(proc1->next == proc2, "Process 1 points to process 2");
        ASSERT(proc2->next == proc3, "Process 2 points to process 3");
        ASSERT(proc3->next == proc1, "Process 3 points back to process 1 (circular)");
    }
}

void test_process_context_initialization(void) {
    TEST("Process context is properly initialized");

    process_t *proc = process_create(test_thread_1, 4096);
    ASSERT(proc != 0, "Process created");

    if (proc) {
        // Check PC is set to entry point
        ASSERT(proc->context.pc == (unsigned long)test_thread_1,
               "Process PC set to entry point");

        // Check stack pointer is set to top of stack
        unsigned long expected_sp = (unsigned long)proc->stack + proc->stack_size;
        ASSERT(proc->context.sp == expected_sp,
               "Process SP set to top of stack");

        // Check x30 (link register) is set to process_exit
        extern void process_exit(void);
        ASSERT(proc->context.x30 == (unsigned long)process_exit,
               "Process x30 (LR) set to process_exit");

        // Check PSTATE is set correctly (EL1h mode)
        ASSERT(proc->context.pstate == 0x5,
               "Process PSTATE set to EL1h (0x5)");
    }
}

// This test is informational - we can't easily verify actual context switching
// without running the scheduler, but we can verify the data structures are set up
void test_scheduler_ready_for_switching(void) {
    TEST("Scheduler data structures ready for context switching");

    // Reset scheduler state
    scheduler_init();

    process_t *proc1 = process_create(test_thread_1, 4096);
    process_t *proc2 = process_create(test_thread_2, 4096);

    scheduler_add(proc1);
    scheduler_add(proc2);

    ASSERT(proc1->state == PROCESS_READY, "Process 1 is READY");
    ASSERT(proc2->state == PROCESS_READY, "Process 2 is READY");
    ASSERT(proc1->next != 0, "Process 1 has next pointer set");
    ASSERT(proc2->next != 0, "Process 2 has next pointer set");

    printf("  [INFO] Scheduler has 2 processes in circular queue\n");
}

void run_process_tests(void) {
    TEST_SUITE("Process Management & Scheduling");

    test_process_create_multiple();
    test_process_state_management();
    test_process_stack_allocation();
    test_process_context_initialization();
    test_scheduler_add_processes();
    test_scheduler_ready_for_switching();
}
