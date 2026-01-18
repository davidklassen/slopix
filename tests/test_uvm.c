#ifdef RUN_TESTS

#include "test.h"
#include "mmu.h"
#include "pmem.h"

TEST(uvm_create_returns_zeroed_table) {
	pte_t *pt = uvm_create();
	ASSERT_NOT_NULL(pt, "should allocate");

	for (int i = 0; i < PTE_PER_TABLE; i++) {
		ASSERT_EQ(pt[i], 0, "entries should be zero");
	}

	uvm_free(pt);
	return 0;
}

TEST(uvm_free_releases_pages) {
	unsigned long before = pmem_free_count();

	pte_t *pt = uvm_create();
	paddr_t page = pmem_alloc();
	uvm_map_page(pt, 0x1000, page, 1, 0);

	// Used: L0 + L1 + L2 + L3 + data page = 5 pages

	uvm_free(pt);

	unsigned long after = pmem_free_count();
	ASSERT_EQ(before, after, "all pages should be freed");

	return 0;
}

TEST(uvm_map_page_creates_mapping) {
	paddr_t l0_pa = pmem_alloc();
	pte_t *l0 = (pte_t *)PA_TO_VA(l0_pa);
	paddr_t page_pa = pmem_alloc();

	unsigned long va = 0x1000;
	int ret = uvm_map_page(l0, va, page_pa, 1, 0);
	ASSERT_EQ(ret, 0, "uvm_map_page should succeed");

	ASSERT(l0[L0_INDEX(va)] & PTE_VALID, "L0 entry should be valid");
	pte_t *l1 = (pte_t *)PA_TO_VA(l0[L0_INDEX(va)] & PTE_ADDR_MASK);
	ASSERT(l1[L1_INDEX(va)] & PTE_VALID, "L1 entry should be valid");
	pte_t *l2 = (pte_t *)PA_TO_VA(l1[L1_INDEX(va)] & PTE_ADDR_MASK);
	ASSERT(l2[L2_INDEX(va)] & PTE_VALID, "L2 entry should be valid");
	pte_t *l3 = (pte_t *)PA_TO_VA(l2[L2_INDEX(va)] & PTE_ADDR_MASK);
	ASSERT(l3[L3_INDEX(va)] & PTE_VALID, "L3 entry should be valid");

	paddr_t mapped_pa = l3[L3_INDEX(va)] & PTE_ADDR_MASK;
	ASSERT_EQ(mapped_pa, page_pa, "L3 entry should map to correct PA");

	uvm_free(l0);
	return 0;
}

TEST(uvm_map_page_sets_permissions_rw) {
	paddr_t l0_pa = pmem_alloc();
	pte_t *l0 = (pte_t *)PA_TO_VA(l0_pa);
	paddr_t page_pa = pmem_alloc();

	unsigned long va = 0x1000;
	int ret = uvm_map_page(l0, va, page_pa, 1, 0);
	ASSERT_EQ(ret, 0, "uvm_map_page should succeed");

	pte_t *l1 = (pte_t *)PA_TO_VA(l0[L0_INDEX(va)] & PTE_ADDR_MASK);
	pte_t *l2 = (pte_t *)PA_TO_VA(l1[L1_INDEX(va)] & PTE_ADDR_MASK);
	pte_t *l3 = (pte_t *)PA_TO_VA(l2[L2_INDEX(va)] & PTE_ADDR_MASK);
	pte_t pte = l3[L3_INDEX(va)];

	ASSERT(pte & PTE_AF, "Access flag should be set");
	ASSERT((pte & (3UL << 6)) == PTE_AP_RW_ALL, "Should be RW for all");
	ASSERT(pte & PTE_UXN, "Should be non-executable for user");
	ASSERT(pte & PTE_PXN, "Should be non-executable for kernel");

	uvm_free(l0);
	return 0;
}

