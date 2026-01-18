#ifdef RUN_TESTS

#include "test.h"
#include "pmem.h"

TEST(pmem_init_populates_freelist) {
	unsigned long count = pmem_free_count();
	ASSERT(count > 32000, "Should have ~32k pages available");
	ASSERT(count < 33000, "Should have ~32k pages available");
	return 0;
}

TEST(pmem_alloc_returns_aligned_address) {
	paddr_t pa = pmem_alloc();
	ASSERT_NOT_NULL(pa, "Allocation should succeed");
	ASSERT(IS_PAGE_ALIGNED(pa), "Address should be 4KB aligned");
	pmem_free(pa);
	return 0;
}

TEST(pmem_alloc_returns_valid_range) {
	paddr_t pa = pmem_alloc();
	ASSERT_NOT_NULL(pa, "Allocation should succeed");
	ASSERT(pa >= RAM_BASE, "Address should be >= RAM_BASE");
	ASSERT(pa < RAM_BASE + RAM_SIZE, "Address should be < RAM_END");
	pmem_free(pa);
	return 0;
}

TEST(pmem_alloc_returns_zeroed_page) {
	paddr_t pa = pmem_alloc();
	ASSERT_NOT_NULL(pa, "Allocation should succeed");

	unsigned long *p = (unsigned long *)PA_TO_VA(pa);
	for (unsigned long i = 0; i < PAGE_SIZE / sizeof(unsigned long); i++) {
		ASSERT(p[i] == 0, "Page should be zeroed");
	}

	pmem_free(pa);
	return 0;
}

TEST(pmem_alloc_decrements_count) {
	unsigned long before = pmem_free_count();
	paddr_t pa = pmem_alloc();
	unsigned long after = pmem_free_count();

	ASSERT_EQ(after, before - 1, "Free count should decrease by 1");

	pmem_free(pa);
	return 0;
}

TEST(pmem_free_increments_count) {
	paddr_t pa = pmem_alloc();
	unsigned long before = pmem_free_count();
	pmem_free(pa);
	unsigned long after = pmem_free_count();

	ASSERT_EQ(after, before + 1, "Free count should increase by 1");
	return 0;
}

TEST(pmem_alloc_returns_different_pages) {
	paddr_t pa1 = pmem_alloc();
	paddr_t pa2 = pmem_alloc();

	ASSERT_NOT_NULL(pa1, "First allocation should succeed");
	ASSERT_NOT_NULL(pa2, "Second allocation should succeed");
	ASSERT_NE(pa1, pa2, "Sequential allocations should return different pages");

	pmem_free(pa1);
	pmem_free(pa2);
	return 0;
}

TEST(pmem_free_makes_page_reusable) {
	paddr_t pa1 = pmem_alloc();
	pmem_free(pa1);
	paddr_t pa2 = pmem_alloc();

	ASSERT_EQ(pa1, pa2, "Freed page should be returned next (LIFO)");

	pmem_free(pa2);
	return 0;
}

TEST(pmem_multiple_alloc_free_cycles) {
	paddr_t pages[100];
	unsigned long initial_count = pmem_free_count();

	for (int i = 0; i < 100; i++) {
		pages[i] = pmem_alloc();
		ASSERT_NOT_NULL(pages[i], "Allocation should succeed");
	}

	ASSERT_EQ(pmem_free_count(), initial_count - 100, "Count should decrease by 100");

	for (int i = 0; i < 100; i++) {
		pmem_free(pages[i]);
	}

	ASSERT_EQ(pmem_free_count(), initial_count, "Count should return to initial value");
	return 0;
}

TEST_SUITE(pmem) {
	RUN_TEST(pmem_init_populates_freelist);
	RUN_TEST(pmem_alloc_returns_aligned_address);
	RUN_TEST(pmem_alloc_returns_valid_range);
	RUN_TEST(pmem_alloc_returns_zeroed_page);
	RUN_TEST(pmem_alloc_decrements_count);
	RUN_TEST(pmem_free_increments_count);
	RUN_TEST(pmem_alloc_returns_different_pages);
	RUN_TEST(pmem_free_makes_page_reusable);
	RUN_TEST(pmem_multiple_alloc_free_cycles);
}

#endif
