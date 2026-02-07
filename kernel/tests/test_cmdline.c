#include "test.h"
#include "cmdline.h"

TEST(cmdline_get_nonexistent) {
	ASSERT_NULL(cmdline_get("nonexistent_key_xyz"), "missing key returns NULL");
	return 0;
}

TEST_SUITE(cmdline) {
	RUN_TEST(cmdline_get_nonexistent);
}
