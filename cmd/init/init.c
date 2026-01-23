#include <stdio.h>
#include <unistd.h>

int main(void) {
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
