#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static void wc(int fd, const char *name) {
	int lines = 0, words = 0, chars = 0;
	int inword = 0;
	char buf[512];
	int n;

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		for (int i = 0; i < n; i++) {
			chars++;
			if (buf[i] == '\n') {
				lines++;
			}
			if (isspace(buf[i])) {
				inword = 0;
			} else if (!inword) {
				inword = 1;
				words++;
			}
		}
	}
	if (name) {
		printf("%d %d %d %s\n", lines, words, chars, name);
	} else {
		printf("%d %d %d\n", lines, words, chars);
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		wc(0, (void *)0);
	} else {
		for (int i = 1; i < argc; i++) {
			int fd = open(argv[i], O_RDONLY);
			if (fd < 0) {
				printf("wc: cannot open %s\n", argv[i]);
				continue;
			}
			wc(fd, argv[i]);
			close(fd);
		}
	}
	exit(0);
}
