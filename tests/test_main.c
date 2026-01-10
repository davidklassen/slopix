#include "test_framework.h"
#include "../uart.h"
#include "../printf.h"
#include "../gic.h"
#include "../timer.h"
#include "../interrupts.h"
#include "../pmm.h"
#include "../process.h"
#include "../scheduler.h"

// Test statistics (defined here, extern in test_framework.h)
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

// Test suite declarations
void run_pmm_tests(void);
void run_process_tests(void);

void test_main(void) {
    // Initialize UART for output
    uart_init();

    printf("\n");
    printf("========================================\n");
    printf("  SLOPIX TEST SUITE\n");
    printf("========================================\n");
    printf("\n");

    // Initialize physical memory manager
    printf("[INIT] Initializing Physical Memory Manager...\n");
    pmm_init();
    printf("[INIT] PMM initialized: %d pages free\n", (int)pmm_get_free_pages());

    // Initialize process management (but don't start scheduler)
    printf("[INIT] Initializing Process Management...\n");
    process_init();
    scheduler_init();
    printf("[INIT] Process management initialized\n");

    printf("\n");

    // Run test suites
    run_pmm_tests();
    run_process_tests();

    // Print final summary
    printf("\n");
    printf("========================================\n");
    print_test_summary();
    printf("========================================\n");
    printf("\n");

    // Report final result
    if (tests_failed == 0) {
        printf("Test suite completed successfully.\n");
        printf("System is ready for MMU implementation (M5).\n");
    } else {
        printf("Test suite FAILED - fix issues before proceeding.\n");
    }

    printf("\n[TEST] All tests complete. Halting.\n");
}

// Entry point from boot.S
void main(void) {
    test_main();

    // Halt
    while (1) {
        __asm__ volatile("wfe");
    }
}
