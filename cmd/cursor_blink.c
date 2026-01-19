#include "libc.h"

int main(void) {
	for (;;) {
		write(1, "\x1b[?25h", 6);
		sleep(500);
		write(1, "\x1b[?25l", 6);
		sleep(500);
	}
}
