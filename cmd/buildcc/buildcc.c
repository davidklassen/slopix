#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *srcs[] = {
    "main",
    "tokenize",
    "preprocess",
    "parse",
    "type",
    "codegen",
    "unicode",
    "strings",
    "hashmap",
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
	const char *out = (argc > 1) ? argv[1] : "/tmp/cc.elf";
	const char *cc = (argc > 2) ? argv[2] : "/bin/cc";

	printf("building %s (using %s)...\n", out, cc);

	for (int i = 0; srcs[i]; i++) {
		snprintf(cmd, sizeof(cmd), "%s -I/src/libc -S -o /tmp/%s.s /src/cc/%s.c", cc, srcs[i], srcs[i]);
		if (run(cmd) < 0) {
			return 1;
		}

		snprintf(cmd, sizeof(cmd), "/bin/as -o /tmp/%s.o /tmp/%s.s", srcs[i], srcs[i]);
		if (run(cmd) < 0) {
			return 1;
		}
	}

	snprintf(cmd, sizeof(cmd), "/bin/ld -o %s "
				   "/tmp/main.o /tmp/tokenize.o /tmp/preprocess.o "
				   "/tmp/parse.o /tmp/type.o /tmp/codegen.o "
				   "/tmp/unicode.o /tmp/strings.o /tmp/hashmap.o "
				   "/lib/libc.a",
		 out);
	if (run(cmd) < 0) {
		return 1;
	}

	printf("done: %s\n", out);
	return 0;
}
