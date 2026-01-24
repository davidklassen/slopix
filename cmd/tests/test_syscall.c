#include <test.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/procinfo.h>
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

TEST(getppid_returns_parent) {
	int parent_pid = getpid();
	int child_pid = fork();
	if (child_pid == 0) {
		int ppid = getppid();
		exit(ppid == parent_pid ? 0 : 1);
	}
	int status = wait();
	ASSERT_EQ(status, 0, "child sees correct ppid");
	return 0;
}

TEST(kill_nonexistent) {
	ASSERT_EQ(kill(9999, SIGTERM), -1, "kill nonexistent returns -1");
	return 0;
}

TEST(kill_zero_invalid) {
	ASSERT_EQ(kill(0, SIGTERM), -1, "kill(0) invalid");
	return 0;
}

TEST(kill_negative_invalid) {
	ASSERT_EQ(kill(-1, SIGTERM), -1, "kill(-1) invalid");
	return 0;
}

TEST(kill_terminates_child) {
	int child_pid = fork();
	if (child_pid == 0) {
		sleep(10000);
		exit(0);
	}
	ASSERT_EQ(kill(child_pid, SIGTERM), 0, "kill returns 0");
	int status = wait();
	ASSERT_EQ(status, -1, "killed child exits -1");
	return 0;
}

TEST(getprocs_returns_count) {
	struct procinfo procs[8];
	int n = getprocs(procs, 8);
	ASSERT(n > 0, "getprocs returns positive");
	return 0;
}

TEST(getprocs_bad_pointer) {
	ASSERT_EQ(getprocs((struct procinfo *)0xDEADBEEF, 8), -1, "bad ptr");
	return 0;
}

TEST(getprocs_zero_max) {
	struct procinfo procs[8];
	ASSERT_EQ(getprocs(procs, 0), -1, "max=0 invalid");
	return 0;
}

TEST(getprocs_negative_max) {
	struct procinfo procs[8];
	ASSERT_EQ(getprocs(procs, -1), -1, "max=-1 invalid");
	return 0;
}

TEST(getprocs_finds_self) {
	struct procinfo procs[8];
	int n = getprocs(procs, 8);
	ASSERT(n > 0, "got procs");
	int mypid = getpid();
	int found = 0;
	for (int i = 0; i < n; i++) {
		if (procs[i].pid == mypid) {
			found = 1;
			ASSERT(procs[i].state > 0, "state valid");
			break;
		}
	}
	ASSERT(found, "found self in list");
	return 0;
}

TEST(getprocs_ppid_valid) {
	struct procinfo procs[8];
	int n = getprocs(procs, 8);
	ASSERT(n > 0, "got procs");
	for (int i = 0; i < n; i++) {
		ASSERT(procs[i].ppid >= 0, "ppid non-negative");
	}
	return 0;
}

TEST(waitpid_returns_status) {
	int child_pid = fork();
	if (child_pid == 0) {
		exit(42);
	}
	int status = waitpid(child_pid);
	ASSERT_EQ(status, 42, "waitpid returns exit status");
	return 0;
}

TEST(waitpid_nonexistent) {
	ASSERT_EQ(waitpid(9999), -1, "waitpid nonexistent returns -1");
	return 0;
}

TEST(waitpid_zero_invalid) {
	ASSERT_EQ(waitpid(0), -1, "waitpid(0) invalid");
	return 0;
}

TEST(waitpid_negative_invalid) {
	ASSERT_EQ(waitpid(-1), -1, "waitpid(-1) invalid");
	return 0;
}

TEST(waitpid_not_child) {
	ASSERT_EQ(waitpid(1), -1, "waitpid for init fails");
	return 0;
}

TEST(waitpid_specific_child) {
	int child1 = fork();
	if (child1 == 0) {
		sleep(5000);
		exit(1);
	}

	int child2 = fork();
	if (child2 == 0) {
		exit(2);
	}

	sleep(10);
	int status = waitpid(child2);
	ASSERT_EQ(status, 2, "got child2 status");

	kill(child1, SIGKILL);
	status = waitpid(child1);
	ASSERT_EQ(status, -1, "got child1 killed status");
	return 0;
}

