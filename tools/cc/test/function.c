#include "test.h"

int ret3(void) {
	return 3;
}

int add2(int x, int y) {
	return x + y;
}

int sub2(int x, int y) {
	return x - y;
}

int add6(int a, int b, int c, int d, int e, int f) {
	return a + b + c + d + e + f;
}

int add8(int a, int b, int c, int d, int e, int f, int g, int h) {
	return a + b + c + d + e + f + g + h;
}

int fib(int x) {
	if (x <= 1) {
		return 1;
	}
	return fib(x - 1) + fib(x - 2);
}

int main(void) {
	ASSERT(3, ret3());
	ASSERT(8, add2(3, 5));
	ASSERT(2, sub2(5, 3));
	ASSERT(21, add6(1, 2, 3, 4, 5, 6));
	ASSERT(36, add8(1, 2, 3, 4, 5, 6, 7, 8));
	ASSERT(55, fib(9));

	printf("OK\n");
	poweroff();
	return 0;
}
