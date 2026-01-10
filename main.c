#include "uart.h"
#include "printf.h"
#include "interrupts.h"
#include "timer.h"
#include "pmm.h"
#include "mmu.h"
#include "process.h"
#include "scheduler.h"

// Test statistics - needed even in normal builds since test files are always linked
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

#ifdef TEST_BUILD
#include "tests/test_framework.h"

// Test suite declarations
extern void run_pmm_tests(void);
extern void run_process_tests(void);
extern void run_mmu_register_tests(void);
extern void run_mmu_table_tests(void);
extern void run_mmu_preflight_tests(void);
extern void run_mmu_postflight_tests(void);
extern void run_ttbr1_preflight_tests(void);
extern void run_ttbr1_postflight_tests(void);
#endif

// Assembly function declarations
extern void set_ttbr_registers(unsigned long ttbr0, unsigned long ttbr1);
extern void enable_mmu(void);

// Physical setup function - called by boot.S before MMU enable
// Must be in .text.boot section
__attribute__((section(".text.boot")))
void physical_setup(void) {
    // Initialize UART
    uart_init();

    // Initialize Physical Memory Manager
    pmm_init();

    // Initialize MMU page tables
    mmu_init();

    // Set TTBR0 and TTBR1 registers
    set_ttbr_registers(mmu_get_ttbr0(), mmu_get_ttbr1());
}

// Thread functions (used in normal mode)
#ifndef TEST_BUILD
void thread1(void) {
    int count = 0;
    while (1) {
        printf("[Thread 1] Count: %d\n", count++);
        // Busy wait to slow down output
        for (volatile int i = 0; i < 1000000; i++);
    }
}

void thread2(void) {
    int count = 0;
    while (1) {
        printf("[Thread 2] Count: %d\n", count++);
        // Busy wait to slow down output
        for (volatile int i = 0; i < 1000000; i++);
    }
}
#endif

void main(void) {
#ifdef TEST_BUILD
    // ========================================
    // TEST MODE: Run test suites
    // ========================================

    printf("\n");
    printf("========================================\n");
    printf("  SLOPIX TEST SUITE\n");
    printf("========================================\n");
    printf("\n");

    // Initialize process management (for process tests)
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
    run_ttbr1_preflight_tests();
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
        run_ttbr1_postflight_tests();
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
        printf("System is ready for next milestone.\n");
    } else {
        printf("Test suite FAILED - fix issues before proceeding.\n");
    }

    printf("\n[TEST] All tests complete. Halting.\n");

#else
    // ========================================
    // NORMAL MODE: Run two-process workload
    // ========================================

    printf("SLOPIX\n");
    printf("\n=== M3: Memory Management ===\n");
    // PMM already initialized in physical_setup()

    printf("\n=== M4: Processes ===\n");

    // Initialize process management
    process_init();
    scheduler_init();

    // Create two kernel threads
    process_t *proc1 = process_create(thread1, 4096);
    process_t *proc2 = process_create(thread2, 4096);

    if (!proc1 || !proc2) {
        printf("[ERROR] Failed to create threads\n");
        while (1);
    }

    // Add to scheduler
    scheduler_add(proc1);
    scheduler_add(proc2);

    printf("\n=== M2: Interrupts ===\n");
    printf("Initializing interrupts...\n");

    // Initialize interrupt system
    interrupts_init();

    // Initialize timer (100 Hz = 10ms per tick)
    timer_init(100);

    // Enable interrupts
    interrupts_enable();

    printf("Timer and scheduler started\n");
    printf("Two threads will alternate printing...\n\n");

    // Enable timer-driven scheduling
    timer_enable_scheduling();

    // Start the first thread
    scheduler_schedule();
#endif

    // Halt (for test mode) or should never reach here (for normal mode)
    while (1) {
        __asm__ volatile("wfe");
    }
}
