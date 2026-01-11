#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include "../printf.h"

// Test statistics (extern - defined in tests/test_globals.c)
extern int tests_run;
extern int tests_passed;
extern int tests_failed;

// Color codes for terminal output (optional, works in QEMU console)
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET "\033[0m"

// Test assertion macro
#define ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("  " COLOR_GREEN "[PASS]" COLOR_RESET " %s\n", message); \
    } else { \
        tests_failed++; \
        printf("  " COLOR_RED "[FAIL]" COLOR_RESET " %s\n", message); \
    } \
} while(0)

// Equality assertion macro
#define ASSERT_EQ(actual, expected) \
    ASSERT((actual) == (expected), #actual " == " #expected)

// Greater-than-or-equal assertion macro
#define ASSERT_GE(actual, expected) \
    ASSERT((actual) >= (expected), #actual " >= " #expected)

// Test suite header
#define TEST_SUITE(name) \
    printf("\n" COLOR_YELLOW "=== Test Suite: %s ===" COLOR_RESET "\n", name)

// Individual test function
#define TEST(name) \
    printf("\n[TEST] %s\n", name)

// Test results summary
static inline void print_test_summary(void) {
    printf("\n" COLOR_YELLOW "=== Test Summary ===" COLOR_RESET "\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: " COLOR_GREEN "%d" COLOR_RESET "\n", tests_passed);
    printf("Tests failed: " COLOR_RED "%d" COLOR_RESET "\n", tests_failed);

    if (tests_failed == 0) {
        printf("\n" COLOR_GREEN "ALL TESTS PASSED!" COLOR_RESET "\n");
    } else {
        printf("\n" COLOR_RED "SOME TESTS FAILED!" COLOR_RESET "\n");
    }
}

// Reset test counters (for running multiple test suites)
static inline void reset_test_counters(void) {
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
}

#endif // TEST_FRAMEWORK_H
