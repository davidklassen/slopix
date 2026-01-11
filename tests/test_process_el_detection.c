#include "test_framework.h"
#include "../process.h"

// Dummy function declared elsewhere (in test_process_context_init.c)
extern void dummy_fn(void);

void test_el0_detection(void) {
    TEST("EL0 detection correctly identifies EL0 processes");

    process_t proc = {0};  // Zero-initialized process
    proc.context.exception_level = 0;

    ASSERT(process_is_el0(&proc), "process_is_el0() returns true for EL0 process");
    ASSERT(!process_is_el1(&proc), "process_is_el1() returns false for EL0 process");
}

void test_el1_detection(void) {
    TEST("EL1 detection correctly identifies EL1 processes");

    process_t proc = {0};  // Zero-initialized process
    proc.context.exception_level = 1;

    ASSERT(process_is_el1(&proc), "process_is_el1() returns true for EL1 process");
    ASSERT(!process_is_el0(&proc), "process_is_el0() returns false for EL1 process");
}

void test_default_process_is_el1(void) {
    TEST("Newly created processes default to EL1");

    process_t *proc = process_create(dummy_fn, 1024);
    ASSERT(proc != 0, "Process created successfully");

    if (proc) {
        ASSERT(process_is_el1(proc), "process_is_el1() returns true for new process");
        ASSERT(!process_is_el0(proc), "process_is_el0() returns false for new process");
    }
}

void run_process_el_detection_tests(void) {
    TEST_SUITE("Process EL Detection");

    test_el0_detection();
    test_el1_detection();
    test_default_process_is_el1();
}
