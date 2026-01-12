#include "test_framework.h"
#include "../gic.h"
#include "../timer.h"

static void test_gic_enable_irq(void) {
    TEST("Enable timer IRQ and verify it's enabled in distributor");

    // Check that IRQ is initially disabled after gic_init()
    uint32_t initially_enabled = gic_is_irq_enabled(TIMER_IRQ);
    printf("  [INFO] Timer IRQ %d initially enabled: %u\n", TIMER_IRQ, initially_enabled);

    // Enable timer IRQ via timer_enable_irq()
    timer_enable_irq();
    printf("  [INFO] Called timer_enable_irq()\n");

    // Verify IRQ is now enabled
    uint32_t now_enabled = gic_is_irq_enabled(TIMER_IRQ);
    printf("  [INFO] Timer IRQ %d now enabled: %u\n", TIMER_IRQ, now_enabled);

    ASSERT(now_enabled == 1, "Timer IRQ should be enabled in GIC distributor");
}

void run_gic_tests(void) {
    TEST_SUITE("GIC Unit Tests");

    test_gic_enable_irq();
}
