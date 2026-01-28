#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void head(int fd, int nlines) {
	char buf[512];
	int n;
	int count = 0;

	while ((n = read(fd, buf, sizeof(buf))) > 0 && count < nlines) {
		for (int i = 0; i < n && count < nlines; i++) {
			write(1, &buf[i], 1);
			if (buf[i] == '\n') {
				count++;
			}
		}
	}
}

int main(int argc, char **argv) {
	int nlines = 10;
	int argstart = 1;

	if (argc > 2 && strcmp(argv[1], "-n") == 0) {
		nlines = atoi(argv[2]);
		argstart = 3;
	}

	if (argstart >= argc) {
		head(0, nlines);
	} else {
		for (int i = argstart; i < argc; i++) {
			int fd = open(argv[i], O_RDONLY);
			if (fd < 0) {
				printf("head: cannot open %s\n", argv[i]);
				continue;
			}
			head(fd, nlines);
			close(fd);
		}
	}
	exit(0);
}
