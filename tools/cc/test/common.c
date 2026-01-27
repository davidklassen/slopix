int printf(char *fmt, ...);
void exit(int status);

void assert(int expected, int actual, char *code) {
	if (expected == actual) {
		printf("%s => %d\n", code, actual);
	} else {
		printf("FAIL: %s => expected %d but got %d\n", code, expected, actual);
		exit(1);
	}
}
