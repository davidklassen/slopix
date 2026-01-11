#include "test_framework.h"
#include "../process.h"

void test_pstate_el0_mode(void) {
    TEST("PSTATE_EL0T_IRQ_ENABLED has correct mode bits");

    unsigned long pstate = PSTATE_EL0T_IRQ_ENABLED;
    unsigned int mode = pstate & PSTATE_MODE_MASK;

    ASSERT_EQ(mode, 0x0);
    ASSERT_EQ(PSTATE_EL0T_IRQ_ENABLED, 0x0);
}

void test_pstate_el1_mode(void) {
    TEST("PSTATE_EL1H_IRQ_ENABLED has correct mode bits");

    unsigned long pstate = PSTATE_EL1H_IRQ_ENABLED;
    unsigned int mode = pstate & PSTATE_MODE_MASK;

    ASSERT_EQ(mode, 0x5);
    ASSERT_EQ(PSTATE_EL1H_IRQ_ENABLED, 0x5);
}

void test_pstate_mask_extraction(void) {
    TEST("PSTATE_MODE_MASK correctly extracts mode bits [3:0]");

    // Test that mask extracts low 4 bits from various values
    ASSERT_EQ(0x12345 & PSTATE_MODE_MASK, 0x5);
    ASSERT_EQ(0xABCD0 & PSTATE_MODE_MASK, 0x0);
    ASSERT_EQ(0xFFFF & PSTATE_MODE_MASK, 0xF);
    ASSERT_EQ(0x00008 & PSTATE_MODE_MASK, 0x8);

    // Verify mask value itself
    ASSERT_EQ(PSTATE_MODE_MASK, 0xF);
}

void run_pstate_tests(void) {
    TEST_SUITE("PSTATE Constants");

    test_pstate_el0_mode();
    test_pstate_el1_mode();
    test_pstate_mask_extraction();
}
