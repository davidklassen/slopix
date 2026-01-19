#include "libc.h"

int main(void) {
	write(1, "init: calling exec(hello)\n", 26);
	exec("hello");
	write(1, "init: exec failed!\n", 19);
	return 1;
}
