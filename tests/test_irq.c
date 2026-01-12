#include "test_framework.h"
#include "../timer.h"
#include "../gic.h"
#include "../interrupts.h"
#include "../printf.h"

static void test_timer_irq_fires(void) {
    TEST("Timer IRQ fires and handler is called");

    irq_reset_timer_flag();
    timer_enable_irq();

    write_cntv_ctl_el0(0);
    write_cntv_tval_el0(1000);
    write_cntv_ctl_el0(CNTV_CTL_ENABLE);

    interrupts_enable();

    volatile int timeout = 1000000;
    while (!irq_get_timer_flag() && timeout > 0) {
        timeout--;
    }

    interrupts_disable();
    write_cntv_ctl_el0(0);

    ASSERT(irq_get_timer_flag() == 1, "Timer IRQ handler was called");
    ASSERT(timeout > 0, "Did not timeout waiting for IRQ");
}

static void test_periodic_timer(void) {
    TEST("Periodic timer fires multiple times");

    irq_reset_timer_flag();
    irq_reset_timer_count();

    uint64_t quantum = 10000;
    timer_set_quantum(quantum);
    timer_enable_irq();

    write_cntv_ctl_el0(0);
    write_cntv_tval_el0(quantum);
    write_cntv_ctl_el0(CNTV_CTL_ENABLE);

    interrupts_enable();

    uint32_t target_count = 5;
    volatile int timeout = 10000000;

    while (irq_get_timer_count() < target_count && timeout > 0) {
        timeout--;
    }

    interrupts_disable();
    timer_stop_periodic();

    uint32_t final_count = irq_get_timer_count();

    printf("  [INFO] Timer IRQ count: %u (target: %u)\n", final_count, target_count);

    ASSERT(final_count >= target_count, "Received at least 5 timer interrupts");
    ASSERT(timeout > 0, "Did not timeout waiting for periodic interrupts");
}

void run_irq_tests(void) {
    TEST_SUITE("IRQ Handler Tests");
    test_timer_irq_fires();
    test_periodic_timer();
}
