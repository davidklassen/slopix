#include "libc.h"

int main(void) {
	write(1, "Hello from userspace!\n", 22);
	return 0;
}
