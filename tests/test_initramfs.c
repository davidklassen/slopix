#ifdef RUN_TESTS

#include "test.h"
#include "initramfs.h"

TEST(initramfs_find_shell) {
	struct initramfs_entry entry;
	int ret = initramfs_find("shell", &entry);

	ASSERT_EQ(ret, 0, "should find shell");
	ASSERT(entry.size > 0, "size should be > 0");
	ASSERT(entry.data != 0, "data should not be null");

	return 0;
}

TEST(initramfs_find_nonexistent) {
	struct initramfs_entry entry;
	int ret = initramfs_find("nonexistent", &entry);

	ASSERT_EQ(ret, -1, "should not find nonexistent");

	return 0;
}

TEST(initramfs_shell_is_valid_elf) {
	struct initramfs_entry entry;
	initramfs_find("shell", &entry);

	unsigned int magic = *(unsigned int *)entry.data;
	ASSERT_EQ(magic, 0x464C457F, "should be ELF magic");

	return 0;
}

TEST_SUITE(initramfs) {
	RUN_TEST(initramfs_find_shell);
	RUN_TEST(initramfs_find_nonexistent);
	RUN_TEST(initramfs_shell_is_valid_elf);
}

#endif
