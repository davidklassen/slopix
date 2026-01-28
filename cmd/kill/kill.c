#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static int parse_signal(const char *s) {
	if (s[0] == '-') {
		s++;
	}

	if (s[0] >= '0' && s[0] <= '9') {
		return atoi(s);
	}

	if (strcmp(s, "HUP") == 0) {
		return SIGHUP;
	}
	if (strcmp(s, "INT") == 0) {
		return SIGINT;
	}
	if (strcmp(s, "QUIT") == 0) {
		return SIGQUIT;
	}
	if (strcmp(s, "KILL") == 0) {
		return SIGKILL;
	}
	if (strcmp(s, "TERM") == 0) {
		return SIGTERM;
	}
	if (strcmp(s, "STOP") == 0) {
		return SIGSTOP;
	}
	if (strcmp(s, "CONT") == 0) {
		return SIGCONT;
	}
	if (strcmp(s, "TSTP") == 0) {
		return SIGTSTP;
	}

	return -1;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("usage: kill [-signal] <pid>\n");
		return 1;
	}

	int sig = SIGTERM;
	int pid_arg = 1;

	if (argc >= 3 && argv[1][0] == '-') {
		sig = parse_signal(argv[1]);
		if (sig < 0) {
			printf("kill: invalid signal: %s\n", argv[1]);
			return 1;
		}
		pid_arg = 2;
	}

	int pid = atoi(argv[pid_arg]);
	if (pid <= 0) {
		printf("kill: invalid pid: %s\n", argv[pid_arg]);
		return 1;
	}

	if (kill(pid, sig) < 0) {
		printf("kill: no such process: %d\n", pid);
		return 1;
	}

	return 0;
}
