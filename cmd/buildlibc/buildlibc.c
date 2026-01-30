#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *c_srcs[] = {
    "ctype",
    "errno",
    "libgen",
    "malloc",
    "stdio",
    "stdio_file",
    "stdlib",
    "string",
    "test",
    "time",
    NULL};

static const char *asm_srcs[] = {
    "crt0",
    "syscall",
    NULL};

static int run(const char *cmd) {
	printf("  %s\n", cmd);
	fflush(stdout);
	int pid = fork();
	if (pid == 0) {
		setpgid(0, 0);
		exec(cmd);
		exit(127);
	}
	setpgid(pid, pid);
	int status;
	wait(&status);
	if (WIFSIGNALED(status)) {
		printf("error: killed by signal %d\n", WTERMSIG(status));
		return -1;
	}
	if (WEXITSTATUS(status) != 0) {
		printf("error: exited with status %d\n", WEXITSTATUS(status));
		return -1;
	}
	return 0;
}

int main(int argc, char **argv) {
	char cmd[256];
	const char *out = (argc > 1) ? argv[1] : "/lib/libc.a";
	const char *cc = (argc > 2) ? argv[2] : "/bin/cc";

	printf("building %s (using %s)...\n", out, cc);

	for (int i = 0; c_srcs[i]; i++) {
		snprintf(cmd, sizeof(cmd), "%s -I/src/libc -S -o /tmp/%s.s /src/libc/%s.c", cc, c_srcs[i], c_srcs[i]);
		if (run(cmd) < 0) {
			return 1;
		}

		snprintf(cmd, sizeof(cmd), "/bin/as -o /tmp/%s.o /tmp/%s.s", c_srcs[i], c_srcs[i]);
		if (run(cmd) < 0) {
			return 1;
		}
	}

	for (int i = 0; asm_srcs[i]; i++) {
		snprintf(cmd, sizeof(cmd), "/bin/as -o /tmp/%s.o /src/libc/%s.S", asm_srcs[i], asm_srcs[i]);
		if (run(cmd) < 0) {
			return 1;
		}
	}

	snprintf(cmd, sizeof(cmd), "/bin/ar rcs %s "
				   "/tmp/crt0.o /tmp/syscall.o "
				   "/tmp/stdio.o /tmp/stdio_file.o /tmp/string.o "
				   "/tmp/ctype.o /tmp/test.o /tmp/malloc.o "
				   "/tmp/stdlib.o /tmp/errno.o /tmp/libgen.o /tmp/time.o",
		 out);
	if (run(cmd) < 0) {
		return 1;
	}

	printf("done: %s\n", out);
	return 0;
}
