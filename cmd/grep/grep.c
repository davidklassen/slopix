#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAXLINE 1024

static void grep(int fd, const char *pattern, const char *filename) {
	char buf[512];
	char line[MAXLINE];
	int linelen = 0;
	int n;

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		for (int i = 0; i < n; i++) {
			if (buf[i] == '\n' || linelen >= MAXLINE - 1) {
				line[linelen] = '\0';
				if (strstr(line, pattern)) {
					if (filename) {
						printf("%s:%s\n", filename, line);
					} else {
						printf("%s\n", line);
					}
				}
				linelen = 0;
			} else {
				line[linelen++] = buf[i];
			}
		}
	}
	if (linelen > 0) {
		line[linelen] = '\0';
		if (strstr(line, pattern)) {
			if (filename) {
				printf("%s:%s\n", filename, line);
			} else {
				printf("%s\n", line);
			}
		}
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("usage: grep pattern [file...]\n");
		exit(1);
	}
	const char *pattern = argv[1];
	int show_filename = argc > 3;

	if (argc < 3) {
		grep(0, pattern, (void *)0);
	} else {
		for (int i = 2; i < argc; i++) {
			int fd = open(argv[i], O_RDONLY);
			if (fd < 0) {
				printf("grep: cannot open %s\n", argv[i]);
				continue;
			}
			grep(fd, pattern, show_filename ? argv[i] : (void *)0);
			close(fd);
		}
	}
	exit(0);
}
