#include <test.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

TEST(open_file) {
	int fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "open returns valid fd");
	close(fd);
	return 0;
}

TEST(open_nonexistent) {
	int fd = open("/nonexistent", O_RDONLY);
	ASSERT_EQ(fd, -1, "open nonexistent file returns -1");
	return 0;
}

TEST(open_read_file) {
	int fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "open returns valid fd");
	char buf[32];
	int n = read(fd, buf, sizeof(buf));
	ASSERT(n > 0, "read returns positive count");
	close(fd);
	return 0;
}

TEST(fstat_file) {
	int fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "open returns valid fd");
	struct stat st;
	int r = fstat(fd, &st);
	ASSERT_EQ(r, 0, "fstat succeeds");
	ASSERT(st.size > 0, "file has size > 0");
	ASSERT_EQ(st.type, 1, "type is T_FILE (1)");
	close(fd);
	return 0;
}

TEST(dup_file) {
	int fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "open returns valid fd");
	int fd2 = dup(fd);
	ASSERT(fd2 >= 0, "dup returns valid fd");
	ASSERT_NE(fd, fd2, "dup returns different fd");

	char buf[4];
	int n = read(fd2, buf, 4);
	ASSERT(n > 0, "can read via dup'd fd");

	close(fd);
	close(fd2);
	return 0;
}

TEST(close_invalid_fd) {
	ASSERT_EQ(close(-1), -1, "close(-1) returns -1");
	ASSERT_EQ(close(100), -1, "close(100) returns -1");
	return 0;
}

TEST(read_after_close) {
	int fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "open returns valid fd");
	close(fd);
	char buf[8];
	ASSERT_EQ(read(fd, buf, 8), -1, "read closed fd returns -1");
	return 0;
}

TEST_SUITE(filesys) {
	RUN_TEST(open_file);
	RUN_TEST(open_nonexistent);
	RUN_TEST(open_read_file);
	RUN_TEST(fstat_file);
	RUN_TEST(dup_file);
	RUN_TEST(close_invalid_fd);
	RUN_TEST(read_after_close);
}
