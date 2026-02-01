#include <test.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

TEST(open_nonexistent_sets_errno) {
	errno = 0;
	int fd = open("/this/file/does/not/exist", O_RDONLY);
	ASSERT_EQ(fd, -1, "open fails");
	ASSERT_NE(errno, 0, "errno should be set after failed open");
	return 0;
}

TEST(read_bad_fd_sets_errno) {
	errno = 0;
	int ret = read(999, (char[1]){0}, 1);
	ASSERT_EQ(ret, -1, "read fails");
	ASSERT_NE(errno, 0, "errno should be set after failed read");
	return 0;
}

TEST(write_bad_fd_sets_errno) {
	errno = 0;
	int ret = write(999, "x", 1);
	ASSERT_EQ(ret, -1, "write fails");
	ASSERT_NE(errno, 0, "errno should be set after failed write");
	return 0;
}

TEST(strerror_zero_is_success) {
	char *msg = strerror(0);
	ASSERT_EQ(strcmp(msg, "Success"), 0, "errno 0 means success");
	return 0;
}

TEST(perror_shows_correct_error) {
	errno = ENOENT;
	char *msg = strerror(errno);
	ASSERT_EQ(strcmp(msg, "No such file or directory"), 0, "ENOENT message");
	return 0;
}

TEST_SUITE(errno) {
	RUN_TEST(open_nonexistent_sets_errno);
	RUN_TEST(read_bad_fd_sets_errno);
	RUN_TEST(write_bad_fd_sets_errno);
	RUN_TEST(strerror_zero_is_success);
	RUN_TEST(perror_shows_correct_error);
}
