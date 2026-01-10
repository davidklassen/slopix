#include "test_framework.h"
#include "../mmu.h"

// Inline assembly helpers to read system registers
static inline unsigned long read_mair_el1(void) {
    unsigned long val;
    __asm__ volatile("mrs %0, mair_el1" : "=r"(val));
    return val;
}

static inline unsigned long read_tcr_el1(void) {
    unsigned long val;
    __asm__ volatile("mrs %0, tcr_el1" : "=r"(val));
    return val;
}

static inline unsigned long read_sctlr_el1(void) {
    unsigned long val;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(val));
    return val;
}

static inline unsigned long read_ttbr0_el1(void) {
    unsigned long val;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(val));
    return val;
}

static inline unsigned long read_ttbr1_el1(void) {
    unsigned long val;
    __asm__ volatile("mrs %0, ttbr1_el1" : "=r"(val));
    return val;
}

static void test_mair_configured(void) {
    TEST("MAIR_EL1 configured correctly");

    unsigned long mair = read_mair_el1();
    ASSERT(mair == 0x00FF4400, "MAIR_EL1 = 0x00FF4400");
}

static void test_tcr_t0sz_configured(void) {
    TEST("TCR_EL1 T0SZ configured correctly");

    unsigned long tcr = read_tcr_el1();
    unsigned long t0sz = tcr & 0x3F;  // Extract bits 5:0
    ASSERT(t0sz == 25, "TCR_EL1 bits 5:0 (T0SZ) = 25");
}

static void test_tcr_t1sz_configured(void) {
    TEST("TCR_EL1 T1SZ configured correctly");

    unsigned long tcr = read_tcr_el1();
    unsigned long t1sz = (tcr >> 16) & 0x3F;  // Extract bits 21:16
    ASSERT(t1sz == 25, "TCR_EL1 bits 21:16 (T1SZ) = 25");
}

static void test_ttbr0_matches_page_table(void) {
    TEST("TTBR0_EL1 matches L0 page table address");

    unsigned long ttbr0_reg = read_ttbr0_el1();
    unsigned long ttbr0_expected = mmu_get_ttbr0();
    ASSERT(ttbr0_reg == ttbr0_expected, "TTBR0_EL1 matches mmu_get_ttbr0()");
}

static void test_ttbr0_aligned(void) {
    TEST("TTBR0_EL1 is 4KB aligned");

    unsigned long ttbr0 = read_ttbr0_el1();
    unsigned long lower_bits = ttbr0 & 0xFFF;  // Extract bits 11:0
    ASSERT(lower_bits == 0, "TTBR0_EL1 bits 11:0 are zero (4KB aligned)");
}

static void test_ttbr1_configured(void) {
    TEST("TTBR1_EL1 matches L1 page table address");

    unsigned long ttbr1_reg = read_ttbr1_el1();
    unsigned long ttbr1_expected = mmu_get_ttbr1();
    ASSERT(ttbr1_reg == ttbr1_expected, "TTBR1_EL1 matches mmu_get_ttbr1()");
    ASSERT(ttbr1_reg != 0, "TTBR1_EL1 is non-zero (configured for higher-half kernel)");
}

void run_mmu_register_tests(void) {
    TEST_SUITE("MMU Register Configuration");
    test_mair_configured();
    test_tcr_t0sz_configured();
    test_tcr_t1sz_configured();
    test_ttbr0_matches_page_table();
    test_ttbr0_aligned();
    test_ttbr1_configured();
}
