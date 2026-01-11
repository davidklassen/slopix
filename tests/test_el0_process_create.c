#include "test_framework.h"
#include "../process.h"
#include "../printf.h"

static void dummy_user_fn(void) {
    // User code - will be used for testing
}

static void test_el0_process_creation(void) {
    TEST("Create EL0 process with dual stacks");

    process_t *proc = process_create_user(dummy_user_fn, 4096);
    ASSERT(proc != 0, "EL0 process created");

    if (proc) {
        // Verify it's marked as EL0
        ASSERT_EQ(proc->context.exception_level, 0);
        ASSERT_EQ(proc->context.pstate, PSTATE_EL0T_IRQ_ENABLED);

        // Verify dual stacks
        ASSERT(proc->context.sp_el0 != 0, "User stack (SP_EL0) is set");
        ASSERT(proc->context.sp_el1 != 0, "Kernel stack (SP_EL1) is set");
        ASSERT(proc->context.sp_el0 != proc->context.sp_el1, "Stacks are different");

        // Verify helper functions
        ASSERT(process_is_el0(proc), "process_is_el0() returns true");
        ASSERT(!process_is_el1(proc), "process_is_el1() returns false");

        // Verify alignment
        ASSERT_EQ(proc->context.sp_el0 & 0xF, 0);  // 16-byte aligned
        ASSERT_EQ(proc->context.sp_el1 & 0xF, 0);  // 16-byte aligned

        printf("  EL0 process: PID=%d, user_sp=0x%lx, kernel_sp=0x%lx\n",
               proc->pid, proc->context.sp_el0, proc->context.sp_el1);
    }
}

static void test_el0_vs_el1_processes(void) {
    TEST("EL0 and EL1 processes are distinct");

    process_t *el1_proc = process_create(dummy_user_fn, 4096);
    process_t *el0_proc = process_create_user(dummy_user_fn, 4096);

    ASSERT(el1_proc != 0 && el0_proc != 0, "Both processes created");

    // EL1 process
    ASSERT_EQ(el1_proc->context.exception_level, 1);
    ASSERT_EQ(el1_proc->context.pstate, PSTATE_EL1H_IRQ_ENABLED);
    ASSERT_EQ(el1_proc->context.sp_el0, 0);  // No user stack

    // EL0 process
    ASSERT_EQ(el0_proc->context.exception_level, 0);
    ASSERT_EQ(el0_proc->context.pstate, PSTATE_EL0T_IRQ_ENABLED);
    ASSERT(el0_proc->context.sp_el0 != 0, "Has user stack");

    printf("  EL1 process: exception_level=%d, pstate=0x%lx\n",
           el1_proc->context.exception_level, el1_proc->context.pstate);
    printf("  EL0 process: exception_level=%d, pstate=0x%lx\n",
           el0_proc->context.exception_level, el0_proc->context.pstate);
}

void run_el0_process_create_tests(void) {
    TEST_SUITE("EL0 Process Creation");
    test_el0_process_creation();
    test_el0_vs_el1_processes();
}
