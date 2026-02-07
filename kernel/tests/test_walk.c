#include "test.h"
#include "vmm.h"
#include "pmm.h"

TEST(walk_leak_on_oom) {
	unsigned long before = pmm_free_count();

	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	// Drain pages until exactly 2 remain. Store addresses in a linked
	// list inside the pages themselves (each 4KB page has room for a
	// pointer at its start).
	paddr_t drain_head = PMM_INVALID;
	while (pmm_free_count() > 2) {
		paddr_t pa = pmm_alloc();
		ASSERT(pa != PMM_INVALID, "drain alloc");
		paddr_t *p = (paddr_t *)PA_TO_VA(pa);
		*p = drain_head;
		drain_head = pa;
	}

	ASSERT_EQ(pmm_free_count(), 2, "drained to 2 free pages");

	// VA 0x1000 needs 3 new intermediate tables (L1, L2, L3) since
	// the page table is fresh. With only 2 free pages, the 3rd
	// allocation fails. walk() must clean up the first 2.
	int r = vmm_map_page(pt, 0x1000, 0x40001000, 1, 0);
	ASSERT_EQ(r, -1, "vmm_map_page should fail");
	ASSERT_EQ(pmm_free_count(), 2, "no pages leaked on walk failure");

	// Restore drained pages
	while (drain_head != PMM_INVALID) {
		paddr_t *p = (paddr_t *)PA_TO_VA(drain_head);
		paddr_t next = *p;
		pmm_free(drain_head);
		drain_head = next;
	}

	vmm_free(pt);
	ASSERT_EQ(pmm_free_count(), before, "all pages restored");

	return 0;
}

TEST_SUITE(walk) {
	RUN_TEST(walk_leak_on_oom);
}
