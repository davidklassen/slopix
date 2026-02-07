#include "test.h"
#include "gic.h"
#include "board.h"

#define GICD_REG(off) (*(volatile unsigned int *)(GICD_VA + (off)))
#define GICC_REG(off) (*(volatile unsigned int *)(GICC_VA + (off)))

TEST(gicd_enabled) {
	ASSERT_EQ(GICD_REG(GICD_CTLR_OFF) & 1, 1, "GICD_CTLR enable bit");
	return 0;
}

TEST(gicc_enabled) {
	ASSERT_EQ(GICC_REG(GICC_CTLR_OFF) & 1, 1, "GICC_CTLR enable bit");
	return 0;
}

TEST(gicc_pmr_max_priority) {
	ASSERT_EQ(GICC_REG(GICC_PMR_OFF), 0xFF, "GICC_PMR all priorities");
	return 0;
}

TEST_SUITE(gic) {
	RUN_TEST(gicd_enabled);
	RUN_TEST(gicc_enabled);
	RUN_TEST(gicc_pmr_max_priority);
}
