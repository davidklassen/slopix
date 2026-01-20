#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int interval = 1000;
	if (argc > 1) {
		interval = atoi(argv[1]);
		if (interval < 10) {
			interval = 10;
		}
	}

	int n = 0;
	char buf[32];
	for (;;) {
		write(1, "tick ", 5);
		int len = itoa(n++, buf);
		write(1, buf, len);
		write(1, "\n", 1);

		if (poll(0, interval) > 0) {
			char c;
			read(0, &c, 1);
			if (c == 'q') {
				break;
			}
		}
	}
	return 0;
}
