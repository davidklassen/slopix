#ifdef RUN_TESTS

#include "test.h"
#include "pmm.h"

TEST(pmm_init_populates_freelist) {
	unsigned long count = pmm_free_count();
	ASSERT(count > 32000, "Should have ~32k pages available");
	ASSERT(count < 33000, "Should have ~32k pages available");
	return 0;
}

TEST_SUITE(pmm) {
	RUN_TEST(pmm_init_populates_freelist);
}

#endif
