#include "test_framework.h"
#include "../process.h"
#include "../scheduler.h"
#include "../printf.h"

// External assembly function for context switching test
extern void test_context_switch(void *context_frame);

// Global variable to track process execution
static volatile int process_executed = 0;

// Dummy function that does nothing - used as process entry point
static void dummy_scheduler_func(void) {
    // Just return - this is for testing process selection
    return;
}

// Test process function that sets the executed flag
static void test_process_func(void) {
    process_executed = 1;
    printf("  [TEST_PROCESS] Process executed successfully!\n");
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
    ASSERT(returned_stack != fake_context, "Scheduler returned different stack pointer");

    // Verify context frame contents
    // The scheduler creates a context frame on the process stack with layout:
    // [sp_el0, ttbr0_el1, x2, xzr, x3, x4, ..., x29, x30, ELR, SPSR, x0, x1]
    unsigned long *ctx_frame = (unsigned long *)returned_stack;

    unsigned long expected_elr = (unsigned long)dummy_scheduler_func;
    unsigned long actual_elr = ctx_frame[32];
    printf("  [INFO] Context frame ELR_EL1 (position 32): expected=0x%lx, actual=0x%lx\n",
           expected_elr, actual_elr);
    ASSERT(actual_elr == expected_elr, "ELR_EL1 matches process entry point");

    unsigned long expected_spsr = PSTATE_EL1H_IRQ_ENABLED;
    unsigned long actual_spsr = ctx_frame[33];
    printf("  [INFO] Context frame SPSR_EL1 (position 33): expected=0x%lx, actual=0x%lx\n",
           expected_spsr, actual_spsr);
    ASSERT(actual_spsr == expected_spsr, "SPSR_EL1 matches expected PSTATE");

    unsigned long expected_lr = (unsigned long)process_exit;
    unsigned long actual_lr = ctx_frame[31];
    printf("  [INFO] Context frame x30/LR (position 31): expected=0x%lx, actual=0x%lx\n",
           expected_lr, actual_lr);
    ASSERT(actual_lr == expected_lr, "x30/LR points to process_exit");

    printf("  [INFO] Process PID=%d was selected and is now running\n", proc->pid);

    // Call scheduler again - with only one process, it should stay on the same one
    scheduler_schedule_with_context(returned_stack);

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

static void test_scheduler_context_execution(void) {
    TEST("Scheduler context execution: manual context switch via trampoline");

    // Reset scheduler to clean state
    scheduler_init();
    process_set_current(0);  // Clear current process for bootstrap test

    // Reset the global flag
    process_executed = 0;

    // Create a test process that will set the executed flag
    process_t *proc = process_create(test_process_func, 4096);
    ASSERT(proc != 0, "Test process created");

    if (!proc) return;

    // Add to scheduler
    scheduler_add(proc);
    ASSERT(proc->state == PROCESS_READY, "Process state is READY after scheduler_add");

    // Create fake context frame (36 unsigned longs)
    // This matches the layout expected by scheduler.c line 45:
    // [sp_el0, ttbr0_el1, x2, xzr, x3, x4, ..., x29, x30, ELR, SPSR, x0, x1]
    unsigned long fake_context[36];
    for (int i = 0; i < 36; i++) {
        fake_context[i] = 0;
    }

    // Call scheduler_schedule_with_context with the fake frame
    // This should bootstrap and select our process, returning a context frame
    void *returned_stack = scheduler_schedule_with_context(fake_context);

    // Verify the scheduler selected our process
    ASSERT(process_get_current() == proc, "Scheduler selected the test process");
    ASSERT(proc->state == PROCESS_RUNNING, "Process state is RUNNING after scheduling");
    ASSERT(returned_stack != 0, "Scheduler returned non-null stack pointer");
    ASSERT(returned_stack != fake_context, "Scheduler returned different stack pointer");

    printf("  [INFO] Scheduler prepared context frame, calling test_context_switch...\n");

    // Call test_context_switch to manually restore context and jump to process
    test_context_switch(returned_stack);

    // After returning from context switch, verify the process executed
    ASSERT(process_executed == 1, "Process executed and set flag");
    ASSERT(process_get_current() == proc, "Current process is still set correctly");

    printf("  [INFO] Process PID=%d executed successfully via manual context switch\n", proc->pid);
}

void run_scheduler_basic_tests(void) {
    TEST_SUITE("Basic Scheduler Functionality");

    test_scheduler_no_processes();
    test_scheduler_single_process_bootstrap();
    test_scheduler_context_execution();
}
