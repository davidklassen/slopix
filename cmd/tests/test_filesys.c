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

TEST(write_file) {
	int fd = open("/hello", O_WRONLY);
	ASSERT(fd >= 0, "open for write returns valid fd");
	const char *data = "Test";
	int n = write(fd, data, 4);
	ASSERT_EQ(n, 4, "write returns 4");
	close(fd);
	return 0;
}

TEST(write_read_back) {
	int fd = open("/hello", O_RDWR);
	ASSERT(fd >= 0, "open for rdwr returns valid fd");
	const char *data = "HELLO";
	int n = write(fd, data, 5);
	ASSERT_EQ(n, 5, "write returns 5");
	close(fd);

	fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "reopen for read returns valid fd");
	char buf[5];
	n = read(fd, buf, 5);
	ASSERT_EQ(n, 5, "read returns 5");
	ASSERT(buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'L',
	       "read data matches written data");
	close(fd);
	return 0;
}

TEST(open_trunc) {
	int fd = open("/hello", O_WRONLY | O_TRUNC);
	ASSERT(fd >= 0, "open with O_TRUNC returns valid fd");
	struct stat st;
	fstat(fd, &st);
	ASSERT_EQ(st.size, 0, "file truncated to 0");
	close(fd);
	return 0;
}

TEST(write_extends_file) {
	int fd = open("/hello", O_WRONLY | O_TRUNC);
	ASSERT(fd >= 0, "open with O_TRUNC returns valid fd");
	const char *data = "Extended content for testing file growth.";
	int n = write(fd, data, 42);
	ASSERT_EQ(n, 42, "write returns 42");
	close(fd);

	fd = open("/hello", O_RDONLY);
	struct stat st;
	fstat(fd, &st);
	ASSERT_EQ(st.size, 42, "file size is 42");
	close(fd);
	return 0;
}

TEST(create_file) {
	int fd = open("/newfile", O_CREAT | O_RDWR);
	ASSERT(fd >= 0, "open O_CREAT returns valid fd");
	close(fd);
	fd = open("/newfile", O_RDONLY);
	ASSERT(fd >= 0, "can reopen created file");
	close(fd);
	unlink("/newfile");
	return 0;
}

TEST(mkdir_basic) {
	int r = mkdir("/testdir");
	ASSERT_EQ(r, 0, "mkdir succeeds");
	int fd = open("/testdir", O_RDONLY);
	ASSERT(fd >= 0, "can open directory");
	struct stat st;
	fstat(fd, &st);
	ASSERT_EQ(st.type, 2, "type is T_DIR");
	close(fd);
	unlink("/testdir");
	return 0;
}

TEST(link_unlink) {
	int fd = open("/linktest", O_CREAT | O_RDWR);
	write(fd, "data", 4);
	close(fd);

	ASSERT_EQ(link("/linktest", "/linktest2"), 0, "link succeeds");

	unlink("/linktest");
	fd = open("/linktest2", O_RDONLY);
	ASSERT(fd >= 0, "linked file still accessible");
	close(fd);

	unlink("/linktest2");
	return 0;
}

TEST(chdir_basic) {
	mkdir("/chdirtest");
	ASSERT_EQ(chdir("/chdirtest"), 0, "chdir succeeds");

	int fd = open("localfile", O_CREAT | O_WRONLY);
	ASSERT(fd >= 0, "can create file in new cwd");
	close(fd);

	chdir("/");
	unlink("/chdirtest/localfile");
	unlink("/chdirtest");
	return 0;
}

TEST(unlink_nonexistent) {
	ASSERT_EQ(unlink("/nonexistent"), -1, "unlink nonexistent fails");
	return 0;
}

TEST(mkdir_duplicate) {
	ASSERT_EQ(mkdir("/dupdir"), 0, "first mkdir succeeds");
	ASSERT_EQ(mkdir("/dupdir"), -1, "duplicate mkdir fails");
	unlink("/dupdir");
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
	RUN_TEST(write_file);
	RUN_TEST(write_read_back);
	RUN_TEST(open_trunc);
	RUN_TEST(write_extends_file);
	RUN_TEST(create_file);
	RUN_TEST(mkdir_basic);
	RUN_TEST(link_unlink);
	RUN_TEST(chdir_basic);
	RUN_TEST(unlink_nonexistent);
	RUN_TEST(mkdir_duplicate);
}
