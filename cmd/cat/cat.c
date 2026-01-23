#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static void cat(int fd) {
	char buf[512];
	int n;
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		write(1, buf, n);
	}
}

int main(int argc, char **argv) {
	if (argc <= 1) {
		cat(0);
		exit(0);
	}

	for (int i = 1; i < argc; i++) {
		int fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			printf("cat: cannot open %s\n", argv[i]);
			exit(1);
		}
		cat(fd);
		close(fd);
	}
	exit(0);
}
