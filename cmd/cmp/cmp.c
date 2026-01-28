#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if (argc != 3) {
		printf("usage: cmp file1 file2\n");
		return 2;
	}

	int fd1 = open(argv[1], O_RDONLY);
	if (fd1 < 0) {
		printf("cmp: %s: cannot open\n", argv[1]);
		return 2;
	}

	int fd2 = open(argv[2], O_RDONLY);
	if (fd2 < 0) {
		printf("cmp: %s: cannot open\n", argv[2]);
		close(fd1);
		return 2;
	}

	long offset = 0;
	long line = 1;
	char c1, c2;

	for (;;) {
		int n1 = read(fd1, &c1, 1);
		int n2 = read(fd2, &c2, 1);

		if (n1 == 0 && n2 == 0) {
			break;
		}

		if (n1 == 0) {
			printf("cmp: EOF on %s after byte %ld\n", argv[1], offset);
			close(fd1);
			close(fd2);
			return 1;
		}

		if (n2 == 0) {
			printf("cmp: EOF on %s after byte %ld\n", argv[2], offset);
			close(fd1);
			close(fd2);
			return 1;
		}

		if (c1 != c2) {
			printf("%s %s differ: byte %ld, line %ld\n", argv[1], argv[2], offset + 1, line);
			close(fd1);
			close(fd2);
			return 1;
		}

		offset++;
		if (c1 == '\n') {
			line++;
		}
	}

	close(fd1);
	close(fd2);
	return 0;
}
