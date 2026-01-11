#include "test_framework.h"
#include "../process.h"
#include "../printf.h"

// Dummy function declared elsewhere (in test_process_context_init.c)
extern void dummy_fn(void);

void test_dual_stack_foundation_complete(void) {
    TEST("Dual-stack foundation - all components verified");

    // Create an EL1 process
    process_t *proc = process_create(dummy_fn, 4096);
    ASSERT(proc != 0, "Process created successfully");

    if (proc) {
        // Verify all structure fields are correctly initialized
        ASSERT_EQ(proc->context.exception_level, 1);  // Defaults to EL1
        ASSERT_EQ(proc->context.pstate, PSTATE_EL1H_IRQ_ENABLED);  // EL1h mode
        ASSERT(proc->context.sp_el1 != 0, "Kernel stack (sp_el1) set");
        ASSERT_EQ(proc->context.sp_el0, 0);  // User stack not set (EL1 process)
        ASSERT_EQ(proc->context.ttbr0_el1, 0);  // Page table not set yet (future feature)

        // Verify helper functions work
        ASSERT(process_is_el1(proc), "process_is_el1() returns true");
        ASSERT(!process_is_el0(proc), "process_is_el0() returns false");

        // Verify constants are correct
        ASSERT_EQ(CONTEXT_FRAME_SIZE, 36);
        ASSERT_EQ(CONTEXT_FRAME_BYTES, 288);
        ASSERT_EQ(CONTEXT_FRAME_BYTES % 16, 0);  // Alignment check

        // Success message
        printf("  " COLOR_GREEN "[DUAL-STACK] Foundation verified: PASS" COLOR_RESET "\n");
    }
}

void run_dual_stack_foundation_tests(void) {
    TEST_SUITE("Dual-Stack Foundation Integration");

    test_dual_stack_foundation_complete();
}
