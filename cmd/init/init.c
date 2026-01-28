#include <stdio.h>
#include <unistd.h>

int main(void) {
	printf("\nWelcome to Slopix!\n");
	printf("To exit QEMU press Ctrl-a x\n\n");

	if (fork() == 0) {
		exec("/bin/cursor_blink");
		exit(1);
	}

	int shell_pid = -1;
	for (;;) {
		if (shell_pid < 0) {
			shell_pid = fork();
			if (shell_pid == 0) {
				exec("/bin/shell");
				printf("init: failed to exec /bin/shell\n");
				exit(1);
			}
		}
		int pid = wait(NULL);
		if (pid == shell_pid) {
			shell_pid = -1;
		}
	}
}
