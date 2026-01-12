#include "test_framework.h"
#include "../timer.h"

// Test that the timer countdown works and ISTATUS flag gets set
static void test_timer_fires(void) {
    TEST("Timer countdown sets ISTATUS flag");

    // Read initial counter value
    uint64_t start_counter = read_cntvct_el0();
    printf("  [INFO] Initial counter value: %lu\n", start_counter);

    // Disable timer first to ensure clean state
    write_cntv_ctl_el0(0);

    // Set a short timer value (1000 ticks is very fast)
    uint64_t tval = 1000;
    write_cntv_tval_el0(tval);
    printf("  [INFO] Set TVAL to %lu ticks\n", tval);

    // Enable timer with IMASK=1 (prevent IRQ) and ENABLE=1
    uint64_t ctl = CNTV_CTL_ENABLE | CNTV_CTL_IMASK;
    write_cntv_ctl_el0(ctl);
    printf("  [INFO] Timer enabled with IMASK (CTL=0x%lx)\n", ctl);

    // Spin-wait until ISTATUS bit becomes 1 or timeout
    // Timeout after ~10 million iterations to prevent hang
    uint64_t timeout = 10000000;
    uint64_t iterations = 0;
    uint64_t status = 0;

    while (iterations < timeout) {
        status = read_cntv_ctl_el0();
        if (status & CNTV_CTL_ISTATUS) {
            break;
        }
        iterations++;
    }

    uint64_t end_counter = read_cntvct_el0();
    printf("  [INFO] Final counter value: %lu (elapsed: %lu ticks)\n",
           end_counter, end_counter - start_counter);
    printf("  [INFO] Final CTL status: 0x%lx (iterations: %lu)\n", status, iterations);

    // Verify ISTATUS became 1
    ASSERT((status & CNTV_CTL_ISTATUS) != 0, "ISTATUS bit set after timer expired");
    ASSERT(iterations < timeout, "Timer fired before timeout");

    // Disable timer after test
    write_cntv_ctl_el0(0);
    printf("  [INFO] Timer disabled\n");
}

void run_timer_tests(void) {
    TEST_SUITE("Timer Unit Tests");

    test_timer_fires();
}