TEST(uvm_map_page_sets_permissions_ro_exec) {
	paddr_t l0_pa = pmem_alloc();
	pte_t *l0 = (pte_t *)PA_TO_VA(l0_pa);
	paddr_t page_pa = pmem_alloc();

	unsigned long va = 0x1000;
	int ret = uvm_map_page(l0, va, page_pa, 0, 1);
	ASSERT_EQ(ret, 0, "uvm_map_page should succeed");

	pte_t *l1 = (pte_t *)PA_TO_VA(l0[L0_INDEX(va)] & PTE_ADDR_MASK);
	pte_t *l2 = (pte_t *)PA_TO_VA(l1[L1_INDEX(va)] & PTE_ADDR_MASK);
	pte_t *l3 = (pte_t *)PA_TO_VA(l2[L2_INDEX(va)] & PTE_ADDR_MASK);
	pte_t pte = l3[L3_INDEX(va)];

	ASSERT((pte & (3UL << 6)) == PTE_AP_RO_ALL, "Should be RO for all");
	ASSERT(!(pte & PTE_UXN), "Should be executable for user");
	ASSERT(pte & PTE_PXN, "Should be non-executable for kernel");

	uvm_free(l0);
	return 0;
}

TEST(uvm_map_page_fails_on_double_map) {
	paddr_t l0_pa = pmem_alloc();
	pte_t *l0 = (pte_t *)PA_TO_VA(l0_pa);
	paddr_t page1_pa = pmem_alloc();
	paddr_t page2_pa = pmem_alloc();

	unsigned long va = 0x1000;
	int ret1 = uvm_map_page(l0, va, page1_pa, 1, 0);
	ASSERT_EQ(ret1, 0, "First mapping should succeed");

	int ret2 = uvm_map_page(l0, va, page2_pa, 1, 0);
	ASSERT_EQ(ret2, -1, "Second mapping to same VA should fail");

	pmem_free(page2_pa); // page2 was never mapped
	uvm_free(l0);
	return 0;
}

TEST(uvm_map_page_allocates_intermediate_tables) {
	unsigned long before = pmem_free_count();

	paddr_t l0_pa = pmem_alloc();
	pte_t *l0 = (pte_t *)PA_TO_VA(l0_pa);
	paddr_t page_pa = pmem_alloc();

	unsigned long va = 0x1000;
	int ret = uvm_map_page(l0, va, page_pa, 1, 0);
	ASSERT_EQ(ret, 0, "uvm_map_page should succeed");

	unsigned long after = pmem_free_count();
	// 1 for L0 + 1 for page + 3 for L1/L2/L3 tables = 5 pages
	ASSERT_EQ(before - after, 5, "Should allocate 5 pages total");

	uvm_free(l0);
	return 0;
}

TEST(uvm_map_page_reuses_existing_tables) {
	paddr_t l0_pa = pmem_alloc();
	pte_t *l0 = (pte_t *)PA_TO_VA(l0_pa);
	paddr_t page1_pa = pmem_alloc();
	paddr_t page2_pa = pmem_alloc();

	unsigned long va1 = 0x1000;
	int ret1 = uvm_map_page(l0, va1, page1_pa, 1, 0);
	ASSERT_EQ(ret1, 0, "First mapping should succeed");

	unsigned long before = pmem_free_count();

	unsigned long va2 = 0x2000;
	int ret2 = uvm_map_page(l0, va2, page2_pa, 1, 0);
	ASSERT_EQ(ret2, 0, "Second mapping should succeed");

	unsigned long after = pmem_free_count();
	ASSERT_EQ(before, after, "Should not allocate new tables for nearby VA");

	uvm_free(l0);
	return 0;
}

TEST_SUITE(uvm) {
	RUN_TEST(uvm_create_returns_zeroed_table);
	RUN_TEST(uvm_free_releases_pages);
	RUN_TEST(uvm_map_page_creates_mapping);
	RUN_TEST(uvm_map_page_sets_permissions_rw);
	RUN_TEST(uvm_map_page_sets_permissions_ro_exec);
	RUN_TEST(uvm_map_page_fails_on_double_map);
	RUN_TEST(uvm_map_page_allocates_intermediate_tables);
	RUN_TEST(uvm_map_page_reuses_existing_tables);
}

#endif
