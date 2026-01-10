#include "test_framework.h"

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

static void test_mmu_still_disabled(void) {
    TEST("MMU is still disabled");

    unsigned long sctlr = read_sctlr_el1();
    unsigned long mmu_enabled = sctlr & 0x1;  // Extract bit 0
    ASSERT(mmu_enabled == 0, "SCTLR_EL1 bit 0 (M bit) = 0");
}

void run_mmu_register_tests(void) {
    TEST_SUITE("MMU Register Configuration");
    test_mair_configured();
    test_tcr_t0sz_configured();
    test_tcr_t1sz_configured();
    test_mmu_still_disabled();
}
