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

TEST(fork_wait) {
	int pid = fork();
	if (pid == 0) {
		exit(42);
	}
	ASSERT_EQ(wait(), 42, "child exit status is 42");
	return 0;
}

TEST(fork_multiple) {
	for (int i = 0; i < 3; i++) {
		int pid = fork();
		if (pid == 0) {
			exit(i);
		}
	}
	int count = 0;
	while (wait() >= 0) {
		count++;
	}
	ASSERT_EQ(count, 3, "reaped 3 children");
	return 0;
}

TEST(sleep_works) {
	sleep(50);
	return 0;
}

TEST(exec_true) {
	int pid = fork();
	if (pid == 0) {
		exec("true");
		exit(1);
	}
	ASSERT_EQ(wait(), 0, "exec'd program exits 0");
	return 0;
}

TEST_SUITE(scheduler) {
	RUN_TEST(fork_wait);
	RUN_TEST(fork_multiple);
	RUN_TEST(sleep_works);
	RUN_TEST(exec_true);
}

int main(void) {
	RUN_SUITE(syscalls);
	RUN_SUITE(scheduler);
	TEST_REPORT();
	TEST_EXIT();
	return 0;
}
