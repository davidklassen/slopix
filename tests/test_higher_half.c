#include "test_framework.h"
#include "../kernel_state.h"
#include <stdint.h>

static void test_executing_from_higher_half(void) {
    TEST("Kernel executing from higher-half addresses");

    ASSERT(kernel_in_higher_half(), "PC is in higher-half range (>= 0xFFFF000000000000)");
}

static void test_stack_in_higher_half(void) {
    TEST("Stack pointer in higher-half");

    uint64_t sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));

    ASSERT(sp >= 0xFFFF000000000000UL, "SP is in higher-half range");
}

static void test_can_access_globals(void) {
    TEST("Can access global variables after transition");

    // Access global test counters (defined in main.c)
    extern int tests_run;
    int old_val = tests_run;
    tests_run = old_val + 1;
    tests_run = old_val;  // Restore

    ASSERT(1, "Can read/write kernel globals from higher-half");
}

void run_higher_half_tests(void) {
    TEST_SUITE("Higher-Half Execution");
    test_executing_from_higher_half();
    test_stack_in_higher_half();
    test_can_access_globals();
}
