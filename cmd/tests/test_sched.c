#include <test.h>
#include <unistd.h>

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
