#include <stdio.h>
#include <unistd.h>

int main(void) {
	printf("\nWelcome to Slopix!\n");
	printf("To exit QEMU press Ctrl-a x\n\n");

	if (fork() == 0) {
		exec("/cursor_blink");
		exit(1);
	}

	int shell_pid = -1;
	for (;;) {
		if (shell_pid < 0) {
			shell_pid = fork();
			if (shell_pid == 0) {
				exec("/shell");
				printf("init: failed to exec /shell\n");
				exit(1);
			}
		}
		int pid = wait();
		if (pid == shell_pid) {
			shell_pid = -1;
		}
	}
}
