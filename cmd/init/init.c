#include <stdio.h>
#include <unistd.h>

int main(void) {
	printf("\nWelcome to Slopix!\n");
	printf("To exit QEMU press Ctrl-a x\n\n");

	if (fork() == 0) {
		exec("/cursor_blink");
		exit(1);
	}

	for (;;) {
		int pid = fork();
		if (pid == 0) {
			exec("/shell");
			printf("init: failed to exec /shell\n");
			exit(1);
		}
		wait();
	}
}
