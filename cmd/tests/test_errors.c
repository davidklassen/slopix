#include <test.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/procinfo.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// write errors
TEST(write_ebadf_invalid_fd) {
	errno = 0;
	ASSERT_EQ(write(999, "x", 1), -1, "write fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

TEST(write_ebadf_negative_fd) {
	errno = 0;
	ASSERT_EQ(write(-1, "x", 1), -1, "write fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

TEST(write_efault_bad_pointer) {
	errno = 0;
	ASSERT_EQ(write(1, (char *)0xDEADBEEF, 10), -1, "write fails");
	ASSERT_EQ(errno, EFAULT, "errno is EFAULT");
	return 0;
}

// read errors
TEST(read_ebadf_invalid_fd) {
	errno = 0;
	char buf[8];
	ASSERT_EQ(read(999, buf, 8), -1, "read fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

TEST(read_ebadf_closed_fd) {
	int fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "open ok");
	close(fd);
	errno = 0;
	char buf[8];
	ASSERT_EQ(read(fd, buf, 8), -1, "read fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

TEST(read_efault_bad_pointer) {
	errno = 0;
	ASSERT_EQ(read(0, (char *)0xDEADBEEF, 10), -1, "read fails");
	ASSERT_EQ(errno, EFAULT, "errno is EFAULT");
	return 0;
}

// open errors
TEST(open_enoent) {
	errno = 0;
	ASSERT_EQ(open("/nonexistent", O_RDONLY), -1, "open fails");
	ASSERT_EQ(errno, ENOENT, "errno is ENOENT");
	return 0;
}

TEST(open_eexist_excl) {
	int fd = open("/excl_test", O_CREAT | O_WRONLY);
	ASSERT(fd >= 0, "create file");
	close(fd);
	errno = 0;
	ASSERT_EQ(open("/excl_test", O_CREAT | O_EXCL | O_WRONLY), -1, "open fails");
	ASSERT_EQ(errno, EEXIST, "errno is EEXIST");
	unlink("/excl_test");
	return 0;
}

// close errors
TEST(close_ebadf_invalid) {
	errno = 0;
	ASSERT_EQ(close(999), -1, "close fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

TEST(close_ebadf_negative) {
	errno = 0;
	ASSERT_EQ(close(-1), -1, "close fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

// fstat errors
TEST(fstat_ebadf_invalid) {
	errno = 0;
	struct stat st;
	ASSERT_EQ(fstat(999, &st), -1, "fstat fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

// dup errors
TEST(dup_ebadf_invalid) {
	errno = 0;
	ASSERT_EQ(dup(999), -1, "dup fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

TEST(dup_ebadf_negative) {
	errno = 0;
	ASSERT_EQ(dup(-1), -1, "dup fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

// chdir errors
TEST(chdir_enoent) {
	errno = 0;
	ASSERT_EQ(chdir("/nonexistent_dir"), -1, "chdir fails");
	ASSERT_EQ(errno, ENOENT, "errno is ENOENT");
	return 0;
}

TEST(chdir_enotdir) {
	errno = 0;
	ASSERT_EQ(chdir("/hello"), -1, "chdir to file fails");
	ASSERT_EQ(errno, ENOTDIR, "errno is ENOTDIR");
	return 0;
}

// unlink errors
TEST(unlink_enoent) {
	errno = 0;
	ASSERT_EQ(unlink("/nonexistent"), -1, "unlink fails");
	ASSERT_EQ(errno, ENOENT, "errno is ENOENT");
	return 0;
}

TEST(unlink_einval_dot) {
	mkdir("/dottest", 0755);
	chdir("/dottest");
	errno = 0;
	ASSERT_EQ(unlink("."), -1, "unlink . fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	chdir("/");
	unlink("/dottest");
	return 0;
}

TEST(unlink_einval_dotdot) {
	mkdir("/dotdottest", 0755);
	mkdir("/dotdottest/sub", 0755);
	chdir("/dotdottest/sub");
	errno = 0;
	ASSERT_EQ(unlink(".."), -1, "unlink .. fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	chdir("/");
	unlink("/dotdottest/sub");
	unlink("/dotdottest");
	return 0;
}

TEST(unlink_enotempty) {
	mkdir("/notempty", 0755);
	int fd = open("/notempty/file", O_CREAT | O_WRONLY);
	close(fd);
	errno = 0;
	ASSERT_EQ(unlink("/notempty"), -1, "unlink non-empty fails");
	ASSERT_EQ(errno, ENOTEMPTY, "errno is ENOTEMPTY");
	unlink("/notempty/file");
	unlink("/notempty");
	return 0;
}

// link errors
TEST(link_enoent_source) {
	errno = 0;
	ASSERT_EQ(link("/nonexistent", "/newlink"), -1, "link fails");
	ASSERT_EQ(errno, ENOENT, "errno is ENOENT");
	return 0;
}

TEST(link_eperm_directory) {
	mkdir("/linkdir", 0755);
	errno = 0;
	ASSERT_EQ(link("/linkdir", "/linkdir2"), -1, "link dir fails");
	ASSERT_EQ(errno, EPERM, "errno is EPERM");
	unlink("/linkdir");
	return 0;
}

// stat errors
TEST(stat_enoent) {
	errno = 0;
	struct stat st;
	ASSERT_EQ(stat("/nonexistent", &st), -1, "stat fails");
	ASSERT_EQ(errno, ENOENT, "errno is ENOENT");
	return 0;
}

// lseek errors
TEST(lseek_ebadf_invalid) {
	errno = 0;
	ASSERT_EQ(lseek(999, 0, 0), -1, "lseek fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

TEST(lseek_einval_whence) {
	int fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "open ok");
	errno = 0;
	ASSERT_EQ(lseek(fd, 0, 99), -1, "lseek bad whence fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	close(fd);
	return 0;
}

TEST(lseek_einval_negative_result) {
	int fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "open ok");
	errno = 0;
	ASSERT_EQ(lseek(fd, -100, 0), -1, "lseek negative offset fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	close(fd);
	return 0;
}

// getcwd errors
TEST(getcwd_einval_small_buffer) {
	chdir("/");
	errno = 0;
	char buf[1];
	ASSERT_EQ((long)getcwd(buf, 1), -1, "getcwd small buffer fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	return 0;
}

// rename errors
TEST(rename_enoent_source) {
	errno = 0;
	ASSERT_EQ(rename("/nonexistent", "/newname"), -1, "rename fails");
	ASSERT_EQ(errno, ENOENT, "errno is ENOENT");
	return 0;
}

// mmap errors
TEST(mmap_einval_zero_length) {
	errno = 0;
	void *p = mmap(0, 0, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	ASSERT_EQ(p, MAP_FAILED, "mmap fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	return 0;
}

TEST(mmap_einval_bad_flags) {
	errno = 0;
	void *p = mmap(0, 4096, PROT_READ, 0, -1, 0);
	ASSERT_EQ(p, MAP_FAILED, "mmap fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	return 0;
}

TEST(mmap_einval_unaligned_fixed) {
	errno = 0;
	void *p = mmap((void *)0x1001, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	ASSERT_EQ(p, MAP_FAILED, "mmap fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	return 0;
}

// munmap errors
TEST(munmap_einval_unaligned) {
	errno = 0;
	ASSERT_EQ(munmap((void *)0x1001, 4096), -1, "munmap fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	return 0;
}

TEST(munmap_einval_zero_length) {
	errno = 0;
	ASSERT_EQ(munmap((void *)0x1000, 0), -1, "munmap fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	return 0;
}

// poll errors
TEST(poll_ebadf_invalid) {
	errno = 0;
	ASSERT_EQ(poll(999, 0), -1, "poll fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

TEST(poll_enotty_regular_file) {
	int fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "open ok");
	errno = 0;
	ASSERT_EQ(poll(fd, 0), -1, "poll on file fails");
	ASSERT_EQ(errno, ENOTTY, "errno is ENOTTY");
	close(fd);
	return 0;
}

// ftruncate errors
TEST(ftruncate_ebadf_invalid) {
	errno = 0;
	ASSERT_EQ(ftruncate(999, 0), -1, "ftruncate fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

TEST(ftruncate_einval_negative) {
	int fd = open("/ftrunc_test", O_CREAT | O_RDWR);
	ASSERT(fd >= 0, "open ok");
	errno = 0;
	ASSERT_EQ(ftruncate(fd, -1), -1, "ftruncate fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	close(fd);
	unlink("/ftrunc_test");
	return 0;
}

TEST(ftruncate_eacces_readonly) {
	int fd = open("/ftrunc_ro", O_CREAT | O_WRONLY);
	write(fd, "test", 4);
	close(fd);
	fd = open("/ftrunc_ro", O_RDONLY);
	ASSERT(fd >= 0, "open ok");
	errno = 0;
	ASSERT_EQ(ftruncate(fd, 0), -1, "ftruncate fails");
	ASSERT_EQ(errno, EACCES, "errno is EACCES");
	close(fd);
	unlink("/ftrunc_ro");
	return 0;
}

// getdents errors (via opendir/readdir)
TEST(getdents_enotdir) {
	int fd = open("/hello", O_RDONLY);
	ASSERT(fd >= 0, "open file ok");
	errno = 0;
	char buf[256];
	extern long getdents(int fd, char *buf, unsigned int count);
	ASSERT_EQ(getdents(fd, buf, 256), -1, "getdents on file fails");
	ASSERT_EQ(errno, ENOTDIR, "errno is ENOTDIR");
	close(fd);
	return 0;
}

TEST(getdents_ebadf) {
	errno = 0;
	char buf[256];
	extern long getdents(int fd, char *buf, unsigned int count);
	ASSERT_EQ(getdents(999, buf, 256), -1, "getdents fails");
	ASSERT_EQ(errno, EBADF, "errno is EBADF");
	return 0;
}

// tcsetpgrp errors
TEST(tcsetpgrp_einval_negative) {
	errno = 0;
	ASSERT_EQ(tcsetpgrp(0, -1), -1, "tcsetpgrp fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	return 0;
}

// wait errors
TEST(wait_esrch_no_children) {
	errno = 0;
	ASSERT_EQ(wait(NULL), -1, "wait fails");
	ASSERT_EQ(errno, ESRCH, "errno is ESRCH");
	return 0;
}

// exec errors
TEST(exec_enoent) {
	int pid = fork();
	if (pid == 0) {
		errno = 0;
		exec("/nonexistent_binary");
		exit(errno == ENOENT ? 0 : 1);
	}
	int status;
	wait(&status);
	ASSERT(WIFEXITED(status), "child exited");
	ASSERT_EQ(WEXITSTATUS(status), 0, "exec set ENOENT");
	return 0;
}

TEST(exec_einval_empty) {
	int pid = fork();
	if (pid == 0) {
		errno = 0;
		exec("");
		exit(errno == EINVAL ? 0 : 1);
	}
	int status;
	wait(&status);
	ASSERT(WIFEXITED(status), "child exited");
	ASSERT_EQ(WEXITSTATUS(status), 0, "exec set EINVAL");
	return 0;
}

TEST(exec_eacces_directory) {
	int pid = fork();
	if (pid == 0) {
		errno = 0;
		exec("/dev");
		exit(errno == EACCES ? 0 : 1);
	}
	int status;
	wait(&status);
	ASSERT(WIFEXITED(status), "child exited");
	ASSERT_EQ(WEXITSTATUS(status), 0, "exec set EACCES");
	return 0;
}

// mkdir errors
TEST(mkdir_eexist) {
	mkdir("/mkdir_exist", 0755);
	errno = 0;
	ASSERT_EQ(mkdir("/mkdir_exist", 0755), -1, "mkdir fails");
	ASSERT_EQ(errno, EEXIST, "errno is EEXIST");
	unlink("/mkdir_exist");
	return 0;
}

// pipe errors
TEST(pipe_efault) {
	errno = 0;
	ASSERT_EQ(pipe((int *)0xDEADBEEF), -1, "pipe fails");
	ASSERT_EQ(errno, EFAULT, "errno is EFAULT");
	return 0;
}

// kill errors
TEST(kill_einval_zero) {
	errno = 0;
	ASSERT_EQ(kill(0, 0), -1, "kill(0) fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	return 0;
}

TEST(kill_esrch_nonexistent) {
	errno = 0;
	ASSERT_EQ(kill(9999, 0), -1, "kill nonexistent fails");
	ASSERT_EQ(errno, ESRCH, "errno is ESRCH");
	return 0;
}

// getprocs errors
TEST(getprocs_einval_zero) {
	errno = 0;
	struct procinfo buf[1];
	ASSERT_EQ(getprocs(buf, 0), -1, "getprocs fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	return 0;
}

TEST(getprocs_efault) {
	errno = 0;
	ASSERT_EQ(getprocs((struct procinfo *)0xDEADBEEF, 8), -1, "getprocs fails");
	ASSERT_EQ(errno, EFAULT, "errno is EFAULT");
	return 0;
}

// waitpid errors
TEST(waitpid_esrch_nonexistent) {
	errno = 0;
	ASSERT_EQ(waitpid(9999, NULL, 0), -1, "waitpid fails");
	ASSERT_EQ(errno, ESRCH, "errno is ESRCH");
	return 0;
}

TEST(waitpid_esrch_no_children) {
	errno = 0;
	ASSERT_EQ(waitpid(-1, NULL, 0), -1, "waitpid fails");
	ASSERT_EQ(errno, ESRCH, "errno is ESRCH");
	return 0;
}

TEST_SUITE(errors) {
	// write
	RUN_TEST(write_ebadf_invalid_fd);
	RUN_TEST(write_ebadf_negative_fd);
	RUN_TEST(write_efault_bad_pointer);
	// read
	RUN_TEST(read_ebadf_invalid_fd);
	RUN_TEST(read_ebadf_closed_fd);
	RUN_TEST(read_efault_bad_pointer);
	// open
	RUN_TEST(open_enoent);
	RUN_TEST(open_eexist_excl);
	// close
	RUN_TEST(close_ebadf_invalid);
	RUN_TEST(close_ebadf_negative);
	// fstat
	RUN_TEST(fstat_ebadf_invalid);
	// dup
	RUN_TEST(dup_ebadf_invalid);
	RUN_TEST(dup_ebadf_negative);
	// chdir
	RUN_TEST(chdir_enoent);
	RUN_TEST(chdir_enotdir);
	// unlink
	RUN_TEST(unlink_enoent);
	RUN_TEST(unlink_einval_dot);
	RUN_TEST(unlink_einval_dotdot);
	RUN_TEST(unlink_enotempty);
	// link
	RUN_TEST(link_enoent_source);
	RUN_TEST(link_eperm_directory);
	// stat
	RUN_TEST(stat_enoent);
	// lseek
	RUN_TEST(lseek_ebadf_invalid);
	RUN_TEST(lseek_einval_whence);
	RUN_TEST(lseek_einval_negative_result);
	// getcwd
	RUN_TEST(getcwd_einval_small_buffer);
	// rename
	RUN_TEST(rename_enoent_source);
	// mmap
	RUN_TEST(mmap_einval_zero_length);
	RUN_TEST(mmap_einval_bad_flags);
	RUN_TEST(mmap_einval_unaligned_fixed);
	// munmap
	RUN_TEST(munmap_einval_unaligned);
	RUN_TEST(munmap_einval_zero_length);
	// poll
	RUN_TEST(poll_ebadf_invalid);
	RUN_TEST(poll_enotty_regular_file);
	// ftruncate
	RUN_TEST(ftruncate_ebadf_invalid);
	RUN_TEST(ftruncate_einval_negative);
	RUN_TEST(ftruncate_eacces_readonly);
	// getdents
	RUN_TEST(getdents_enotdir);
	RUN_TEST(getdents_ebadf);
	// tcsetpgrp
	RUN_TEST(tcsetpgrp_einval_negative);
	// wait
	RUN_TEST(wait_esrch_no_children);
	// exec
	RUN_TEST(exec_enoent);
	RUN_TEST(exec_einval_empty);
	RUN_TEST(exec_eacces_directory);
	// mkdir
	RUN_TEST(mkdir_eexist);
	// pipe
	RUN_TEST(pipe_efault);
	// kill
	RUN_TEST(kill_einval_zero);
	RUN_TEST(kill_esrch_nonexistent);
	// getprocs
	RUN_TEST(getprocs_einval_zero);
	RUN_TEST(getprocs_efault);
	// waitpid
	RUN_TEST(waitpid_esrch_nonexistent);
	RUN_TEST(waitpid_esrch_no_children);
}
