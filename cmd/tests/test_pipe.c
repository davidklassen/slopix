#include <test.h>
#include <unistd.h>
#include <string.h>

TEST(pipe_create) {
	int fd[2];
	ASSERT_EQ(pipe(fd), 0, "pipe should succeed");
	ASSERT(fd[0] >= 0, "read fd should be valid");
	ASSERT(fd[1] >= 0, "write fd should be valid");
	ASSERT(fd[0] != fd[1], "fds should be different");
	close(fd[0]);
	close(fd[1]);
	return 0;
}

TEST(pipe_write_read) {
	int fd[2];
	char buf[32];
	pipe(fd);

	ASSERT_EQ(write(fd[1], "hello", 5), 5, "write 5 bytes");
	ASSERT_EQ(read(fd[0], buf, 5), 5, "read 5 bytes");
	buf[5] = '\0';
	ASSERT(strcmp(buf, "hello") == 0, "data should match");

	close(fd[0]);
	close(fd[1]);
	return 0;
}

TEST(pipe_eof_on_close) {
	int fd[2];
	char buf[32];
	pipe(fd);

	close(fd[1]);
	ASSERT_EQ(read(fd[0], buf, 10), 0, "read should return 0 (EOF)");

	close(fd[0]);
	return 0;
}

TEST(pipe_broken_pipe) {
	int fd[2];
	pipe(fd);

	close(fd[0]);
	ASSERT_EQ(write(fd[1], "test", 4), -1, "write should fail");

	close(fd[1]);
	return 0;
}

TEST(pipe_fork_communicate) {
	int fd[2];
	char buf[32];
	pipe(fd);

	int pid = fork();
	if (pid == 0) {
		close(fd[0]);
		write(fd[1], "from child", 10);
		close(fd[1]);
		exit(0);
	}

	close(fd[1]);
	int n = read(fd[0], buf, sizeof(buf));
	buf[n] = '\0';
	close(fd[0]);
	wait(NULL);

	ASSERT_EQ(n, 10, "should read 10 bytes");
	ASSERT(strcmp(buf, "from child") == 0, "data should match");
	return 0;
}

TEST(pipe_multiple_writes) {
	int fd[2];
	char buf[64];
	pipe(fd);

	write(fd[1], "one", 3);
	write(fd[1], "two", 3);
	write(fd[1], "three", 5);

	int n = read(fd[0], buf, 11);
	buf[n] = '\0';
	ASSERT_EQ(n, 11, "should read 11 bytes");
	ASSERT(strcmp(buf, "onetwothree") == 0, "concatenated data");

	close(fd[0]);
	close(fd[1]);
	return 0;
}

TEST(pipe_partial_read) {
	int fd[2];
	char buf[32];
	pipe(fd);

	write(fd[1], "hello world", 11);
	ASSERT_EQ(read(fd[0], buf, 5), 5, "read 5 bytes");
	ASSERT_EQ(read(fd[0], buf, 6), 6, "read remaining 6 bytes");

	close(fd[0]);
	close(fd[1]);
	return 0;
}

TEST_SUITE(pipes) {
	RUN_TEST(pipe_create);
	RUN_TEST(pipe_write_read);
	RUN_TEST(pipe_eof_on_close);
	RUN_TEST(pipe_broken_pipe);
	RUN_TEST(pipe_fork_communicate);
	RUN_TEST(pipe_multiple_writes);
	RUN_TEST(pipe_partial_read);
}
