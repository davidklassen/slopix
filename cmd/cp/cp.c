#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if (argc != 3) {
		printf("usage: cp src dst\n");
		exit(1);
	}
	int src = open(argv[1], O_RDONLY);
	if (src < 0) {
		printf("cp: cannot open %s\n", argv[1]);
		exit(1);
	}
	int dst = open(argv[2], O_CREAT | O_WRONLY | O_TRUNC);
	if (dst < 0) {
		printf("cp: cannot create %s\n", argv[2]);
		close(src);
		exit(1);
	}
	char buf[512];
	int n;
	while ((n = read(src, buf, sizeof(buf))) > 0) {
		if (write(dst, buf, n) != n) {
			printf("cp: write error\n");
			close(src);
			close(dst);
			exit(1);
		}
	}
	close(src);
	close(dst);
	exit(0);
}
