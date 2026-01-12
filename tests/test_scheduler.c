#include "test_framework.h"
#include "../process.h"
#include "../scheduler.h"
#include "../printf.h"

// Dummy function that does nothing - used as process entry point
static void dummy_scheduler_func(void) {
    // Just return - this is for testing process selection
    return;
}

static void test_scheduler_single_process_bootstrap(void) {
    TEST("Scheduler bootstrap: select first ready process");

    // Reset scheduler to clean state
    scheduler_init();

    // Create a single test process
    process_t *proc = process_create(dummy_scheduler_func, 4096);
    ASSERT(proc != 0, "Test process created");

    if (!proc) return;

    // Verify initial state
    ASSERT(process_get_current() == 0, "No current process before scheduling");

    // Add to scheduler
    scheduler_add(proc);
    ASSERT(proc->state == PROCESS_READY, "Process state is READY after scheduler_add");

    // Create fake context frame (36 unsigned longs)
    // This matches the layout expected by scheduler.c line 52:
    // [sp_el0, ttbr0_el1, x2, xzr, x3, x4, ..., x29, x30, ELR, SPSR, x0, x1]
    unsigned long fake_context[36];
    for (int i = 0; i < 36; i++) {
        fake_context[i] = 0;
    }

    // Call scheduler_schedule_with_context with the fake frame
    // This should bootstrap and select our process
    void *returned_stack = scheduler_schedule_with_context(fake_context);

    // Verify the scheduler selected our process
    ASSERT(process_get_current() == proc, "Scheduler selected the test process");
    ASSERT(proc->state == PROCESS_RUNNING, "Process state is RUNNING after scheduling");
    ASSERT(returned_stack != 0, "Scheduler returned non-null stack pointer");

    printf("  [INFO] Process PID=%d was selected and is now running\n", proc->pid);

    // Call scheduler again - with only one process, it should stay on the same one
    void *returned_stack2 = scheduler_schedule_with_context(returned_stack);

    ASSERT(process_get_current() == proc, "Scheduler stays on same process (no other process)");
    ASSERT(proc->state == PROCESS_RUNNING, "Process still RUNNING after second schedule");

    printf("  [INFO] Second schedule call stayed on same process (as expected)\n");
}

static void test_scheduler_no_processes(void) {
    TEST("Scheduler with no processes returns original stack pointer");

    // Reset scheduler to clean state
    scheduler_init();

    // Verify no current process
    ASSERT(process_get_current() == 0, "No current process initially");

    // Create fake context frame
    unsigned long fake_context[36];
    for (int i = 0; i < 36; i++) {
        fake_context[i] = 0;
    }

    // Call scheduler with no processes in queue
    void *returned_stack = scheduler_schedule_with_context(fake_context);

    // Should return the same pointer we passed in
    ASSERT(returned_stack == fake_context, "Scheduler returns original stack with no processes");
    ASSERT(process_get_current() == 0, "No current process after scheduling (no processes available)");

    printf("  [INFO] Scheduler correctly handled empty run queue\n");
}

void run_scheduler_basic_tests(void) {
    TEST_SUITE("Basic Scheduler Functionality");

    test_scheduler_no_processes();
    test_scheduler_single_process_bootstrap();
}
