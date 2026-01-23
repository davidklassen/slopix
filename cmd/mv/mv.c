#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if (argc != 3) {
		printf("usage: mv src dst\n");
		exit(1);
	}
	if (rename(argv[1], argv[2]) < 0) {
		printf("mv: cannot rename %s to %s\n", argv[1], argv[2]);
		exit(1);
	}
	exit(0);
}
