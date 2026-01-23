#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("usage: rm file...\n");
		exit(1);
	}
	int status = 0;
	for (int i = 1; i < argc; i++) {
		if (unlink(argv[i]) < 0) {
			printf("rm: cannot remove %s\n", argv[i]);
			status = 1;
		}
	}
	exit(status);
}
