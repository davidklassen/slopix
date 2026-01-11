#include "test_framework.h"
#include "../process.h"

// Dummy function needed for process creation
void dummy_fn(void) {
    while (1) {
        // Infinite loop - process would run forever if scheduled
    }
}

void test_process_created_at_el1(void) {
    TEST("New processes are created at EL1 with correct field initialization");

    process_t *proc = process_create(dummy_fn, 1024);
    ASSERT(proc != 0, "Process created successfully");

    if (proc) {
        // Check that exception level is set to EL1
        ASSERT_EQ(proc->context.exception_level, 1);

        // Check that EL0-specific fields are initialized to zero
        ASSERT_EQ(proc->context.sp_el0, 0);
        ASSERT_EQ(proc->context.ttbr0_el1, 0);

        // Verify existing sp field is still properly set (non-zero)
        ASSERT(proc->context.sp != 0, "Process SP field is set (non-zero)");
    }
}

void run_process_context_init_tests(void) {
    TEST_SUITE("Process Context Initialization");

    test_process_created_at_el1();
}
