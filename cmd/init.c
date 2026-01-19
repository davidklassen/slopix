#include "libc.h"

int main(void) {
	exec("cursor_blink");
	write(1, "init: exec failed!\n", 19);
	return 1;
}
