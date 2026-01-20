#ifdef RUN_TESTS

#include "test.h"
#include "pmm.h"

TEST(pmm_init_populates_freelist) {
	unsigned long count = pmm_free_count();
	ASSERT(count > 32000, "Should have ~32k pages available");
	ASSERT(count < 33000, "Should have ~32k pages available");
	return 0;
}

TEST(pmm_alloc_returns_aligned_address) {
	paddr_t pa = pmm_alloc();
	ASSERT_NOT_NULL(pa, "Allocation should succeed");
	ASSERT(IS_PAGE_ALIGNED(pa), "Address should be 4KB aligned");
	pmm_free(pa);
	return 0;
}

TEST(pmm_alloc_returns_valid_range) {
	paddr_t pa = pmm_alloc();
	ASSERT_NOT_NULL(pa, "Allocation should succeed");
	ASSERT(pa >= RAM_BASE, "Address should be >= RAM_BASE");
	ASSERT(pa < RAM_BASE + RAM_SIZE, "Address should be < RAM_END");
	pmm_free(pa);
	return 0;
}

TEST(pmm_alloc_returns_zeroed_page) {
	paddr_t pa = pmm_alloc();
	ASSERT_NOT_NULL(pa, "Allocation should succeed");

	unsigned long *p = (unsigned long *)PA_TO_VA(pa);
	for (unsigned long i = 0; i < PAGE_SIZE / sizeof(unsigned long); i++) {
		ASSERT(p[i] == 0, "Page should be zeroed");
	}

	pmm_free(pa);
	return 0;
}

TEST(pmm_alloc_decrements_count) {
	unsigned long before = pmm_free_count();
	paddr_t pa = pmm_alloc();
	unsigned long after = pmm_free_count();

	ASSERT_EQ(after, before - 1, "Free count should decrease by 1");

	pmm_free(pa);
	return 0;
}

TEST(pmm_free_increments_count) {
	paddr_t pa = pmm_alloc();
	unsigned long before = pmm_free_count();
	pmm_free(pa);
	unsigned long after = pmm_free_count();

	ASSERT_EQ(after, before + 1, "Free count should increase by 1");
	return 0;
}

TEST(pmm_alloc_returns_different_pages) {
	paddr_t pa1 = pmm_alloc();
	paddr_t pa2 = pmm_alloc();

	ASSERT_NOT_NULL(pa1, "First allocation should succeed");
	ASSERT_NOT_NULL(pa2, "Second allocation should succeed");
	ASSERT_NE(pa1, pa2, "Sequential allocations should return different pages");

	pmm_free(pa1);
	pmm_free(pa2);
	return 0;
}

TEST(pmm_free_makes_page_reusable) {
	paddr_t pa1 = pmm_alloc();
	pmm_free(pa1);
	paddr_t pa2 = pmm_alloc();

	ASSERT_EQ(pa1, pa2, "Freed page should be returned next (LIFO)");

	pmm_free(pa2);
	return 0;
}

TEST(pmm_multiple_alloc_free_cycles) {
	paddr_t pages[100];
	unsigned long initial_count = pmm_free_count();

	for (int i = 0; i < 100; i++) {
		pages[i] = pmm_alloc();
		ASSERT_NOT_NULL(pages[i], "Allocation should succeed");
	}

	ASSERT_EQ(pmm_free_count(), initial_count - 100, "Count should decrease by 100");

	for (int i = 0; i < 100; i++) {
		pmm_free(pages[i]);
	}

	ASSERT_EQ(pmm_free_count(), initial_count, "Count should return to initial value");
	return 0;
}

TEST_SUITE(pmm) {
	RUN_TEST(pmm_init_populates_freelist);
	RUN_TEST(pmm_alloc_returns_aligned_address);
	RUN_TEST(pmm_alloc_returns_valid_range);
	RUN_TEST(pmm_alloc_returns_zeroed_page);
	RUN_TEST(pmm_alloc_decrements_count);
	RUN_TEST(pmm_free_increments_count);
	RUN_TEST(pmm_alloc_returns_different_pages);
	RUN_TEST(pmm_free_makes_page_reusable);
	RUN_TEST(pmm_multiple_alloc_free_cycles);
}

#endif