TEST(kill_with_sigkill) {
	int child_pid = fork();
	if (child_pid == 0) {
		sleep(10000);
		exit(0);
	}
	ASSERT_EQ(kill(child_pid, SIGKILL), 0, "sigkill ok");
	int status = wait();
	ASSERT_EQ(status, -1, "killed by sigkill");
	return 0;
}

TEST(kill_with_sigstop) {
	int child_pid = fork();
	if (child_pid == 0) {
		sleep(10000);
		exit(0);
	}
	sleep(10);
	ASSERT_EQ(kill(child_pid, SIGSTOP), 0, "sigstop ok");
	sleep(10);

	struct procinfo procs[8];
	int n = getprocs(procs, 8);
	int found_stopped = 0;
	for (int i = 0; i < n; i++) {
		if (procs[i].pid == child_pid && procs[i].state == 4) {
			found_stopped = 1;
			break;
		}
	}
	ASSERT(found_stopped, "child is stopped");

	kill(child_pid, SIGKILL);
	wait();
	return 0;
}

TEST(kill_with_sigcont) {
	int child_pid = fork();
	if (child_pid == 0) {
		sleep(10000);
		exit(42);
	}
	sleep(10);

	ASSERT_EQ(kill(child_pid, SIGSTOP), 0, "stop ok");
	sleep(10);

	ASSERT_EQ(kill(child_pid, SIGCONT), 0, "cont ok");
	sleep(10);

	struct procinfo procs[8];
	int n = getprocs(procs, 8);
	int found_running = 0;
	for (int i = 0; i < n; i++) {
		if (procs[i].pid == child_pid && procs[i].state != 4) {
			found_running = 1;
			break;
		}
	}
	ASSERT(found_running, "child resumed");

	kill(child_pid, SIGKILL);
	wait();
	return 0;
}

TEST(signal_zero_exists) {
	int mypid = getpid();
	ASSERT_EQ(kill(mypid, 0), 0, "sig 0 to self ok");
	return 0;
}

TEST(signal_zero_nonexistent) {
	ASSERT_EQ(kill(9999, 0), -1, "sig 0 to nonexistent");
	return 0;
}

TEST(ps_shows_stopped) {
	int child_pid = fork();
	if (child_pid == 0) {
		sleep(10000);
		exit(0);
	}
	sleep(10);
	kill(child_pid, SIGSTOP);
	sleep(10);

	struct procinfo procs[8];
	int n = getprocs(procs, 8);
	int found = 0;
	for (int i = 0; i < n; i++) {
		if (procs[i].pid == child_pid) {
			ASSERT_EQ(procs[i].state, 4, "state is STOPPED");
			found = 1;
			break;
		}
	}
	ASSERT(found, "found stopped child");

	kill(child_pid, SIGKILL);
	wait();
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
	RUN_TEST(getppid_returns_parent);
	RUN_TEST(kill_nonexistent);
	RUN_TEST(kill_zero_invalid);
	RUN_TEST(kill_negative_invalid);
	RUN_TEST(kill_terminates_child);
	RUN_TEST(getprocs_returns_count);
	RUN_TEST(getprocs_bad_pointer);
	RUN_TEST(getprocs_zero_max);
	RUN_TEST(getprocs_negative_max);
	RUN_TEST(getprocs_finds_self);
	RUN_TEST(getprocs_ppid_valid);
	RUN_TEST(waitpid_returns_status);
	RUN_TEST(waitpid_nonexistent);
	RUN_TEST(waitpid_zero_invalid);
	RUN_TEST(waitpid_negative_invalid);
	RUN_TEST(waitpid_not_child);
	RUN_TEST(waitpid_specific_child);
	RUN_TEST(kill_with_sigkill);
	RUN_TEST(kill_with_sigstop);
	RUN_TEST(kill_with_sigcont);
	RUN_TEST(signal_zero_exists);
	RUN_TEST(signal_zero_nonexistent);
	RUN_TEST(ps_shows_stopped);
}
