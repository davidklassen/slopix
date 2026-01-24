#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("usage: kill <pid>\n");
		return 1;
	}

	int pid = atoi(argv[1]);
	if (pid <= 0) {
		printf("kill: invalid pid: %s\n", argv[1]);
		return 1;
	}

	if (kill(pid) < 0) {
		printf("kill: no such process: %d\n", pid);
		return 1;
	}

	return 0;
}
