#include "test_framework.h"
#include "../mmu.h"
#include "../memory.h"

static void test_ttbr0_allocated(void) {
    TEST("TTBR0 (L0 table) allocated");

    unsigned long ttbr0 = mmu_get_ttbr0();
    ASSERT(ttbr0 != 0, "TTBR0 is non-zero (L0 table allocated)");
}

static void test_l2_table_exists(void) {
    TEST("L2 table exists");

    unsigned long *l2 = mmu_get_l2_table();
    ASSERT(l2 != 0, "L2 table pointer is non-zero");
}

static void test_l0_points_to_l1(void) {
    TEST("L0[0] points to L1 table");

    unsigned long ttbr0 = mmu_get_ttbr0();
    unsigned long *l0 = (unsigned long *)ttbr0;
    unsigned long l0_entry = l0[0];

    // Check VALID bit (bit 0)
    ASSERT((l0_entry & PTE_VALID) != 0, "L0[0] has VALID bit set");

    // Check TABLE bit (bit 1)
    ASSERT((l0_entry & PTE_TABLE) != 0, "L0[0] has TABLE bit set");
}

static void test_identity_map_entry_0(void) {
    TEST("L2[0] maps physical address 0x00000000");

    unsigned long *l2 = mmu_get_l2_table();
    unsigned long entry = l2[0];

    // Check VALID bit (bit 0)
    ASSERT((entry & PTE_VALID) != 0, "L2[0] has VALID bit set");

    // Check physical address (bits 47:21 should be 0 for entry 0)
    unsigned long phys_addr = entry & 0xFFFFFFE00000UL;
    ASSERT(phys_addr == 0x0, "L2[0] physical address is 0x00000000");

    // Check AttrIndx (bits 4:2 should be MT_NORMAL = 2)
    unsigned long attr_indx = (entry >> 2) & 0x7;
    ASSERT(attr_indx == MT_NORMAL, "L2[0] has MT_NORMAL attribute");

    // Check AF bit (bit 10)
    ASSERT((entry & PTE_AF) != 0, "L2[0] has AF bit set");
}

static void test_identity_map_entry_64(void) {
    TEST("L2[64] maps physical address 0x08000000");

    unsigned long *l2 = mmu_get_l2_table();
    unsigned long entry = l2[64];

    // Check VALID bit
    ASSERT((entry & PTE_VALID) != 0, "L2[64] has VALID bit set");

    // Check physical address (entry 64 * 2MB = 0x08000000)
    unsigned long phys_addr = entry & 0xFFFFFFE00000UL;
    ASSERT(phys_addr == 0x08000000, "L2[64] physical address is 0x08000000");
}

static void test_all_128_entries_valid(void) {
    TEST("All 128 L2 entries have VALID bit set");

    unsigned long *l2 = mmu_get_l2_table();
    int all_valid = 1;

    for (int i = 0; i < 128; i++) {
        if ((l2[i] & PTE_VALID) == 0) {
            all_valid = 0;
            break;
        }
    }

    ASSERT(all_valid, "All L2[0..127] entries are valid");
}

void run_mmu_table_tests(void) {
    TEST_SUITE("MMU Page Table Structure");
    test_ttbr0_allocated();
    test_l2_table_exists();
    test_l0_points_to_l1();
    test_identity_map_entry_0();
    test_identity_map_entry_64();
    test_all_128_entries_valid();
}
