#include "test_framework.h"
#include "../uart.h"
#include "../printf.h"
#include "../gic.h"
#include "../timer.h"
#include "../interrupts.h"
#include "../pmm.h"
#include "../mmu.h"
#include "../process.h"
#include "../scheduler.h"

// Test statistics (defined here, extern in test_framework.h)
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

// Test suite declarations
void run_pmm_tests(void);
void run_process_tests(void);
void run_mmu_register_tests(void);
void run_mmu_table_tests(void);
void run_mmu_preflight_tests(void);
void run_mmu_postflight_tests(void);

// Assembly function declarations
extern void set_ttbr_registers(unsigned long ttbr0, unsigned long ttbr1);
extern void enable_mmu(void);

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

    // Initialize MMU (page tables only, MMU still disabled)
    printf("[INIT] Initializing MMU page tables...\n");
    mmu_init();
    printf("[INIT] MMU page tables initialized\n");

    // Set TTBR registers
    printf("[INIT] Setting TTBR0_EL1 and TTBR1_EL1...\n");
    set_ttbr_registers(mmu_get_ttbr0(), mmu_get_ttbr1());
    printf("[INIT] TTBR registers set\n");

    // Initialize process management (but don't start scheduler)
    printf("[INIT] Initializing Process Management...\n");
    process_init();
    scheduler_init();
    printf("[INIT] Process management initialized\n");

    printf("\n");

    // === PRE-FLIGHT TESTS ===
    printf("========================================\n");
    printf("  MMU PRE-FLIGHT CHECKS\n");
    printf("========================================\n");
    printf("\n");

    int preflight_failures_before = tests_failed;
    run_mmu_preflight_tests();
    int preflight_failures = tests_failed - preflight_failures_before;

    if (preflight_failures > 0) {
        printf("\n[CRITICAL] Pre-flight checks FAILED!\n");
        printf("[CRITICAL] MMU will NOT be enabled - fix issues first\n");
        printf("\n");
    } else {
        printf("\n[OK] Pre-flight checks PASSED\n");
        printf("\n");

        // === ENABLE MMU ===
        printf("========================================\n");
        printf("  ENABLING MMU...\n");
        printf("========================================\n");
        printf("\n");

        enable_mmu();

        printf("[OK] MMU ENABLED!\n");
        printf("\n");

        // === POST-FLIGHT TESTS ===
        printf("========================================\n");
        printf("  MMU POST-FLIGHT VERIFICATION\n");
        printf("========================================\n");
        printf("\n");

        run_mmu_postflight_tests();
        printf("\n");
    }

    // Run test suites
    run_pmm_tests();
    run_process_tests();
    run_mmu_register_tests();
    run_mmu_table_tests();

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
