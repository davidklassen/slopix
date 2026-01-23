#ifdef RUN_TESTS

#include "test.h"
#include "dtb.h"
#include "string.h"

TEST(dtb_bootargs_not_null) {
	ASSERT_NOT_NULL(dtb_get_bootargs(), "bootargs should not be NULL");
	return 0;
}

TEST(dtb_bootargs_has_init) {
	const char *args = dtb_get_bootargs();
	ASSERT_NOT_NULL(args, "bootargs exists");
	ASSERT_NOT_NULL(strstr(args, "init="), "bootargs contains init=");
	return 0;
}

TEST_SUITE(dtb) {
	RUN_TEST(dtb_bootargs_not_null);
	RUN_TEST(dtb_bootargs_has_init);
}

#endif
