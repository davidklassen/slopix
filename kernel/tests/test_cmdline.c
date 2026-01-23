#ifdef RUN_TESTS

#include "test.h"
#include "cmdline.h"

TEST(cmdline_get_init) {
	ASSERT_NOT_NULL(cmdline_get("init"), "init key exists");
	return 0;
}

TEST(cmdline_get_nonexistent) {
	ASSERT_NULL(cmdline_get("nonexistent_key_xyz"), "missing key returns NULL");
	return 0;
}

TEST_SUITE(cmdline) {
	RUN_TEST(cmdline_get_init);
	RUN_TEST(cmdline_get_nonexistent);
}

#endif
