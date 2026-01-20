#include "libc.h"

static int readline(char *buf, int max) {
	int n = 0;
	while (n < max - 1) {
		char c;
		if (read(0, &c, 1) > 0) {
			if (c == '\r' || c == '\n') {
				write(1, "\n", 1);
				break;
			} else if (c == 127 || c == '\b') {
				if (n > 0) {
					n--;
					write(1, "\b \b", 3);
				}
			} else if (c >= ' ') {
				buf[n++] = c;
				write(1, &c, 1);
			}
		}
	}
	buf[n] = '\0';
	return n;
}

int main(void) {
	if (fork() == 0) {
		exec("cursor_blink");
		exit(1);
	}

	char buf[64];
	write(1, "slopix> ", 8);

	for (;;) {
		int n = readline(buf, sizeof(buf));
		if (n > 0) {
			if (fork() == 0) {
				exec(buf);
				write(1, "not found\n", 10);
				exit(1);
			}
			wait();
		}
		write(1, "slopix> ", 8);
	}
}
