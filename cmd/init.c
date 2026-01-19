#include "libc.h"

int main(void) {
	int pid = fork();
	if (pid == 0) {
		// Child
		write(1, "child: hello from fork!\n", 24);
		exit(0);
	} else {
		// Parent
		write(1, "parent: waiting for child\n", 26);
		int child = wait();
		write(1, "parent: child exited, pid=", 26);
		char c = '0' + child;
		write(1, &c, 1);
		write(1, "\n", 1);
	}

	// Now exec cursor_blink
	exec("cursor_blink");
	write(1, "init: exec failed!\n", 19);
	return 1;
}
