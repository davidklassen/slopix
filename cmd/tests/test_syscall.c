#include <test.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

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

TEST(stat_file) {
	struct stat st;
	ASSERT_EQ(stat("/hello", &st), 0, "stat hello");
	ASSERT_EQ(st.type, 1, "hello is file");
	return 0;
}

TEST(stat_nonexistent) {
	struct stat st;
	ASSERT_EQ(stat("/nonexistent", &st), -1, "stat fails");
	return 0;
}

TEST(getcwd_root) {
	chdir("/");
	char buf[64];
	long len = (long)getcwd(buf, sizeof(buf));
	ASSERT(len > 0, "getcwd returns length");
	ASSERT_EQ(strcmp(buf, "/"), 0, "path is /");
	return 0;
}

TEST(lseek_basic) {
	int fd = open("/lseektest", O_CREAT | O_RDWR);
	ASSERT(fd >= 0, "create file");
	write(fd, "hello", 5);
	ASSERT_EQ(lseek(fd, 0, 0), 0, "seek to start");
	ASSERT_EQ(lseek(fd, 0, 2), 5, "seek to end");
	ASSERT_EQ(lseek(fd, -2, 1), 3, "seek relative");
	close(fd);
	unlink("/lseektest");
	return 0;
}

TEST(rename_file) {
	int fd = open("/oldname", O_CREAT | O_WRONLY);
	write(fd, "data", 4);
	close(fd);
	ASSERT_EQ(rename("/oldname", "/newname"), 0, "rename ok");
	struct stat st;
	ASSERT_EQ(stat("/oldname", &st), -1, "old gone");
	ASSERT_EQ(stat("/newname", &st), 0, "new exists");
	unlink("/newname");
	return 0;
}

TEST(rename_dir_basic) {
	ASSERT_EQ(mkdir("/olddir"), 0, "mkdir olddir");
	int fd = open("/olddir/file", O_CREAT | O_WRONLY);
	write(fd, "test", 4);
	close(fd);
	ASSERT_EQ(rename("/olddir", "/newdir"), 0, "rename dir");
	struct stat st;
	ASSERT_EQ(stat("/olddir", &st), -1, "old dir gone");
	ASSERT_EQ(stat("/newdir", &st), 0, "new dir exists");
	ASSERT_EQ(st.type, 2, "is directory");
	ASSERT_EQ(stat("/newdir/file", &st), 0, "file inside moved");
	unlink("/newdir/file");
	unlink("/newdir");
	return 0;
}

TEST(rename_dir_dotdot) {
	ASSERT_EQ(mkdir("/parent"), 0, "mkdir parent");
	ASSERT_EQ(mkdir("/parent/child"), 0, "mkdir child");
	ASSERT_EQ(mkdir("/other"), 0, "mkdir other");

	// Move /parent/child to /other/moved
	ASSERT_EQ(rename("/parent/child", "/other/moved"), 0, "rename across");

	// Verify ".." inside moved dir points to new parent
	chdir("/other/moved");
	char buf[64];
	chdir("..");
	getcwd(buf, sizeof(buf));
	ASSERT_EQ(strcmp(buf, "/other"), 0, "dotdot updated");

	chdir("/");
	unlink("/other/moved");
	unlink("/other");
	unlink("/parent");
	return 0;
}

TEST(rename_dir_cycle) {
	ASSERT_EQ(mkdir("/a"), 0, "mkdir a");
	ASSERT_EQ(mkdir("/a/b"), 0, "mkdir b");

	// Try to move /a into /a/b - should fail (cycle)
	ASSERT_EQ(rename("/a", "/a/b/a"), -1, "cycle rejected");

	// Cleanup
	unlink("/a/b");
	unlink("/a");
	return 0;
}

TEST(open_append) {
	int fd = open("/appendtest", O_CREAT | O_WRONLY);
	ASSERT(fd >= 0, "create file");
	write(fd, "hello", 5);
	close(fd);

	fd = open("/appendtest", O_WRONLY | O_APPEND);
	ASSERT(fd >= 0, "open append");
	write(fd, "world", 5);
	close(fd);

	fd = open("/appendtest", O_RDONLY);
	char buf[16];
	int n = read(fd, buf, 16);
	ASSERT_EQ(n, 10, "read both writes");
	buf[n] = '\0';
	ASSERT_EQ(strcmp(buf, "helloworld"), 0, "content appended");
	close(fd);
	unlink("/appendtest");
	return 0;
}

TEST(exec_from_disk) {
	int pid = fork();
	if (pid == 0) {
		exec("/true");
		exit(1);
	}
	int status = wait();
	ASSERT_EQ(status, 0, "exec /true from disk");
	return 0;
}

TEST_SUITE(syscalls) {
	RUN_TEST(write_returns_count);
	RUN_TEST(read_poll);
	RUN_TEST(getpid_positive);
	RUN_TEST(bad_write_pointer);
	RUN_TEST(bad_read_pointer);
	RUN_TEST(stat_file);
	RUN_TEST(stat_nonexistent);
	RUN_TEST(getcwd_root);
	RUN_TEST(lseek_basic);
	RUN_TEST(rename_file);
	RUN_TEST(rename_dir_basic);
	RUN_TEST(rename_dir_dotdot);
	RUN_TEST(rename_dir_cycle);
	RUN_TEST(open_append);
	RUN_TEST(exec_from_disk);
}
