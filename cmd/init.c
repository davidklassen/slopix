#include "libc.h"

int main(void) {
	// Test echo with arguments
	int pid = fork();
	if (pid == 0) {
		exec("echo hello world from slopix");
		exit(1);
	}
	wait();

	// Now exec cursor_blink
	exec("cursor_blink");
	write(1, "init: exec failed!\n", 19);
	return 1;
}
