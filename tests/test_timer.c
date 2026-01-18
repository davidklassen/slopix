#ifdef RUN_TESTS

#include "test.h"
#include "arch.h"
#include "gic.h"
#include "timer.h"

#define GICC_PMR_REG (*(volatile unsigned int *)(GICC_VIRT + GICC_PMR_OFF))

TEST(arch_irq_mask) {
	disable_irq();
	unsigned long daif1 = read_daif();
	ASSERT(daif1 & DAIF_IRQ_BIT, "IRQ should be masked after disable_irq");

	enable_irq();
	unsigned long daif2 = read_daif();
	ASSERT(!(daif2 & DAIF_IRQ_BIT), "IRQ should be unmasked after enable_irq");

	return 0;
}

TEST(timer_freq) {
	unsigned long freq = read_cntfrq_el0();
	ASSERT_EQ(freq, 62500000, "CNTFRQ_EL0 should be 62.5MHz");
	return 0;
}

TEST(gic_pmr) {
	unsigned int pmr = GICC_PMR_REG;
	ASSERT_EQ(pmr, 0xFF, "GICC_PMR should be 0xFF");
	return 0;
}

TEST(timer_ticks) {
	unsigned long before = timer_get_ticks();

	for (int i = 0; i < 10; i++) {
		wfi();
	}

	unsigned long after = timer_get_ticks();
	ASSERT(after > before, "ticks should increment");
	return 0;
}

TEST_SUITE(timer) {
	RUN_TEST(arch_irq_mask);
	RUN_TEST(timer_freq);
	RUN_TEST(gic_pmr);
	RUN_TEST(timer_ticks);
}

#endif
