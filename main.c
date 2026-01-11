#include "uart.h"
#include "printf.h"
#include "interrupts.h"
#include "timer.h"
#include "pmm.h"
#include "mmu.h"
#include "process.h"
#include "scheduler.h"

#ifdef TEST_BUILD
#include "tests/test_framework.h"

extern void run_pmm_tests(void);
extern void run_process_tests(void);
extern void run_mmu_preflight_tests(void);
extern void run_mmu_postflight_tests(void);
extern void run_ttbr1_preflight_tests(void);
extern void run_ttbr1_postflight_tests(void);
extern void run_higher_half_tests(void);
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

#ifndef TEST_BUILD
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
#endif

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
    print_test_summary();

#else
    process_t *proc1 = process_create(thread1, 4096);
    process_t *proc2 = process_create(thread2, 4096);

    if (!proc1 || !proc2) {
        printf("[ERROR] Failed to create threads\n");
        while (1) __asm__ volatile("wfe");
    }

    scheduler_add(proc1);
    scheduler_add(proc2);

    interrupts_init();
    timer_init(100);
    interrupts_enable();
    timer_enable_scheduling();
    scheduler_schedule();
#endif

    while (1) {
        __asm__ volatile("wfe");
    }
}
