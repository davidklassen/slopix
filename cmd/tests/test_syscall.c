#include <test.h>
#include <unistd.h>

TEST(write_returns_count) {
	ASSERT_EQ(write(1, "x", 1), 1, "write returns 1");
	return 0;
}

TEST(read_poll) {
	ASSERT_EQ(poll(0, 0), 0, "poll times out with no input");
	return 0;
}

TEST(getpid_positive) {
	ASSERT(getpid() > 0, "pid is positive");
	return 0;
}

TEST(bad_write_pointer) {
	ASSERT_EQ(write(1, (char *)0xDEADBEEF, 10), -1, "bad pointer returns -1");
	return 0;
}

TEST(bad_read_pointer) {
	ASSERT_EQ(read(0, (char *)0xDEADBEEF, 10), -1, "bad pointer returns -1");
	return 0;
}

TEST_SUITE(syscalls) {
	RUN_TEST(write_returns_count);
	RUN_TEST(read_poll);
	RUN_TEST(getpid_positive);
	RUN_TEST(bad_write_pointer);
	RUN_TEST(bad_read_pointer);
}
