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
	ASSERT(st.st_size > 0, "file has size > 0");
	ASSERT_EQ(st.st_mode, 1, "type is T_FILE (1)");
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
	ASSERT_EQ(st.st_size, 0, "file truncated to 0");
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
	ASSERT_EQ(st.st_size, 42, "file size is 42");
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
	ASSERT_EQ(st.st_mode, 2, "type is T_DIR");
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

TEST(fork_shares_offset) {
	int fd = open("/large", O_RDONLY);
	ASSERT(fd >= 0, "open returns valid fd");

	char buf[4];
	read(fd, buf, 4);

	int pid = fork();
	if (pid == 0) {
		read(fd, buf, 4);
		close(fd);
		exit(0);
	}
	ASSERT(pid > 0, "fork succeeds");
	wait(NULL);

	long off = lseek(fd, 0, 1);
	ASSERT_EQ(off, 8, "offset advanced by both parent and child reads");

	close(fd);
	return 0;
}

TEST(dup_shares_offset) {
	int fd = open("/large", O_RDONLY);
	ASSERT(fd >= 0, "open returns valid fd");

	int fd2 = dup(fd);
	ASSERT(fd2 >= 0, "dup returns valid fd");

	char buf[4];
	read(fd, buf, 4);
	read(fd2, buf, 4);

	long off1 = lseek(fd, 0, 1);
	long off2 = lseek(fd2, 0, 1);

	ASSERT_EQ(off1, 8, "fd offset is 8");
	ASSERT_EQ(off2, 8, "dup'd fd offset is also 8");
	ASSERT_EQ(off1, off2, "both fds share same offset");

	close(fd);
	close(fd2);
	return 0;
}

TEST(large_file_write) {
	int fd = open("/largefile", O_CREAT | O_WRONLY | O_TRUNC);
	ASSERT(fd >= 0, "open returns valid fd");

	char buf[1024];
	for (int i = 0; i < 1024; i++) {
		buf[i] = (char)(i & 0xff);
	}

	int target_size = 300 * 1024;
	int written = 0;
	while (written < target_size) {
		int n = write(fd, buf, 1024);
		ASSERT(n > 0, "write succeeds");
		written += n;
	}

	struct stat st;
	fstat(fd, &st);
	ASSERT(st.st_size >= target_size, "file size >= 300KB");
	close(fd);
	return 0;
}

TEST(large_file_read) {
	int fd = open("/largefile", O_RDONLY);
	ASSERT(fd >= 0, "open returns valid fd");

	struct stat st;
	fstat(fd, &st);
	ASSERT(st.st_size >= 300 * 1024, "file is large enough");

	char buf[1024];
	int total = 0;
	int n;
	while ((n = read(fd, buf, 1024)) > 0) {
		for (int i = 0; i < n; i++) {
			ASSERT_EQ(buf[i], (char)((total + i) & 0xff), "data correct");
		}
		total += n;
	}

	ASSERT_EQ(total, st.st_size, "read entire file");
	close(fd);
	return 0;
}

TEST(large_file_unlink) {
	ASSERT_EQ(unlink("/largefile"), 0, "unlink succeeds");
	int fd = open("/largefile", O_RDONLY);
	ASSERT_EQ(fd, -1, "file gone after unlink");
	return 0;
}

TEST(ftruncate_shrink) {
	int fd = open("/test_trunc.txt", O_CREAT | O_RDWR);
	write(fd, "hello world", 11);
	ASSERT_EQ(ftruncate(fd, 5), 0, "ftruncate succeeds");
	struct stat st;
	fstat(fd, &st);
	ASSERT_EQ(st.st_size, 5, "size is 5");
	close(fd);
	unlink("/test_trunc.txt");
	return 0;
}

TEST(ftruncate_to_zero) {
	int fd = open("/test_trunc2.txt", O_CREAT | O_RDWR);
	write(fd, "test", 4);
	ASSERT_EQ(ftruncate(fd, 0), 0, "truncate to zero");
	struct stat st;
	fstat(fd, &st);
	ASSERT_EQ(st.st_size, 0, "size is 0");
	close(fd);
	unlink("/test_trunc2.txt");
	return 0;
}

TEST(ftruncate_same_size) {
	int fd = open("/test_trunc3.txt", O_CREAT | O_RDWR);
	write(fd, "hello", 5);
	ASSERT_EQ(ftruncate(fd, 5), 0, "same size ok");
	struct stat st;
	fstat(fd, &st);
	ASSERT_EQ(st.st_size, 5, "size unchanged");
	close(fd);
	unlink("/test_trunc3.txt");
	return 0;
}

TEST(ftruncate_readonly_fails) {
	int fd = open("/test_trunc4.txt", O_CREAT | O_WRONLY);
	write(fd, "test", 4);
	close(fd);
	fd = open("/test_trunc4.txt", O_RDONLY);
	ASSERT_EQ(ftruncate(fd, 2), -1, "readonly fails");
	close(fd);
	unlink("/test_trunc4.txt");
	return 0;
}

TEST(ftruncate_extend_fails) {
	int fd = open("/test_trunc5.txt", O_CREAT | O_RDWR);
	write(fd, "hi", 2);
	ASSERT_EQ(ftruncate(fd, 10), -1, "extend fails");
	close(fd);
	unlink("/test_trunc5.txt");
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
	RUN_TEST(fork_shares_offset);
	RUN_TEST(dup_shares_offset);
	RUN_TEST(large_file_write);
	RUN_TEST(large_file_read);
	RUN_TEST(large_file_unlink);
	RUN_TEST(ftruncate_shrink);
	RUN_TEST(ftruncate_to_zero);
	RUN_TEST(ftruncate_same_size);
	RUN_TEST(ftruncate_readonly_fails);
	RUN_TEST(ftruncate_extend_fails);
}
