#include "test_framework.h"
#include "../mmu.h"
#include "../memory.h"

// Assembly function to read TCR_EL1
static inline unsigned long read_tcr_el1(void) {
    unsigned long val;
    __asm__ volatile("mrs %0, tcr_el1" : "=r" (val));
    return val;
}

static void test_ttbr1_allocated(void) {
    TEST("TTBR1 L1 table allocated");

    unsigned long ttbr1 = mmu_get_ttbr1();
    ASSERT(ttbr1 != 0, "TTBR1 is non-zero (L1 table allocated)");
}

static void test_tcr_t1sz(void) {
    TEST("TCR_EL1.T1SZ = 25");

    unsigned long tcr = read_tcr_el1();
    unsigned long t1sz = (tcr >> 16) & 0x3F;  // T1SZ is bits [21:16]

    ASSERT(t1sz == 25, "T1SZ is 25 (39-bit VA for TTBR1)");
}

static void test_ttbr1_l1_entry0_valid(void) {
    TEST("TTBR1 L1[0] points to valid L2 table");

    unsigned long ttbr1 = mmu_get_ttbr1();
    unsigned long *l1 = (unsigned long *)ttbr1;
    unsigned long l1_entry = l1[0];

    // Check VALID bit (bit 0)
    ASSERT((l1_entry & PTE_VALID) != 0, "TTBR1 L1[0] has VALID bit set");

    // Check TABLE bit (bit 1)
    ASSERT((l1_entry & PTE_TABLE) != 0, "TTBR1 L1[0] has TABLE bit set");
}

static void test_ttbr1_l1_entry1_valid(void) {
    TEST("TTBR1 L1[1] points to valid L2 table");

    unsigned long ttbr1 = mmu_get_ttbr1();
    unsigned long *l1 = (unsigned long *)ttbr1;
    unsigned long l1_entry = l1[1];

    // Check VALID bit (bit 0)
    ASSERT((l1_entry & PTE_VALID) != 0, "TTBR1 L1[1] has VALID bit set");

    // Check TABLE bit (bit 1)
    ASSERT((l1_entry & PTE_TABLE) != 0, "TTBR1 L1[1] has TABLE bit set");
}

static void test_ttbr1_l2_kernel_entry0(void) {
    TEST("TTBR1 L2_kernel[0] maps PA 0x40000000");

    unsigned long *l2_kernel = mmu_get_ttbr1_l2_kernel();
    unsigned long entry = l2_kernel[0];

    // Check VALID bit
    ASSERT((entry & PTE_VALID) != 0, "TTBR1 L2_kernel[0] has VALID bit set");

    // Check physical address (bits 47:21 should be 0x40000)
    unsigned long phys_addr = entry & 0xFFFFFFE00000UL;
    ASSERT(phys_addr == 0x40000000, "TTBR1 L2_kernel[0] physical address is 0x40000000");

    // Check AttrIndx (bits 4:2 should be MT_NORMAL = 2)
    unsigned long attr_indx = (entry >> 2) & 0x7;
    ASSERT(attr_indx == MT_NORMAL, "TTBR1 L2_kernel[0] has MT_NORMAL attribute");

    // Check AF bit (bit 10)
    ASSERT((entry & PTE_AF) != 0, "TTBR1 L2_kernel[0] has AF bit set");
}

static void test_ttbr1_page_walk(void) {
    TEST("TTBR1 page table walk for VA 0xFFFFFF8040000074 -> PA 0x40000074");

    // Walk page tables manually
    unsigned long ttbr1 = mmu_get_ttbr1();
    unsigned long *l1 = (unsigned long *)ttbr1;

    // For VA 0xFFFF_FF80_4000_0074:
    // VA[63:39] = all 1s (TTBR1 selector)
    // VA[38:30] = 0b000000001 = 1 (L1 index)
    // VA[29:21] = 0b000000000 = 0 (L2 index)
    // VA[20:0]  = 0x74 (offset within 2MB block)

    // Step 1: Read L1[1]
    unsigned long l1_entry = l1[1];
    ASSERT((l1_entry & PTE_VALID) != 0, "L1[1] is valid");
    ASSERT((l1_entry & PTE_TABLE) != 0, "L1[1] is table descriptor");

    // Extract L2 table address
    unsigned long l2_table_addr = l1_entry & 0xFFFFFFFFF000UL;  // Bits [47:12]
    unsigned long *l2_table = (unsigned long *)l2_table_addr;

    // Step 2: Read L2[0]
    unsigned long l2_entry = l2_table[0];
    ASSERT((l2_entry & PTE_VALID) != 0, "L2[0] is valid");
    ASSERT((l2_entry & PTE_TABLE) == 0, "L2[0] is block descriptor");

    // Extract physical address
    unsigned long block_base = l2_entry & 0xFFFFFFE00000UL;  // Bits [47:21]
    unsigned long final_pa = block_base + 0x74;

    ASSERT(final_pa == 0x40000074, "Final PA is 0x40000074");
}

static void test_ttbr1_dual_access_precondition(void) {
    TEST("TTBR1 dual access precondition - read PA 0x40000074 via TTBR0");

    // First, verify we can read via TTBR0 (identity mapping at 0x40000074)
    volatile unsigned int *ptr_ttbr0 = (volatile unsigned int *)0x40000074;
    unsigned int value_ttbr0 = *ptr_ttbr0;

    // Just verify we can read it (value doesn't matter, just that it doesn't fault)
    (void)value_ttbr0;  // Use the value to avoid compiler warning

    ASSERT(1, "Successfully read via TTBR0 at 0x40000074");
}

// Note: This test can only run AFTER MMU is enabled and TTBR1 is active
static void test_ttbr1_dual_access_postflight(void) {
    TEST("TTBR1 dual access - same memory via TTBR0 and TTBR1");

    // Read via TTBR0 (identity mapping)
    volatile unsigned int *ptr_ttbr0 = (volatile unsigned int *)0x40000074;
    unsigned int value_ttbr0 = *ptr_ttbr0;

    // Read via TTBR1 (higher-half mapping)
    // VA 0xFFFF_FF80_4000_0074 should map to same PA 0x40000074
    volatile unsigned int *ptr_ttbr1 = (volatile unsigned int *)0xFFFFFF8040000074UL;
    unsigned int value_ttbr1 = *ptr_ttbr1;

    ASSERT(value_ttbr0 == value_ttbr1, "Same value read via TTBR0 and TTBR1");
}

void run_ttbr1_preflight_tests(void) {
    TEST_SUITE("TTBR1 Page Tables (Pre-MMU)");
    test_ttbr1_allocated();
    test_tcr_t1sz();
    test_ttbr1_l1_entry0_valid();
    test_ttbr1_l1_entry1_valid();
    test_ttbr1_l2_kernel_entry0();
    test_ttbr1_page_walk();
    test_ttbr1_dual_access_precondition();
}

void run_ttbr1_postflight_tests(void) {
    TEST_SUITE("TTBR1 Higher-Half Access (Post-MMU)");
    test_ttbr1_dual_access_postflight();
}
