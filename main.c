#include "uart.h"
#include "printf.h"
#include "interrupts.h"
#include "pmm.h"
#include "mmu.h"
#include "process.h"
#include "scheduler.h"
#include "kernel_exit.h"
#include "syscall.h"
#include "timer.h"
#include "gic.h"

#ifdef TEST_BUILD
#include "tests/test_framework.h"

extern void run_pmm_tests(void);
extern void run_process_tests(void);
extern void run_mmu_preflight_tests(void);
extern void run_mmu_postflight_tests(void);
extern void run_ttbr1_preflight_tests(void);
extern void run_ttbr1_postflight_tests(void);
extern void run_higher_half_tests(void);
extern void run_sync_exception_tests(void);
extern void run_context_fields_tests(void);
extern void run_process_context_init_tests(void);
extern void run_pstate_tests(void);
extern void run_process_el_detection_tests(void);
extern void run_context_frame_size_tests(void);
extern void run_dual_stack_foundation_tests(void);
extern void run_pte_bit_tests(void);
extern void run_svc_detection_tests(void);
extern void run_syscall_infrastructure_tests(void);
extern void run_syscall_exit_tests(void);
extern void run_el0_process_create_tests(void);
extern void run_el0_hello_tests(void);
extern void run_scheduler_basic_tests(void);
extern void run_timer_tests(void);
extern void demonstrate_el0_execution(void);
#endif

extern void set_ttbr_registers(unsigned long ttbr0, unsigned long ttbr1);

extern void enable_mmu(void);

__attribute__((section(".text.boot")))
void physical_setup(void) {
    uart_init();
    pmm_init();
    mmu_init();
    set_ttbr_registers(mmu_get_ttbr0(), mmu_get_ttbr1());
}

void thread1(void) {
    int count = 0;
    while (1) {
        printf("[Thread 1] Count: %d\n", count++);
        for (volatile int i = 0; i < 1000000; i++);
    }
}

void thread2(void) {
    int count = 0;
    while (1) {
        printf("[Thread 2] Count: %d\n", count++);
        for (volatile int i = 0; i < 1000000; i++);
    }
}

void main(void) {
    process_init();
    scheduler_init();

#ifdef TEST_BUILD
    run_mmu_preflight_tests();
    run_ttbr1_preflight_tests();
#endif

    extern void transition_to_higher_half(void);
    enable_mmu();
    printf("[MMU] MMU enabled\n");
    transition_to_higher_half();
    printf("[MMU] Transitioned to higher-half\n");

#ifdef TEST_BUILD
    run_mmu_postflight_tests();
    run_ttbr1_postflight_tests();
    run_higher_half_tests();

    run_pmm_tests();
    run_process_tests();
    run_context_fields_tests();
    run_process_context_init_tests();
    run_pstate_tests();
    run_process_el_detection_tests();
    run_context_frame_size_tests();
    run_dual_stack_foundation_tests();
    run_pte_bit_tests();

    // Initialize exception handling for SVC tests
    interrupts_init();
    syscall_init();
    run_svc_detection_tests();
    run_syscall_infrastructure_tests();
    run_syscall_exit_tests();
    run_el0_process_create_tests();
    run_el0_hello_tests();
    run_scheduler_basic_tests();
    run_timer_tests();

    print_test_summary();

    // If all tests passed, run actual EL0 demonstration
    if (tests_failed == 0) {
        demonstrate_el0_execution();
        // This function will ERET to EL0 and never return
    }
#endif

    timer_init();
    gic_init();
    printf("[GIC] Initialized\n");
    printf("Run...\n");

    while (1) {
        __asm__ volatile("wfe");
    }
}
