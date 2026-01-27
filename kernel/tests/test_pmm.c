#ifdef RUN_TESTS

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

TEST_SUITE(pmm) {
	RUN_TEST(pmm_init_populates_freelist);
}

#endif
