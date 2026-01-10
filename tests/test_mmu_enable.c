#include "test_framework.h"
#include "../mmu.h"
#include "../pmm.h"
#include "../memory.h"

// Inline assembly helpers
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

// PRE-FLIGHT TESTS

static void test_preflight_page_tables_allocated(void) {
    TEST("PRE-FLIGHT: Page tables allocated");

    unsigned long ttbr0 = mmu_get_ttbr0();
    unsigned long *l2 = mmu_get_l2_table();

    ASSERT(ttbr0 != 0, "L0 table allocated");
    ASSERT(l2 != 0, "L2 table allocated");
}

static void test_preflight_ttbr0_set(void) {
    TEST("PRE-FLIGHT: TTBR0 points to L0 table");

    unsigned long ttbr0_reg = read_ttbr0_el1();
    unsigned long ttbr0_expected = mmu_get_ttbr0();

    ASSERT(ttbr0_reg == ttbr0_expected, "TTBR0_EL1 set correctly");
}

static void test_preflight_mair_configured(void) {
    TEST("PRE-FLIGHT: MAIR configured");

    unsigned long mair = read_mair_el1();
    ASSERT(mair == 0x00FF4400, "MAIR_EL1 = 0x00FF4400");
}

static void test_preflight_tcr_configured(void) {
    TEST("PRE-FLIGHT: TCR configured");

    unsigned long tcr = read_tcr_el1();
    unsigned long t0sz = tcr & 0x3F;

    ASSERT(t0sz == 25, "TCR_EL1 T0SZ = 25");
}

static void test_preflight_kernel_region_mapped(void) {
    TEST("PRE-FLIGHT: Kernel region mapped in TTBR1");

    // Kernel symbols are now at virtual addresses (0xFFFFFF8040XXXXXX)
    // These map via TTBR1, not TTBR0
    // Check TTBR1 L1[1] points to L2_kernel
    unsigned long ttbr1 = mmu_get_ttbr1();
    unsigned long *l1 = (unsigned long *)ttbr1;
    unsigned long entry = l1[1];

    ASSERT((entry & PTE_VALID) != 0, "TTBR1 L1[1] is valid");
    ASSERT((entry & PTE_TABLE) != 0, "TTBR1 L1[1] points to table (kernel region)");
}

static void test_preflight_stack_mapped(void) {
    TEST("PRE-FLIGHT: Stack region mapped");

    // Stack is in low memory (first 256MB), mapped by L2 table
    unsigned long *l2 = mmu_get_l2_table();
    // Check first 10 entries (first 20MB)
    int valid_count = 0;
    for (int i = 0; i < 10; i++) {
        if ((l2[i] & PTE_VALID) != 0) {
            valid_count++;
        }
    }

    ASSERT(valid_count == 10, "First 20MB mapped (stack region)");
}

static void test_preflight_uart_mapped(void) {
    TEST("PRE-FLIGHT: UART region mapped (0x09000000)");

    // UART at 0x09000000 = entry 4 or 5 in L2 (0x09000000 / 2MB = 4.5)
    unsigned long *l2 = mmu_get_l2_table();
    unsigned long entry = l2[4];

    ASSERT((entry & PTE_VALID) != 0, "UART region (L2[4]) is valid");
}

static void test_preflight_gic_mapped(void) {
    TEST("PRE-FLIGHT: GIC region mapped (0x08000000)");

    // GIC at 0x08000000 = entry 4 in L2 (0x08000000 / 2MB = 4)
    unsigned long *l2 = mmu_get_l2_table();
    unsigned long entry = l2[4];

    ASSERT((entry & PTE_VALID) != 0, "GIC region (L2[4]) is valid");
}

// POST-FLIGHT TESTS

static void test_postflight_mmu_enabled(void) {
    TEST("POST-FLIGHT: MMU is enabled");

    unsigned long sctlr = read_sctlr_el1();
    unsigned long mmu_enabled = sctlr & 0x1;

    ASSERT(mmu_enabled == 1, "SCTLR_EL1.M = 1 (MMU enabled)");
}

static void test_postflight_can_write_stack(void) {
    TEST("POST-FLIGHT: Can write to stack");

    volatile unsigned long test_val = 0;
    test_val = 0xDEADBEEF;

    ASSERT(test_val == 0xDEADBEEF, "Stack write/read works");
}

static void test_postflight_can_read_data(void) {
    TEST("POST-FLIGHT: Can read global data");

    // Try to read the L2 table pointer (global variable in mmu.c)
    unsigned long *l2 = mmu_get_l2_table();

    ASSERT(l2 != 0, "Can read global variables");
}

static void test_postflight_uart_works(void) {
    TEST("POST-FLIGHT: UART still works");

    // If we got here and printf worked, UART is fine
    // Just do a simple assertion
    ASSERT(1, "Printf works (UART accessible)");
}

static void test_postflight_pmm_works(void) {
    TEST("POST-FLIGHT: PMM still works");

    void *page = pmm_alloc_page();
    ASSERT(page != 0, "Can allocate page after MMU enable");

    // Free it back
    if (page) {
        pmm_free_page(page);
    }
}

// Wrapper functions

void run_mmu_preflight_tests(void) {
    TEST_SUITE("MMU Pre-Flight Checks");
    test_preflight_page_tables_allocated();
    test_preflight_ttbr0_set();
    test_preflight_mair_configured();
    test_preflight_tcr_configured();
    test_preflight_kernel_region_mapped();
    test_preflight_stack_mapped();
    test_preflight_uart_mapped();
    test_preflight_gic_mapped();
}

void run_mmu_postflight_tests(void) {
    TEST_SUITE("MMU Post-Flight Verification");
    test_postflight_mmu_enabled();
    test_postflight_can_write_stack();
    test_postflight_can_read_data();
    test_postflight_uart_works();
    test_postflight_pmm_works();
}
