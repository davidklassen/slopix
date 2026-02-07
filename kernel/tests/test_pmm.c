#include "test.h"
#include "pmm.h"
#include "board.h"

// This test is fragile: it depends on RAM_SIZE (128MB = 32768 pages) and the
// size of the initramfs which is reserved before pmm_init(). As userspace
// programs grow, fewer pages will be available. We use a conservative lower
// bound that allows ~12MB for kernel + initramfs overhead.
TEST(pmm_init_populates_freelist) {
	unsigned long count = pmm_free_count();
	unsigned long total_pages = RAM_SIZE / PAGE_SIZE;
	unsigned long min_expected = total_pages - 3000; // allow ~12MB overhead
	ASSERT(count > min_expected, "Should have most of RAM available");
	ASSERT(count <= total_pages, "Cannot exceed total RAM");
	return 0;
}

TEST(pmm_alloc_contiguous_basic) {
	unsigned long before = pmm_free_count();
	paddr_t pa = pmm_alloc_contiguous(4);
	ASSERT(pa != PMM_INVALID, "Should allocate 4 contiguous pages");
	ASSERT(IS_PAGE_ALIGNED(pa), "Should be page-aligned");
	unsigned long after = pmm_free_count();
	ASSERT_EQ(before - 4, after, "Should decrease free count by 4");
	pmm_free_contiguous(pa, 4);
	ASSERT_EQ(before, pmm_free_count(), "Should restore free count");
	return 0;
}

TEST(pmm_alloc_contiguous_one) {
	unsigned long before = pmm_free_count();
	paddr_t pa = pmm_alloc_contiguous(1);
	ASSERT(pa != PMM_INVALID, "Should allocate 1 page");
	ASSERT_EQ(before - 1, pmm_free_count(), "Should decrease by 1");
	pmm_free(pa);
	ASSERT_EQ(before, pmm_free_count(), "Should restore");
	return 0;
}

TEST(pmm_alloc_contiguous_zero) {
	paddr_t pa = pmm_alloc_contiguous(0);
	ASSERT_EQ(PMM_INVALID, pa, "Zero pages should fail");
	return 0;
}

TEST(pmm_alloc_contiguous_pages_are_contiguous) {
	paddr_t pa = pmm_alloc_contiguous(4);
	ASSERT(pa != PMM_INVALID, "Should allocate");
	// Verify we can access all 4 pages (they're zeroed by allocator)
	for (int i = 0; i < 4; i++) {
		unsigned long *p = (unsigned long *)PA_TO_VA(pa + i * PAGE_SIZE);
		ASSERT_EQ(0, *p, "Page should be zeroed");
	}
	pmm_free_contiguous(pa, 4);
	return 0;
}

TEST(pmm_alloc_contiguous_after_free) {
	unsigned long before = pmm_free_count();
	paddr_t p1 = pmm_alloc();
	paddr_t p2 = pmm_alloc();
	paddr_t p3 = pmm_alloc();
	pmm_free(p2);
	pmm_free(p1);
	pmm_free(p3);
	paddr_t pa = pmm_alloc_contiguous(4);
	ASSERT(pa != PMM_INVALID, "contiguous alloc after re-insertion");
	ASSERT(IS_PAGE_ALIGNED(pa), "aligned");
	pmm_free_contiguous(pa, 4);
	ASSERT_EQ(before, pmm_free_count(), "free count restored");
	return 0;
}

TEST(pmm_free_double_free_detected) {
	paddr_t pa = pmm_alloc();
	ASSERT(pa != PMM_INVALID, "Should allocate page");
	pmm_free(pa);
	unsigned long count = pmm_free_count();
	pmm_free(pa);
	ASSERT_EQ(count, pmm_free_count(), "Double free should not increase count");
	return 0;
}

TEST_SUITE(pmm) {
	RUN_TEST(pmm_init_populates_freelist);
	RUN_TEST(pmm_alloc_contiguous_basic);
	RUN_TEST(pmm_alloc_contiguous_one);
	RUN_TEST(pmm_alloc_contiguous_zero);
	RUN_TEST(pmm_alloc_contiguous_pages_are_contiguous);
	RUN_TEST(pmm_alloc_contiguous_after_free);
	RUN_TEST(pmm_free_double_free_detected);
}
