#include "libc.h"

int main(int argc, char **argv) {
	int interval = 100; // default 1 second (100 ticks)
	if (argc > 1) {
		interval = atoi(argv[1]) / 10; // convert ms to ticks
		if (interval < 1) {
			interval = 1;
		}
	}

	int n = 0;
	char buf[32];
	for (;;) {
		// Check for 'q' (non-blocking)
		char c;
		if (read(0, &c, 1) > 0 && c == 'q') {
			break;
		}

		// Print "tick N\n"
		write(1, "tick ", 5);
		int len = itoa(n++, buf);
		write(1, buf, len);
		write(1, "\n", 1);

		sleep(interval);
	}
	return 0;
}
