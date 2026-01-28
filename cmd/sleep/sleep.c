#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("usage: sleep <seconds>\n");
		return 1;
	}
	int seconds = atoi(argv[1]);
	if (seconds > 0) {
		sleep(seconds * 1000);
	}
	return 0;
}
