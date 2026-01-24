#ifdef RUN_TESTS

#include "test.h"
#include "board.h"
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

TEST(dtb_initrd_start_valid) {
	unsigned long start = dtb_get_initrd_start();
	ASSERT(start >= RAM_BASE, "initrd_start >= RAM_BASE");
	ASSERT(start < RAM_BASE + RAM_SIZE, "initrd_start < RAM end");
	return 0;
}

TEST(dtb_initrd_end_valid) {
	unsigned long end = dtb_get_initrd_end();
	unsigned long start = dtb_get_initrd_start();
	ASSERT(end > start, "initrd_end > start");
	ASSERT(end <= RAM_BASE + RAM_SIZE, "initrd_end <= RAM end");
	return 0;
}

TEST_SUITE(dtb) {
	RUN_TEST(dtb_bootargs_not_null);
	RUN_TEST(dtb_bootargs_has_init);
	RUN_TEST(dtb_initrd_start_valid);
	RUN_TEST(dtb_initrd_end_valid);
}

#endif
