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
		int shell_pid = fork();
		if (shell_pid == 0) {
			exec("/shell");
			printf("init: failed to exec /shell\n");
			exit(1);
		}
		waitpid(shell_pid);
	}
}
