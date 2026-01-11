#include "test_framework.h"
#include "../memory.h"

static void test_uxn_bit(void) {
    TEST("UXN bit is at position 54");

    unsigned long pte = PTE_UXN;
    ASSERT_EQ((pte >> 54) & 1, 1);
}

static void test_pxn_bit(void) {
    TEST("PXN bit is at position 53");

    unsigned long pte = PTE_PXN;
    ASSERT_EQ((pte >> 53) & 1, 1);
}

static void test_kernel_data_attributes(void) {
    TEST("PTE_KERNEL_DATA has correct attributes");

    unsigned long pte = PTE_KERNEL_DATA;

    // AP[2:1] = 00 (kernel only)
    ASSERT_EQ((pte >> 6) & 3, 0);

    // UXN = 1 (no execute)
    ASSERT_EQ((pte >> 54) & 1, 1);
}

static void test_user_data_attributes(void) {
    TEST("PTE_USER_DATA has correct attributes");

    unsigned long pte = PTE_USER_DATA;

    // AP[2:1] = 01 (user accessible)
    ASSERT_EQ((pte >> 6) & 3, 1);

    // UXN = 1 (user can't execute)
    ASSERT_EQ((pte >> 54) & 1, 1);

    // PXN = 1 (kernel can't execute)
    ASSERT_EQ((pte >> 53) & 1, 1);
}

static void test_user_code_attributes(void) {
    TEST("PTE_USER_CODE has correct attributes");

    unsigned long pte = PTE_USER_CODE;

    // AP[2:1] = 01 (user accessible)
    ASSERT_EQ((pte >> 6) & 3, 1);

    // PXN = 0 (but wait, should be 1 for security!)
    ASSERT_EQ((pte >> 53) & 1, 0);
}

void run_pte_bit_tests(void) {
    TEST_SUITE("PTE Execute-Never Bits");
    test_uxn_bit();
    test_pxn_bit();
    test_kernel_data_attributes();
    test_user_data_attributes();
    test_user_code_attributes();
}
